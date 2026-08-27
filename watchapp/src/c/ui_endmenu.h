#ifndef TR_UI_ENDMENU_H
#define TR_UI_ENDMENU_H

#include <pebble.h>

/* Reconcile the end-session ActionMenu / confirm modal with g_ctx. Called
   from ui_route_to_state. */
void ui_endmenu_sync(void);

/* The "exit without saving?" confirmation modal window. */
Window *ui_endmenu_confirm_window(void);

#endif
