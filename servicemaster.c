#include "sm_err.h"
#include "display.h"
#include "bus.h"
#include <ncurses.h>
#include <getopt.h>

char *program_name;
bool show_welcome;

// List of colorschemes
const char *colors[] = {"default",
                        "nord",
                        "solarizeddark",
                        "dracula",
                        "monokai",
                        "gruvboxdark",
                        "onedark",
                        "monochrome",
                        "solarizedlight"};

// Help message
const char *help = "\nUsage: " D_HEADLINE " [options]\n\n"
                   "Options:\n"
                   "  -v  Display the version information and exit\n"
                   "  -w  Do not show the welcome message\n"
                   "  -h  Display this help message and exit\n"
                   "  -c  Set the colorscheme\n"
                   "  -l  List all available colorschemes\n\n"
                   "After launching ServiceMaster, you can use the following controls:\n"
                   "- Arrow keys, page up/down: Navigate through the list of units.\n"
                   "- Space: Toggle between system and user units.\n"
                   "- Enter: Show detailed status of the selected unit.\n"
                   "- F1-F8: Perform actions (start, stop, restart, etc.) on the selected unit.\n"
                   "- a-z: Quick filter units by type.\n"
                   "- q or ESC: Quit the application.\n"
                   "- +,-: Switch between colorschemes.\n"
                   "- f: Search for units by name.\n\n"
                   "                2025 Lennart Martens\n\n"
                   "License: MIT\n"
                   "For bug reports, feature requests, or general inquiries:\n"
                   "https://github.com/lennart1978/servicemaster\n\n";

/**
 * Displays a welcome message with basic usage and security information.
 
static void show_welcome_message()
{
    const char *welcome_text =
        "Welcome to ServiceMaster!\n\n"
        "This tool allows you to manage systemd units through an intuitive interface.\n\n"
        "SECURITY GUIDELINE:\n"
        "- Only root can manage system services\n"
        "- Regular users can only manage their own user services\n\n"
        "Press any key to continue...";

    display_status_window(welcome_text, "ServiceMaster " D_VERSION);
}
*/

static void list_colorschemes()
{
    wprintf(L"ServiceMaster " D_VERSION "\n\n");
    wprintf(L"Available colorschemes:\n");
    wprintf(L"-----------------------\n\n");

    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
    {
        wprintf(L"%s\n", colors[i]);
    }
    wprintf(L"\n");
}

/**
 * Handles user input and performs various operations on systemd services.
 * This function is responsible for:
 * - Handling user input from the keyboard, including navigation, service operations, and mode changes
 * - Updating the display based on the current state and user actions
 * - Calling appropriate functions to perform service operations (start, stop, restart, etc.)
 * - Reloading the service list when necessary
 */
void wait_input()
{
    int rc;
    sd_event *ev = NULL;

    rc = sd_event_default(&ev);
    if (rc < 0)
        sm_err_set("Cannot fetch default event handler: %s\n", strerror(-rc));

    rc = sd_event_loop(ev);
    if (rc < 0)
        sm_err_set("Cannot run even loop: %s\n", strerror(-rc));
    sd_event_unref(ev);
    return;
}

/**
 * The main entry point of the application.
 * This function initializes the screen, retrieves all systemd services,
 * filters them, and then enters a loop to wait for user input.
 * The function returns 0 on successful exit, or -1 on error.
 */
int main(int argc, char *argv[])
{
    // Initialize program name and welcome message flag
    program_name = argv[0];
    int option;
    show_welcome = true;

    // Parse command line options
    while ((option = getopt(argc, argv, "vwhc:l")) != -1)
    {
        switch (option)
        {
        case 'v':
            wprintf(L"Version: " D_VERSION "\n");
            return EXIT_SUCCESS;
            break;

        case 'w':
            show_welcome = false;
            break;

        case 'h':
            wprintf(L"%s", help);
            return EXIT_SUCCESS;
            break;

        case 'c':
            if (strcmp(optarg, "nord") == 0)
            {
                colorscheme = NORD;
            }
            else if (strcmp(optarg, "solarizeddark") == 0)
            {
                colorscheme = SOLARIZEDDARK;
            }
            else if (strcmp(optarg, "dracula") == 0)
            {
                colorscheme = DRACULA;
            }
            else if (strcmp(optarg, "monokai") == 0)
            {
                colorscheme = MONOKAI;
            }
            else if (strcmp(optarg, "gruvboxdark") == 0)
            {
                colorscheme = GRUVBOXDARK;
            }
            else if (strcmp(optarg, "onedark") == 0)
            {
                colorscheme = ONEDARK;
            }
            else if (strcmp(optarg, "monochrome") == 0)
            {
                colorscheme = MONOCHROME;
            }
            else if (strcmp(optarg, "solarizedlight") == 0)
            {
                colorscheme = SOLARIZEDLIGHT;
            }
            else if (strcmp(optarg, "default") == 0)
            {
                colorscheme = DEFAULT;
            }
            else
            {
                wprintf(L"Unknown colorscheme: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;

        case 'l':
            list_colorschemes();
            return EXIT_SUCCESS;
            break;

        default:
            wprintf(L"Wrong arguments: Type -h for help\n");
            return EXIT_FAILURE;
        }
    }

    // Set bus type based on user or root
    if (geteuid())
        display_set_bus_type(USER);
    else
        display_set_bus_type(SYSTEM);

    // Initialize bus and display
    bus_init();
    display_init();

    display_redraw(bus_currently_displayed());

    wait_input();

    // Restore terminal state before exit
    endwin();
    echo();
    curs_set(1);

    return 0;
}
