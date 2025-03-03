#ifndef _DISPLAY_H_
#define _DISPLAY_H_
#include <stdbool.h>
#include "service.h"
#include "bus.h"

#define KEY_RETURN 10
#define KEY_ESC 27
#define KEY_SPACE 32

#define D_ESCOFF_MS 300000LLU
#define D_VERSION "1.6.5"
#define D_FUNCTIONS "F1:START F2:STOP F3:RESTART F4:ENABLE F5:DISABLE F6:MASK F7:UNMASK F8:RELOAD"
#define D_SERVICE_TYPES "a:ALL d:DEV i:SLICE s:SERVICE o:SOCKET t:TARGET r:TIMER m:MOUNT c:SCOPE n:AMOUNT w:SWAP p:PATH h:SSHOT"
#define D_HEADLINE "ServiceMaster " D_VERSION
#define D_NAVIGATION_BASE "Left/Right: Modus | Up/Down: Select | Return: Show status | PageUp/Down: Scroll | f: Search | Space: System/User | +,-: Theme = %s"
#define D_QUIT "q/ESC:Quit"

#define D_XLOAD 84
#define D_XACTIVE 94
#define D_XSUB 104
#define D_XDESCRIPTION 114

#define D_MODE(m)        \
    {                    \
        position = 0;    \
        index_start = 0; \
        mode = m;        \
        clear();         \
    }

extern char *program_name;

typedef enum
{
    DEFAULT,
    NORD,
    SOLARIZEDDARK,
    DRACULA,
    MONOKAI,
    GRUVBOXDARK,
    ONEDARK,
    MONOCHROME,
    SOLARIZEDLIGHT
} colorscheme_t;

typedef struct
{
    const char *name;
    struct
    {
        short r, g, b;
    } colors[8];
} ColorScheme;

extern colorscheme_t colorscheme;

enum bus_type display_bus_type(void);
enum service_type display_mode(void);
void display_erase(void);
void display_init(void);
void display_redraw(Bus *bus);
void display_redraw_row(Service *svc);
void display_set_bus_type(enum bus_type);
void display_status_window(const char *status, const char *title);
void d_op(Bus *bus, Service *svc, enum operation mode, const char *txt);
void set_color_scheme(colorscheme_t scheme);

#endif
