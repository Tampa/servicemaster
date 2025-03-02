#include "sm_err.h"
#include "display.h"
#include "bus.h"
#include <ncurses.h>

char *program_name;

/**
 * Displays a welcome message with basic usage and security information.
 */
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
    (void)argc;
    program_name = argv[0];

    if (geteuid())
        display_set_bus_type(USER);
    else
        display_set_bus_type(SYSTEM);

    bus_init();
    display_init();
    show_welcome_message();
    display_redraw(bus_currently_displayed());

    wait_input();

    // Restore terminal state before exit
    endwin();
    echo();
    curs_set(1);

    return 0;
}
