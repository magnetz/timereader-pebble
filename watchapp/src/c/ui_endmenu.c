#include "ui_endmenu.h"
#include "ui_common.h"
#include "strings.h"

static ActionMenu *s_menu;
static ActionMenuLevel *s_level;
static bool s_closing_for_state;
static Window *s_confirm_window;
static ActionBarLayer *s_confirm_bar;
static TextLayer *s_confirm_text;
static TextLayer *s_confirm_hint;

static void item_performed(ActionMenu *menu, const ActionMenuItem *item, void *ctx) {
  (void)menu; (void)item; (void)ctx;   /* work happens in menu_did_close */
}

static void menu_did_close(ActionMenu *menu, const ActionMenuItem *performed, void *ctx) {
  (void)menu; (void)ctx;
  s_menu = NULL;
  if (s_closing_for_state) {
    /* We closed it ourselves because the state already moved on. */
    s_closing_for_state = false;
    return;
  }
  if (performed) {
    g_ctx.end_menu_index = (int)(uintptr_t)action_menu_item_get_action_data(performed);
    ui_dispatch(EV_SELECT);
  } else if (g_ctx.state == APP_END_SESSION_MENU && !g_ctx.end_menu_confirming) {
    /* Dismissed with Back, no item chosen -> "Annulla". */
    ui_dispatch(EV_BACK);
  }
}

static void ensure_level(void) {
  if (s_level) return;
  s_level = action_menu_level_create(3);
  action_menu_level_add_action(s_level, S(STR_MENU_SAVE), item_performed,
                               (void *)(uintptr_t)0);
  action_menu_level_add_action(s_level, S(STR_MENU_EXIT), item_performed,
                               (void *)(uintptr_t)1);
  action_menu_level_add_action(s_level, S(STR_MENU_CANCEL), item_performed,
                               (void *)(uintptr_t)2);
}

static void open_menu(void) {
  if (s_menu) return;
  ensure_level();
  ActionMenuConfig config = {
    .root_level = s_level,
    .colors = {
      .background = GColorChromeYellow,
      .foreground = GColorBlack,
    },
    .align = ActionMenuAlignCenter,
    .did_close = menu_did_close,
  };
  s_menu = action_menu_open(&config);
}

/* --- confirm modal --------------------------------------------------- */

static void confirm_select(ClickRecognizerRef r, void *c) { ui_dispatch(EV_SELECT); }
static void confirm_back(ClickRecognizerRef r, void *c) { ui_dispatch(EV_BACK); }

static void confirm_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, confirm_select);
  window_single_click_subscribe(BUTTON_ID_BACK, confirm_back);
}

static void confirm_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  s_confirm_text = text_layer_create(GRect(6, 40, b.size.w - ACTION_BAR_WIDTH - 10,
                                           b.size.h - 60));
  text_layer_set_background_color(s_confirm_text, GColorClear);
  text_layer_set_text_color(s_confirm_text, GColorWhite);
  text_layer_set_font(s_confirm_text, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(s_confirm_text, S(STR_EXIT_CONFIRM));
  layer_add_child(root, text_layer_get_layer(s_confirm_text));

  s_confirm_hint = text_layer_create(GRect(6, b.size.h - 40,
                                           b.size.w - ACTION_BAR_WIDTH - 10, 36));
  text_layer_set_background_color(s_confirm_hint, GColorClear);
  text_layer_set_text_color(s_confirm_hint, GColorLightGray);
  text_layer_set_font(s_confirm_hint, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(s_confirm_hint, S(STR_EXIT_HINT));
  layer_add_child(root, text_layer_get_layer(s_confirm_hint));

  s_confirm_bar = action_bar_layer_create();
  action_bar_layer_set_background_color(s_confirm_bar, GColorChromeYellow);
  action_bar_layer_set_click_config_provider(s_confirm_bar, confirm_click_config);
  action_bar_layer_add_to_window(s_confirm_bar, window);
}

static void confirm_unload(Window *window) {
  action_bar_layer_destroy(s_confirm_bar);
  s_confirm_bar = NULL;
  text_layer_destroy(s_confirm_hint);
  s_confirm_hint = NULL;
  text_layer_destroy(s_confirm_text);
  s_confirm_text = NULL;
}

Window *ui_endmenu_confirm_window(void) {
  if (!s_confirm_window) {
    s_confirm_window = window_create();
    window_set_background_color(s_confirm_window, GColorBlack);
    window_set_click_config_provider(s_confirm_window, confirm_click_config);
    window_set_window_handlers(s_confirm_window, (WindowHandlers) {
      .load = confirm_load,
      .unload = confirm_unload,
    });
  }
  return s_confirm_window;
}

void ui_endmenu_sync(void) {
  bool want_menu = (g_ctx.state == APP_END_SESSION_MENU) &&
                   !g_ctx.end_menu_confirming;
  if (want_menu && !s_menu) {
    open_menu();
  } else if (!want_menu && s_menu) {
    s_closing_for_state = true;
    action_menu_close(s_menu, false);
    s_menu = NULL;
  }
}
