#include "ui_list.h"
#include "ui_common.h"
#include "strings.h"

static Window *s_window;
static MenuLayer *s_menu;
static Window *s_nobooks_window;
static TextLayer *s_nobooks_text;

static uint16_t get_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  int n;
  ui_books(&n);
  return n;
}

static void draw_row(GContext *gctx, const Layer *cell, MenuIndex *idx, void *ctx) {
  int n;
  const DigestBook *books = ui_books(&n);
  if (idx->row >= n) {
    return;
  }
  const DigestBook *b = &books[idx->row];

  GRect bounds = layer_get_bounds(cell);
  graphics_context_set_text_color(gctx, ui_color_for_book_state(b->color));

  char title[56];
  if (b->favorite) {
    snprintf(title, sizeof(title), "★ %s", b->title);
  } else {
    snprintf(title, sizeof(title), "%s", b->title);
  }

  graphics_draw_text(gctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(bounds.origin.x + 4, bounds.origin.y + 2,
                           bounds.size.w - 8, bounds.size.h - 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static int16_t get_cell_height(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  return 46;
}

static void selection_changed(MenuLayer *menu, MenuIndex new_index,
                              MenuIndex old_index, void *ctx) {
  g_ctx.book_index = new_index.row;
}

static void select_click(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  g_ctx.book_index = idx->row;
  ui_dispatch(EV_SELECT);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  ui_setup_status_bar(window);

  GRect mf = GRect(0, STATUS_BAR_LAYER_HEIGHT, b.size.w,
                   b.size.h - STATUS_BAR_LAYER_HEIGHT);
  s_menu = menu_layer_create(mf);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_rows,
    .draw_row = draw_row,
    .get_cell_height = get_cell_height,
    .selection_changed = selection_changed,
    .select_click = select_click,
  });
  menu_layer_set_normal_colors(s_menu, GColorBlack, GColorWhite);
  menu_layer_set_highlight_colors(s_menu, GColorDarkGray, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
}

Window *ui_list_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  return s_window;
}

void ui_list_refresh(void) {
  if (s_menu) {
    int n;
    ui_books(&n);
    menu_layer_reload_data(s_menu);
    if (g_ctx.book_index >= 0 && g_ctx.book_index < n) {
      menu_layer_set_selected_index(
          s_menu, MenuIndex(0, g_ctx.book_index),
          MenuRowAlignCenter, false);
    }
  }
}

static void nobooks_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  ui_setup_status_bar(window);
  s_nobooks_text = text_layer_create(GRect(6, 50, b.size.w - 12, b.size.h - 60));
  text_layer_set_text(s_nobooks_text, S(STR_NO_BOOKS));
  text_layer_set_font(s_nobooks_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_nobooks_text, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_nobooks_text));
}

static void nobooks_unload(Window *window) {
  text_layer_destroy(s_nobooks_text);
  s_nobooks_text = NULL;
}

Window *ui_nobooks_window(void) {
  if (!s_nobooks_window) {
    s_nobooks_window = window_create();
    window_set_window_handlers(s_nobooks_window, (WindowHandlers) {
      .load = nobooks_load,
      .unload = nobooks_unload,
    });
  }
  return s_nobooks_window;
}
