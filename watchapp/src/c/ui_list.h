#ifndef TR_UI_LIST_H
#define TR_UI_LIST_H

#include <pebble.h>

Window *ui_list_window(void);      /* the book-list MenuLayer window */
Window *ui_nobooks_window(void);   /* the "no books" placeholder window */
void ui_list_refresh(void);

#endif
