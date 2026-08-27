#include "ui_summary.h"
#include "ui_common.h"
#include "strings.h"

static Window *s_window;
static Layer *s_content;

static void content_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f_label = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f_big = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  graphics_context_set_text_color(ctx, GColorWhite);

  char line[48];

  graphics_draw_text(ctx, S(STR_SESSION_SAVED), f_label,
                     GRect(4, 0, b.size.w - 8, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  snprintf(line, sizeof(line), S(STR_PAGES_FMT), g_ctx.last_session_pages);
  graphics_draw_text(ctx, line, f_big, GRect(4, 24, b.size.w - 8, 32),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  char dur[12];
  ui_format_duration(dur, sizeof(dur), g_ctx.last_session_seconds);
  int secs = g_ctx.last_session_seconds;
  int pph_x100 = secs > 0
      ? (int)(((long)g_ctx.last_session_pages * 360000) / secs)
      : 0;
  snprintf(line, sizeof(line), "%s", dur);
  graphics_draw_text(ctx, line, f_label, GRect(4, 60, b.size.w - 8, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  snprintf(line, sizeof(line), S(STR_PPH_FMT),
           pph_x100 / 100, (pph_x100 % 100) / 10, "");
  graphics_draw_text(ctx, line, f_label, GRect(4, 80, b.size.w - 8, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, S(STR_SUMMARY_HINT), f_label,
                     GRect(4, 104, b.size.w - 8, 40),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void click_select(ClickRecognizerRef r, void *c) { ui_dispatch(EV_SELECT); }
static void click_back(ClickRecognizerRef r, void *c) { ui_dispatch(EV_BACK); }
static void click_up(ClickRecognizerRef r, void *c) { ui_dispatch(EV_UP); }
static void click_back_long(ClickRecognizerRef r, void *c) { ui_dispatch(EV_BACK_LONG); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, click_select);
  window_single_click_subscribe(BUTTON_ID_BACK, click_back);
  window_single_click_subscribe(BUTTON_ID_UP, click_up);
  window_long_click_subscribe(BUTTON_ID_BACK, 0, click_back_long, NULL);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  ui_setup_status_bar(window);
  s_content = layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, b.size.w,
                                 b.size.h - STATUS_BAR_LAYER_HEIGHT));
  layer_set_update_proc(s_content, content_update);
  layer_add_child(root, s_content);
}

static void window_unload(Window *window) {
  layer_destroy(s_content);
  s_content = NULL;
}

Window *ui_summary_window(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  return s_window;
}

void ui_summary_refresh(void) {
  if (s_content) layer_mark_dirty(s_content);
}
