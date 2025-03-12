// include guard:
#ifndef SHOW_H
#define SHOW_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // For int64_t
#include "lib/toml.h"
#include <ncurses.h>

#define NUM_COLORS 8

typedef struct
{
    char *name;
    int black[3];
    int white[3];
    int green[3];
    int yellow[3];
    int red[3];
    int magenta[3];
    int cyan[3];
    int blue[3];
} ColorScheme;

extern char *actual_scheme;
extern ColorScheme *color_schemes;
extern int scheme_count;
void free_color_schemes();
int parse_rgb_array(toml_array_t *arr, int *rgb, const char *color_name, const char *scheme_name);
int parse_color_scheme(toml_table_t *table);
int load_color_schemes(const char *filename);
int load_actual_scheme(const char *filename);
void print_file(const char *filename);
void cleanup_handler(int signum);
void setup_signal_handlers();

#endif
