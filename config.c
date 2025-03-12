#include "config.h"
#include <signal.h>

// External function to reset the terminal window title
extern void reset_terminal_title(void);

// Global variables for color schemes
char *actual_scheme = NULL;
ColorScheme *color_schemes = NULL;
int scheme_count = 0;

// Array of color names for validation
const char *color_names[NUM_COLORS] = {
    "black", "white", "green", "yellow",
    "red", "magenta", "cyan", "blue"};

/**
 * Signal handler for cleaning up resources before program termination.
 *
 * This function is called when a termination signal is received. It performs
 * necessary cleanup by:
 * 1. Freeing color scheme resources
 * 2. Freeing the current color scheme name
 * 3. Printing a cleanup message
 * 4. Exiting the program with the received signal number
 *
 * @param signum The signal number that triggered the handler
 */
void cleanup_handler(int signum)
{
    free_color_schemes();
    free(actual_scheme);

    // Reset terminal settings
    reset_terminal_title();
    endwin();
    reset_shell_mode();
    fflush(stdout);
    fflush(stderr);
    fflush(stdin);
    // Print Signal message
    printf("\nSignal: %s !\nAll memory freed. Exiting...\n\n", strsignal(signum));
    curs_set(1);
    // Exit with the signal number
    exit(signum);
}

/**
 * Configures signal handlers for graceful program termination.
 *
 * Sets up signal handlers for common termination and error signals (SIGINT, SIGTERM,
 * SIGABRT, SIGSEGV ...) to invoke the cleanup_handler, ensuring proper resource cleanup
 * before program exit.
 */
void setup_signal_handlers()
{
    struct sigaction sa;
    sa.sa_handler = cleanup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Setup signal handlers
    sigaction(SIGINT, &sa, NULL);  // Interrupt signal from keyboard
    sigaction(SIGTERM, &sa, NULL); // Terminate signal from kill(1)
    sigaction(SIGABRT, &sa, NULL); // Abort signal from abort(3)
    sigaction(SIGSEGV, &sa, NULL); // Segmentation fault
    sigaction(SIGHUP, &sa, NULL);  // Hangup detected
    sigaction(SIGQUIT, &sa, NULL); // Quit signal (Ctrl+\)
    sigaction(SIGILL, &sa, NULL);  // Illegal instruction
    sigaction(SIGFPE, &sa, NULL);  // Floating point exception
    sigaction(SIGBUS, &sa, NULL);  // Bus error (bad memory access)
    sigaction(SIGPIPE, &sa, NULL); // Broken pipe
    sigaction(SIGSYS, &sa, NULL);  // Bad system call
}
// Function to print a (TOML) file
void print_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Error opening file");
        return;
    }
    size_t l;
    fseek(fp, 0, SEEK_END);
    l = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("\nFile: %s | %ldKB\n", filename, l / 1024);
    printf("--------------------------------------------------\n\n");

    char *line = NULL;
    while (getline(&line, &l, fp) != -1)
    {
        printf("%s", line);
    }
    free(line);

    fclose(fp);
}

/**
 * Frees all allocated memory for color schemes.
 *
 * This function:
 * 1. Frees each scheme's name string
 * 2. Frees the color_schemes array
 * 3. Resets the global color scheme variables
 */
void free_color_schemes()
{
    for (int i = 0; i < scheme_count; i++)
    {
        free(color_schemes[i].name);
    }
    free(color_schemes);
    color_schemes = NULL;
    scheme_count = 0;
}

/**
 * Parses an RGB color array from a TOML configuration.
 *
 * @param arr The TOML array containing RGB values
 * @param rgb Pointer to an integer array where the RGB values will be stored
 * @param color_name Name of the color being parsed (for error messages)
 * @param scheme_name Name of the color scheme being parsed (for error messages)
 * @return 1 on success, 0 on failure
 *
 * The function validates that:
 * - The array exists
 * - The array contains exactly 3 elements (R,G,B)
 * - Each value is a valid integer between 0-255
 *
 * Error conditions are reported to stderr with context about which color
 * and scheme caused the error.
 */
int parse_rgb_array(toml_array_t *arr, int *rgb, const char *color_name, const char *scheme_name)
{
    if (!arr)
    {
        fprintf(stderr, "Missing '%s' in scheme '%s'\n", color_name, scheme_name);
        return 0;
    }

    if (toml_array_nelem(arr) != 3)
    {
        fprintf(stderr, "Invalid RGB array length for '%s' in scheme '%s'\n", color_name, scheme_name);
        return 0;
    }

    for (int i = 0; i < 3; i++)
    {
        toml_raw_t raw = toml_raw_at(arr, i);
        if (!raw)
        {
            fprintf(stderr, "Error reading RGB[%d] for '%s' in scheme '%s'\n", i, color_name, scheme_name);
            return 0;
        }

        int64_t value;
        if (toml_rtoi(raw, &value) != 0)
        {
            fprintf(stderr, "Failed to parse integer for '%s' in scheme '%s'\n", color_name, scheme_name);
            return 0;
        }

        if (value < 0 || value > 255)
        {
            fprintf(stderr, "Invalid RGB value %ld (0-255 allowed) for '%s' in scheme '%s'\n", value, color_name, scheme_name);
            return 0;
        }

        rgb[i] = (int)value;
    }
    return 1;
}

/**
 * Parses a color scheme from a TOML table configuration.
 *
 * @param table TOML table containing the color scheme configuration
 * @return 1 on success, 0 on failure
 *
 * The function expects the following structure in the TOML table:
 * - A "name" field containing the scheme name
 * - RGB array entries for each color: black, white, green, yellow, red, magenta, cyan, blue
 *   Each RGB array must contain exactly 3 integers (0-255)
 *
 * The parsed scheme is added to the global color_schemes array.
 * Memory is allocated for both the scheme name and the color_schemes array.
 *
 * Error conditions:
 * - Missing or invalid "name" field
 * - Missing or invalid RGB arrays for any color
 * - Memory allocation failures
 */
int parse_color_scheme(toml_table_t *table)
{
    ColorScheme scheme = {0};

    // Parse name
    toml_raw_t name_raw = toml_raw_in(table, "name");
    if (!name_raw)
    {
        fprintf(stderr, "Missing 'name' in scheme\n");
        return 0;
    }

    char *name;
    if (toml_rtos(name_raw, &name) != 0)
    {
        fprintf(stderr, "Failed to parse scheme name\n");
        return 0;
    }
    scheme.name = strdup(name);

    // Parse colors
    for (int i = 0; i < NUM_COLORS; i++)
    {
        toml_array_t *color_arr = toml_array_in(table, color_names[i]);
        int *target = NULL;

        if (strcmp(color_names[i], "black") == 0)
            target = scheme.black;
        else if (strcmp(color_names[i], "white") == 0)
            target = scheme.white;
        else if (strcmp(color_names[i], "green") == 0)
            target = scheme.green;
        else if (strcmp(color_names[i], "yellow") == 0)
            target = scheme.yellow;
        else if (strcmp(color_names[i], "red") == 0)
            target = scheme.red;
        else if (strcmp(color_names[i], "magenta") == 0)
            target = scheme.magenta;
        else if (strcmp(color_names[i], "cyan") == 0)
            target = scheme.cyan;
        else if (strcmp(color_names[i], "blue") == 0)
            target = scheme.blue;

        if (!parse_rgb_array(color_arr, target, color_names[i], scheme.name))
        {
            free(scheme.name);
            return 0;
        }
    }

    // Add to array
    ColorScheme *tmp = realloc(color_schemes, (scheme_count + 1) * sizeof(ColorScheme));
    if (!tmp)
    {
        fprintf(stderr, "Memory allocation failed for scheme '%s'\n", scheme.name);
        free(scheme.name);
        return 0;
    }

    color_schemes = tmp;
    color_schemes[scheme_count++] = scheme;
    return 1;
}

/**
 * Loads color schemes from a TOML configuration file.
 *
 * @param filename Path to the TOML configuration file
 * @return 1 on success, 0 on failure
 *
 * The function expects a TOML file with the following structure:
 * - A root array named "colorschemes"
 * - Each element in the array is a table containing a color scheme definition
 *
 * The function:
 * 1. Opens and parses the TOML file
 * 2. Locates the "colorschemes" array
 * 3. Iterates through each scheme in the array
 * 4. Parses each scheme using parse_color_scheme()
 *
 * Error conditions:
 * - File cannot be opened
 * - TOML parsing errors
 * - Missing "colorschemes" array
 * - Invalid scheme definitions
 */
int load_color_schemes(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Error opening file");
        return 0;
    }

    char errbuf[200];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root)
    {
        fprintf(stderr, "TOML Parse error: %s\n", errbuf);
        return 0;
    }

    toml_array_t *schemes = toml_array_in(root, "colorschemes");
    if (!schemes)
    {
        fprintf(stderr, "Root 'colorschemes' array not found\n");
        toml_free(root);
        return 0;
    }

    int arr_len = toml_array_nelem(schemes);

    for (int i = 0; i < arr_len; i++)
    {
        toml_table_t *scheme = toml_table_at(schemes, i);
        if (!scheme)
        {
            fprintf(stderr, "Invalid scheme at index %d: Not a table\n", i);
            continue;
        }

        if (!parse_color_scheme(scheme))
        {
            fprintf(stderr, "Aborting due to error in scheme %d\n", i);
            toml_free(root);
            return 0;
        }
    }

    toml_free(root);
    return 1;
}

/**
 * Loads the currently active color scheme name from a TOML configuration file.
 *
 * @param filename Path to the TOML configuration file
 * @return 1 on success, 0 on failure
 *
 * The function expects a TOML file containing an 'actual_colorscheme' string field.
 * The value is stored in the global 'actual_scheme' variable.
 *
 * Error conditions:
 * - File cannot be opened
 * - TOML parsing errors
 * - Missing 'actual_colorscheme' field
 * - Invalid string value
 */
int load_actual_scheme(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Error opening file");
        return 0;
    }

    char errbuf[200];
    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root)
    {
        fprintf(stderr, "TOML Parse error: %s\n", errbuf);
        return 0;
    }

    toml_raw_t actual_scheme_raw = toml_raw_in(root, "actual_colorscheme");
    if (!actual_scheme_raw)
    {
        fprintf(stderr, "Missing 'actual_colorscheme' in file\n");
        toml_free(root);
        return 0;
    }

    if (toml_rtos(actual_scheme_raw, &actual_scheme) != 0)
    {
        fprintf(stderr, "Failed to parse 'actual_colorscheme'\n");
        toml_free(root);
        return 0;
    }

    toml_free(root);
    return 1;
}
