#include <ctype.h>
#include <ncurses.h>
#include <errno.h>
#include <systemd/sd-event.h>
#include <signal.h>
#include "sm_err.h"
#include "service.h"
#include "display.h"
#include <sys/ioctl.h>

static uint64_t start_time = 0;
static enum service_type mode = SERVICE;
static enum bus_type type = SYSTEM;
static int index_start = 0;
static int position = 0;
static uid_t euid = INT32_MAX;
static sd_event *event = NULL;
static sd_event_source *event_source = NULL;

extern const char **service_str_types;

// Forward declaration of display_service_row
static void display_service_row(Service *svc, int row, int spc);

/**
 * Handles the display of a service row with all its details.
 *
 * @param svc The service to display
 * @param row The row number (relative position)
 * @param spc The spacing/offset from the top
 */
static void display_service_row(Service *svc, int row, int spc)
{
    int i;
    char short_unit[D_XLOAD - 2];
    char short_unit_file_state[10];
    char *short_description;
    size_t maxx_description = getmaxx(stdscr) - D_XDESCRIPTION - 1;

    // Clear the unit name column
    for (i = 1; i < D_XLOAD - 1; i++)
        mvaddch(row + spc, i, ' ');

    // If the unit name is too long, truncate it and add ...
    if (strlen(svc->unit) >= D_XLOAD - 3)
    {
        strncpy(short_unit, svc->unit, D_XLOAD - 2);
        mvaddstr(row + spc, 1, short_unit);
        mvaddstr(row + spc, D_XLOAD - 4, "...");
    }
    else
        mvaddstr(row + spc, 1, svc->unit);

    // Clear the state column
    for (i = D_XLOAD; i < D_XACTIVE - 1; i++)
        mvaddch(row + spc, i, ' ');

    // If the state is too long, truncate it (enabled-runtime will be enabled-r)
    if (!svc->unit_file_state || strlen(svc->unit_file_state) == 0)
        mvprintw(row + spc, D_XLOAD, "%s", svc->load);
    else if (strlen(svc->unit_file_state) > 9)
    {
        strncpy(short_unit_file_state, svc->unit_file_state, 9);
        short_unit_file_state[9] = '\0';
        mvaddstr(row + spc, D_XLOAD, short_unit_file_state);
    }
    else
        mvprintw(row + spc, D_XLOAD, "%s", svc->unit_file_state ? svc->unit_file_state : svc->load);

    // Clear the active column
    for (i = D_XACTIVE; i < D_XSUB - 1; i++)
        mvaddch(row + spc, i, ' ');

    mvprintw(row + spc, D_XACTIVE, "%s", svc->active);

    // Clear the sub column
    for (i = D_XSUB; i < D_XDESCRIPTION - 1; i++)
        mvaddch(row + spc, i, ' ');

    mvprintw(row + spc, D_XSUB, "%s", svc->sub);

    // Clear the description column
    for (i = D_XDESCRIPTION; i < getmaxx(stdscr) - 1; i++)
        mvaddch(row + spc, i, ' ');

    // If the description is too long, truncate it and add ...
    if (strlen(svc->description) >= maxx_description)
    {
        short_description = alloca(maxx_description + 1);
        memset(short_description, 0, maxx_description + 1);
        strncpy(short_description, svc->description, maxx_description - 3);
        mvaddstr(row + spc, D_XDESCRIPTION, short_description);
        mvaddstr(row + spc, D_XDESCRIPTION + maxx_description - 3, "...");
    }
    else
        mvaddstr(row + spc, D_XDESCRIPTION, svc->description);

    // Save the y-position of the service
    svc->ypos = row + spc;
}

/**
 * Displays the list of services on the screen.
 *
 * This function is responsible for rendering the list of services,
 * handling pagination, and applying highlighting to the selected service.
 * It also manages the display of services based on the current mode and
 * available screen space.
 *
 * @param bus Pointer to the Bus structure containing service information.
 */
static void display_services(Bus *bus)
{
    int max_rows, maxy;
    int row = 0;
    int idx = index_start;
    Service *svc;
    int headerrow = 3;
    struct winsize size;
    int visible_services = 0;
    int total_services = 0;
    int dummy_maxx;

    getmaxyx(stdscr, maxy, dummy_maxx);
    (void)dummy_maxx;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    if (size.ws_col < (strlen(D_FUNCTIONS) + strlen(D_SERVICE_TYPES) + 2))
    {
        headerrow = 4;
    }

    int spc = headerrow + 2;
    max_rows = maxy - spc - 1;

    // Zähle zuerst die Services im aktuellen Modus
    for (int i = 0;; i++)
    {
        svc = service_nth(bus, i);
        if (!svc)
            break;
        if (mode == ALL || mode == svc->type)
            total_services++;
    }

    services_invalidate_ypos(bus);

    while (true)
    {
        svc = service_nth(bus, idx);
        if (!svc)
            break;

        if (row >= max_rows)
            break;

        if (mode != ALL && mode != svc->type)
        {
            idx++;
            continue;
        }

        if (row == position)
        {
            attron(COLOR_PAIR(8));
            attron(A_BOLD);
        }

        display_service_row(svc, row, spc);

        if (row == position)
        {
            attroff(COLOR_PAIR(8));
            attroff(A_BOLD);
        }

        row++;
        idx++;
        visible_services++;
    }
}

/**
 * Prints the text and lines for the main user interface.
 * This function is responsible for rendering the header, function keys, and mode indicators
 * on the screen. It also updates the position and mode information based on the current state.
 */
static void display_text_and_lines(Bus *bus)
{
    int x = D_XLOAD / 2 - 10;
    int maxx, maxy;
    char tmptype[16] = {0};
    int headerrow = 3;

    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    if (size.ws_col < (strlen(D_FUNCTIONS) + strlen(D_SERVICE_TYPES) + 2))
    {
        headerrow++;
    }

    getmaxyx(stdscr, maxy, maxx);

    attroff(COLOR_PAIR(9));
    border(0, 0, 0, 0, 0, 0, 0, 0);

    attron(A_BOLD);
    attron(COLOR_PAIR(0));
    mvaddstr(1, 1, D_HEADLINE);
    mvaddstr(1, strlen(D_HEADLINE) + 1 + ((size.ws_col - strlen(D_HEADLINE) - strlen(D_QUIT) - strlen(D_NAVIGATION) - 2) / 2), D_NAVIGATION);
    mvaddstr(1, size.ws_col - strlen(D_QUIT) - 1, D_QUIT);

    attron(COLOR_PAIR(9));
    mvaddstr(2, 1, D_FUNCTIONS);
    attroff(COLOR_PAIR(9));

    attron(COLOR_PAIR(10));
    if (size.ws_col < (strlen(D_FUNCTIONS) + strlen(D_SERVICE_TYPES) + 2))
    {
        mvaddstr(3, 1, D_SERVICE_TYPES);
    }
    else
    {
        mvaddstr(2, size.ws_col - strlen(D_SERVICE_TYPES) - 1, D_SERVICE_TYPES);
    }
    attroff(COLOR_PAIR(10));

    mvprintw(headerrow, D_XLOAD - 10, "Pos.:%3d", position + index_start);
    mvprintw(headerrow, 1, "UNIT:");

    attron(COLOR_PAIR(4));
    mvprintw(headerrow, 7, "(%s)", type ? "USER" : "SYSTEM");
    attroff(COLOR_PAIR(4));

    mvprintw(headerrow, D_XLOAD, "STATE:");
    mvprintw(headerrow, D_XACTIVE, "ACTIVE:");
    mvprintw(headerrow, D_XSUB, "SUB:");
    mvprintw(headerrow, D_XDESCRIPTION, "DESCRIPTION:");

    attron(COLOR_PAIR(4));
    attron(A_UNDERLINE);

    // Sets the type count
    strncpy(tmptype, service_string_type(mode), 16);
    tmptype[sizeof(tmptype) - 1] = '\0';
    tmptype[0] = toupper(tmptype[0]);
    mvprintw(headerrow, x, "%s: %d", tmptype, bus->total_types[mode]);

    attroff(COLOR_PAIR(4));
    attroff(A_UNDERLINE);
    attroff(A_BOLD);
    mvhline(headerrow + 1, 1, ACS_HLINE, maxx - 2);
    mvvline(headerrow, D_XLOAD - 1, ACS_VLINE, maxy - 3);
    mvvline(headerrow, D_XACTIVE - 1, ACS_VLINE, maxy - 3);
    mvvline(headerrow, D_XSUB - 1, ACS_VLINE, maxy - 3);
    mvvline(headerrow, D_XDESCRIPTION - 1, ACS_VLINE, maxy - 3);
}

/**
 * Handles user input and performs various operations on systemd services.
 * This function is responsible for:
 * - Handling user input from the keyboard, including navigation, service operations, and mode changes
 * - Updating the display based on the current state and user actions
 * - Calling appropriate functions to perform service operations (start, stop, restart, etc.)
 * - Reloading the service list when necessary
 *
 * @param s The event source that triggered the callback.
 * @param fd The file descriptor associated with the event source.
 * @param revents The events that occurred on the file descriptor.
 * @param data Arbitrary user data passed to the callback.
 * @return 0 to indicate the event was handled successfully.
 */
int display_key_pressed(sd_event_source *s, int fd, uint32_t revents, void *data)
{
    (void)fd;
    int c;
    char *status = NULL;
    int max_services = 0;
    int maxy;
    int maxx;

    // Mouse-Reset
    printf("\033[?1003l\n");
    mousemask(0, NULL);

    getmaxyx(stdscr, maxy, maxx);

    int headerrow = 3;
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    if (size.ws_col < (strlen(D_FUNCTIONS) + strlen(D_SERVICE_TYPES) + 2))
    {
        headerrow = 4;
    }
    int spc = headerrow + 2;               // +2 for the separator line and a space
    int max_visible_rows = maxy - spc - 1; // Exact calculation of visible rows
    int page_scroll = max_visible_rows;    // For Page Up/Down
    bool update_state = false;
    Service *svc = NULL;
    Bus *bus = (Bus *)data;

    if ((revents & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) > 0)
        return 0;

    c = getch();

    // Count the total number of services of the current type
    for (int i = 0;; i++)
    {
        svc = service_nth(bus, i);
        if (!svc)
            break;
        if (mode == ALL || mode == svc->type)
            max_services++;
    }

    svc = service_nth(bus, position + index_start);
    set_escdelay(25);

    switch (c)
    {
    case 'f': // Search functionality
    {
        // Variables for search window dimensions and positioning
        char search_query[256] = {0};
        int win_height = 3, win_width = 80;
        int starty = (maxy - win_height) / 2;
        int startx = (maxx - win_width) / 2;
        Service *found_service = NULL;
        static bool search_in_progress = false; // Prevents multiple concurrent searches

        // Prevent nested searches
        if (search_in_progress)
            break;

        search_in_progress = true;

        // Create and display search input window
        WINDOW *input_win = newwin(win_height, win_width, starty, startx);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 1, "Enter search query: ");
        wrefresh(input_win);

        // Enable text input and show cursor
        echo();
        curs_set(1);
        wgetnstr(input_win, search_query, sizeof(search_query) - 1);
        noecho();
        curs_set(0);

        // Clean up input window
        delwin(input_win);
        refresh();
        flushinp();

        // Clear any remaining input
        while (getch() != ERR)
            ;

        // Exit search if query is empty
        if (strlen(search_query) == 0)
        {
            search_in_progress = false;
            break;
        }

        // Store current mode and switch to ALL to search across all services
        enum service_type current_mode = mode;
        mode = ALL;

        // Search for service matching query
        for (int i = 0;; i++)
        {
            Service *svc_iter = service_nth(bus, i);
            if (!svc_iter)
                break;

            if (strcasestr(svc_iter->unit, search_query) != NULL)
            {
                found_service = svc_iter;
                break;
            }
        }

        if (found_service)
        {
            // Switch to found service's type
            mode = found_service->type;
            int filtered_pos = 0;

            // Calculate position of found service in filtered list
            for (int i = 0;; i++)
            {
                Service *svc = service_nth(bus, i);
                if (!svc)
                    break;

                if (svc->type == mode)
                {
                    if (svc == found_service)
                    {
                        // Adjust scroll position to show found service
                        if (filtered_pos >= max_visible_rows)
                        {
                            index_start = filtered_pos - max_visible_rows + 1;
                            position = max_visible_rows - 1;
                        }
                        else
                        {
                            index_start = 0;
                            position = filtered_pos;
                        }
                        break;
                    }
                    filtered_pos++;
                }
            }

            // Redraw display with found service
            clear();
            display_services(bus);
            display_text_and_lines(bus);
            refresh();
        }
        else
        {
            // Restore previous mode if no service found
            mode = current_mode;
            display_status_window("No matching service found.", "Search");
        }

        // Reset search state and update start time
        search_in_progress = false;
        start_time = service_now();
        return 0;
    }
    case KEY_ESC:
        nodelay(stdscr, TRUE); // Non-blocking mode
        int esc_timeout = 50;  // 50ms Timeout für Escape-Sequenzen
        wtimeout(stdscr, esc_timeout);
        // Buffer to store escape sequence characters
        char seq[10] = {0};
        int i = 0, c;

        // Read escape sequence characters until ERR or '~' is encountered
        while ((c = getch()) != ERR && i < 9)
        {
            seq[i++] = c;
            if (c == '~')
                break;
        }
        seq[i] = '\0';

        // Handle different escape sequences for function keys, Some terminals send escape sequences for function keys
        if (strcmp(seq, "[11~") == 0)
        {
            d_op(bus, svc, START, "Start");
        }
        else if (strcmp(seq, "[12~") == 0)
        {
            d_op(bus, svc, STOP, "Stop");
        }
        else if (strcmp(seq, "[13~") == 0)
        {
            d_op(bus, svc, RESTART, "Restart");
        }
        else if (strcmp(seq, "[14~") == 0)
        {
            d_op(bus, svc, ENABLE, "Enable");
            update_state = true;
        }
        else
        {
            // Exit if ESC was pressed and enough time has passed since start
            if ((service_now() - start_time) < D_ESCOFF_MS)
                break;
            endwin();
            exit(EXIT_SUCCESS);
        }
        nodelay(stdscr, FALSE); // Zurück zum normalen Modus
        break;
    case KEY_F(1):
        d_op(bus, svc, START, "Start");
        break;
    case KEY_F(2):
        d_op(bus, svc, STOP, "Stop");
        break;
    case KEY_F(3):
        d_op(bus, svc, RESTART, "Restart");
        break;
    case KEY_F(4):
        d_op(bus, svc, ENABLE, "Enable");
        update_state = true;
        break;
    case KEY_F(5):
        d_op(bus, svc, DISABLE, "Disable");
        update_state = true;
        break;
    case KEY_F(6):
        d_op(bus, svc, MASK, "Mask");
        update_state = true;
        break;
    case KEY_F(7):
        d_op(bus, svc, UNMASK, "Unmask");
        update_state = true;
        break;
    case KEY_F(8):
        d_op(bus, svc, RELOAD, "Reload");
        break;
    case KEY_UP:
        if (position > 0)
        {
            // If position > 0, just move the cursor up
            position--;
        }
        else if (index_start > 0)
        {
            // If we're at the top edge and there are more entries above, scroll up
            index_start--;
        }
        break;

    case KEY_DOWN:
        if (position < max_visible_rows - 1 && position + index_start < max_services - 1)
        {
            // If we're not at the bottom edge and there are more entries, move the cursor
            position++;
        }
        else if (position + index_start < max_services - 1)
        {
            // If we're at the bottom edge and there are more entries, scroll down
            index_start++;
        }
        break;

    case KEY_PPAGE: // Page Up
        if (index_start > 0)
        {
            // Scroll one page up
            index_start -= page_scroll;
            if (index_start < 0)
                index_start = 0;
            erase();
        }
        position = 0;
        break;

    case KEY_NPAGE: // Page Down
        if (index_start + max_visible_rows < max_services)
        {
            // Scroll one page down
            index_start += page_scroll;
            if (index_start + max_visible_rows > max_services)
                index_start = max_services - max_visible_rows;
            if (index_start < 0)
                index_start = 0;
            erase();
        }
        position = 0;
        break;

    case KEY_LEFT:
        if (mode > ALL)
            D_MODE(mode - 1);
        break;

    case KEY_RIGHT:
        if (mode < SNAPSHOT)
            D_MODE(mode + 1);
        break;

    case KEY_SPACE:
        if (bus_system_only())
            break;

        type ^= 0x1;
        bus = bus_currently_displayed();
        sd_event_source_set_userdata(s, bus);
        erase();
        break;

    case KEY_RETURN:
        svc = service_nth(bus, position + index_start);
        if (!svc)
            break;
        status = service_status_info(bus, svc);

        display_status_window(status ? status : "No status information available.", "Status:");
        free(status);
        break;

    case 'a':
        D_MODE(ALL);
        break;

    case 'd':
        D_MODE(DEVICE);
        break;

    case 'i':
        D_MODE(SLICE);
        break;

    case 's':
        D_MODE(SERVICE);
        break;

    case 'o':
        D_MODE(SOCKET);
        break;

    case 't':
        D_MODE(TARGET);
        break;

    case 'r':
        D_MODE(TIMER);
        break;

    case 'm':
        D_MODE(MOUNT);
        break;

    case 'c':
        D_MODE(SCOPE);
        break;

    case 'n':
        D_MODE(AUTOMOUNT);
        break;

    case 'w':
        D_MODE(SWAP);
        break;

    case 'p':
        D_MODE(PATH);
        break;

    case 'h':
        D_MODE(SNAPSHOT);
        break;

    case 'q':
        endwin();
        exit(EXIT_SUCCESS);
        break;

    default:
        break;
    }

    if (update_state)
    {
        if (svc != NULL)
        { // <-- Schutz vor NULL-Zeigern
            bus_update_unit_file_state(bus, svc);
            display_redraw_row(svc);
            svc->changed = 0;
        }
        update_state = false; // Zurücksetzen nach Verarbeitung
    }

    // Make sure we are not going over the end of the list
    if (index_start + position >= max_services)
    {
        if (max_services > 0)
        {
            if (max_services > max_visible_rows)
            {
                index_start = max_services - max_visible_rows;
                position = max_visible_rows - 1;
            }
            else
            {
                index_start = 0;
                position = max_services - 1;
            }
        }
        else
        {
            index_start = 0;
            position = 0;
        }
    }

    // Full redraw of the screen
    erase();
    display_redraw(bus);
    refresh();

    return 0;
}

/**
 * Returns the current bus type.
 *
 * This function provides access to the static 'type' variable,
 * which represents the current bus type (SYSTEM or USER).
 *
 * @return The current bus type.
 */
enum bus_type display_bus_type(void)
{
    return type;
}

/**
 * Returns the current service type mode.
 *
 * This function provides access to the static 'mode' variable,
 * which represents the current service type filter (ALL, DEVICE, SERVICE, etc.).
 *
 * @return The current service type mode.
 */
enum service_type display_mode(void)
{
    return mode;
}

/**
 * Redraws the entire display with the current bus information.
 *
 * This function performs a complete redraw of the screen by:
 * 1. Displaying all services from the provided bus
 * 2. Clearing any remaining space below the services list
 * 3. Drawing the text headers and separator lines
 * 4. Refreshing the screen to show the changes
 *
 * @param bus Pointer to the Bus structure containing services to display
 */
void display_redraw(Bus *bus)
{
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    // Erstelle den Headline-Text mit root-Markierung
    char headline[100];
    snprintf(headline, sizeof(headline), "%s%s%s",
             D_HEADLINE,
             (geteuid() == 0) ? " " : "",
             (geteuid() == 0) ? "(root)" : "");

    mvaddstr(1, 1, headline);
    if (geteuid() == 0)
    {
        // Position direkt nach dem bereits geschriebenen Text
        int root_pos = 1 + strlen(D_HEADLINE) + 1;
        move(1, root_pos);
        attron(COLOR_PAIR(3) | A_BOLD); // Rot und fett
        printw("(root)");
        attroff(COLOR_PAIR(3) | A_BOLD);
    }

    display_services(bus);
    clrtobot();
    display_text_and_lines(bus);
    refresh();
}

/**
 * Refreshes the display row for the given service.
 *
 * If the service is currently displayed on the screen, this function will
 * clear the row for the service and redraw it to ensure the display is
 * up-to-date.
 *
 * @param svc The service to refresh the display row for.
 */
void display_redraw_row(Service *svc)
{
    // If the service is on the screen, invalidate the row so it refreshes
    // correctly
    int x, y;

    if (svc->ypos < 0)
        return;

    getyx(stdscr, y, x);
    wmove(stdscr, svc->ypos, D_XLOAD);
    wclrtoeol(stdscr);
    wmove(stdscr, y, x);
}

void display_erase(void)
{
    erase();
}

/**
 * Sets the current bus type for the display.
 *
 * This function updates the static 'type' variable that determines
 * whether system or user services are being displayed.
 *
 * @param ty The bus type to set (SYSTEM or USER)
 */
void display_set_bus_type(enum bus_type ty)
{
    type = ty;
}

/**
 * Signal handler for SIGWINCH (window change) events.
 *
 * This function is called when the terminal window is resized. It:
 * 1. Gets the new terminal dimensions
 * 2. Resizes the screen buffer to match
 * 3. Redraws the entire display
 *
 * @param sig Signal number (unused)
 */
static void handle_winch(int sig)
{
    (void)sig;
    Bus *current_bus = bus_currently_displayed();
    struct winsize size;

    // Get the current terminal size
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    // Terminal size has changed, resize the screen
    resizeterm(size.ws_row, size.ws_col);

    // Redraw the screen
    clear();
    display_redraw(current_bus);
    refresh();
}

/**
 * Initializes the display and event handling system.
 *
 * This function:
 * 1. Sets up SIGWINCH handler for terminal window resizing
 * 2. Initializes systemd event loop and IO event handling
 * 3. Sets up ncurses display settings
 * 4. Configures color pairs for the UI
 *
 * The function handles:
 * - Window resize events through SIGWINCH
 * - Keyboard input through epoll events
 * - Basic terminal display settings
 * - Color definitions for various UI elements
 *
 * Error conditions are handled by setting error messages through sm_err_set()
 * and returning early if critical initialization fails.
 */
void display_init(void)
{
    int rc;
    Bus *bus = bus_currently_displayed();

    // Signal-Handler for SIGWINCH
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGWINCH, &sa, NULL) == -1)
    {
        sm_err_set("Cannot setup window change handler: %s\n", strerror(errno));
        return;
    }

    // initialize event loop
    rc = sd_event_default(&event);
    if (rc < 0)
    {
        sm_err_set("Cannot initialize event loop: %s\n", strerror(-rc));
        return;
    }

    // initialize event handler
    rc = sd_event_add_io(event,
                         &event_source,
                         STDIN_FILENO,
                         EPOLLIN,
                         display_key_pressed,
                         bus);
    if (rc < 0)
    {
        sm_err_set("Cannot initialize event handler: %s\n", strerror(-rc));
        return;
    }

    // activate event handler
    rc = sd_event_source_set_enabled(event_source, SD_EVENT_ON);
    if (rc < 0)
    {
        sm_err_set("Cannot enable event source: %s\n", strerror(-rc));
        return;
    }

    euid = geteuid();
    start_time = service_now();

    // initialize ncurses
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    set_escdelay(0);
    start_color();

    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    printf("\033[?1003h\n"); // Erweiterte Maus-Events aktivieren

    // initialize colors
    init_pair(0, COLOR_BLACK, COLOR_WHITE);
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);
    init_pair(6, COLOR_BLUE, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(8, COLOR_WHITE, COLOR_BLUE);
    init_pair(9, COLOR_WHITE, COLOR_RED);
    init_pair(10, COLOR_BLACK, COLOR_GREEN);
    init_pair(11, COLOR_RED, COLOR_YELLOW);
    init_pair(12, COLOR_RED, COLOR_BLUE);

    clear();
    border(0, 0, 0, 0, 0, 0, 0, 0);
}

/**
 * Displays a status window with the provided status message and title.
 *
 * This function creates a centered window on the screen with the given status
 * message and title. The window is displayed until the user presses a key.
 *
 * @param status The status message to display in the window.
 * @param title The title to display at the top of the window.
 */
void display_status_window(const char *status, const char *title)
{
    char status_cpy[strlen(status) + 1];
    strcpy(status_cpy, status);
    int maxx_row = 0, maxy = 0, maxx = 0;
    int current_row_length = 0;
    int rows = 0, height = 0, width = 0;
    int startx = 0, starty = 0;
    WINDOW *win = NULL;
    int text_starty, y, x;
    int line_length = 0;
    const char *line_start = NULL;
    const char *line_end = NULL;

    strcpy(status_cpy, status);

    for (int count = 0; status_cpy[count] != '\0'; count++)
    {
        if (status_cpy[count] == '\n')
        {
            rows++;
            if (current_row_length > maxx_row)
                maxx_row = current_row_length;
            current_row_length = 0;
        }
        else
            current_row_length++;
    }

    if (current_row_length > maxx_row)
        maxx_row = current_row_length;

    getmaxyx(stdscr, maxy, maxx);

    if (rows >= maxy)
        height = maxy + 2;
    else
        height = rows + 2;
    if (rows == 0)
        height = 3;

    if (maxx_row >= maxx)
        width = maxx;
    else
        width = maxx_row + 4;

    starty = (maxy - height) / 2;
    startx = (maxx - width) / 2;

    win = newwin(height, width, starty, startx);
    box(win, 0, 0);
    keypad(win, TRUE);
    start_color();
    init_pair(13, COLOR_RED, COLOR_BLACK);

    text_starty = 1;
    y = text_starty;
    x = 1;

    wattron(win, A_BOLD);
    wattron(win, A_UNDERLINE);

    mvwprintw(win, 0, (width / 2) - (strlen(title) / 2), "%s", title);
    wattroff(win, A_UNDERLINE);

    if (rows == 0)
        wattron(win, COLOR_PAIR(13));

    line_start = status_cpy;
    while ((line_end = strchr(line_start, '\n')) != NULL)
    {
        line_length = line_end - line_start;
        if (line_length > width - 2)
            line_length = width - 6;

        mvwaddnstr(win, y++, x, line_start, line_length);
        line_start = line_end + 1;
    }

    mvwprintw(win, y, x, "%s", line_start);
    wrefresh(win);
    wgetch(win);

    wattroff(win, COLOR_PAIR(13));
    wattroff(win, A_BOLD);

    delwin(win);
    refresh();
}

void d_op(Bus *bus, Service *svc, enum operation mode, const char *txt)
{
    (void)svc;
    bool success = false;

    if (bus->type == SYSTEM && euid != 0)
    {
        WINDOW *win = newwin(6, 60, LINES / 2 - 3, COLS / 2 - 30);
        box(win, 0, 0);

        // Aktiviere rote Farbe
        wattron(win, COLOR_PAIR(3)); // Rot auf Schwarz
        wattron(win, A_BOLD);

        mvwprintw(win, 0, 2, "Info:");
        mvwprintw(win, 2, 2, "You must be root for this operation on system units.");
        mvwprintw(win, 3, 2, "Would you like to restart with sudo? (y/n)");

        // Deaktiviere Attribute
        wattroff(win, A_BOLD);
        wattroff(win, COLOR_PAIR(3));

        wrefresh(win);

        // Eingabemodus vorbereiten
        flushinp();
        nodelay(stdscr, FALSE);

        int c = wgetch(win);

        delwin(win);
        touchwin(stdscr);
        refresh();

        if (c == 'y' || c == 'Y')
        {
            endwin();
            system("reset");

            char *args[] = {"sudo", program_name, NULL};
            execvp("sudo", args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }

        nodelay(stdscr, TRUE);
        return;
    }

    Service *temp_svc = service_nth(bus, position + index_start);
    if (!temp_svc)
    {
        display_status_window("No valid service selected.", "Error:");
        return;
    }

    success = bus_operation(bus, temp_svc, mode);
    if (!success)
    {
        display_status_window("Command could not be executed on this unit.", txt);
    }
}
