#ifndef TR_UI_COMMON_H
#define TR_UI_COMMON_H

#include <pebble.h>
#include "state_machine.h"
#include "model.h"

/* The one app context, owned by ui_common.c. */
extern SmContext g_ctx;

/* Books backing the current session (seed data in SP1). */
const DigestBook *ui_books(int *count);

/* Colour for a book title by its reading state. */
GColor ui_color_for_book_state(BookColorState s);

/* StatusBarLayer across the top of a long-lived window, app-consistent. */
void ui_setup_status_bar(Window *w);

/* Push/pop windows so the window stack matches g_ctx.state. */
void ui_route_to_state(void);

/* Refresh whichever window currently reflects g_ctx.state. */
void ui_refresh_current(void);

/* Single choke point: sm_handle + perform side effect + route + refresh.
   Every window's click handler calls this. */
void ui_dispatch(Event ev);

#endif
