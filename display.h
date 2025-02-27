#ifndef _DISPLAY_H_
#define _DISPLAY_H_
#include <stdbool.h>
#include "service.h"
#include "bus.h"

#define KEY_RETURN 10
#define KEY_ESC 27
#define KEY_SPACE 32

#define D_ESCOFF_MS      300000LLU
#define D_VERSION        "1.5.1"
#define D_FUNCTIONS      "F1:START F2:STOP F3:RESTART F4:ENABLE F5:DISABLE F6:MASK F7:UNMASK F8:RELOAD"
#define D_SERVICE_TYPES  "A:ALL D:DEV I:SLICE S:SERVICE O:SOCKET T:TARGET R:TIMER M:MOUNT C:SCOPE N:AMOUNT W:SWAP P:PATH H:SSHOT"
#define D_HEADLINE       "ServiceMaster "D_VERSION""
#define D_NAVIGATION     "Left/Right: Modus | Up/Down: Select | Return: Show status"
#define D_QUIT           "Q/ESC:Quit"

#define D_XLOAD 84
#define D_XACTIVE 94
#define D_XSUB 104
#define D_XDESCRIPTION 114

#define D_MODE(m) {\
    position = 0;\
    index_start = 0;\
    mode = m;\
    clear();\
}

#define D_OP(bus, svc, mode, txt) {\
    bool success = false;\
    if(bus->type == SYSTEM && euid != 0) {\
        display_status_window(" You must be root for this operation on system units. Press space to toggle: System/User.", "info:");\
        break;\
    }\
    Service *temp_svc = service_nth(bus, position + index_start);\
    if (!temp_svc) {\
        display_status_window("No valid service selected.", "Error:");\
        break;\
    }\
    success = bus_operation(bus, temp_svc, mode);\
    if (!success)\
        display_status_window("Command could not be executed on this unit.", txt":");\
}

enum bus_type display_bus_type(void);
enum service_type display_mode(void);
void display_erase(void);
void display_init(void);
void display_redraw(Bus *bus);
void display_redraw_row(Service *svc);
void display_set_bus_type(enum bus_type);
void display_status_window(const char *status, const char *title);
#endif
