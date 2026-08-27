#include "ui_digit.h"
#include "ui_common.h"
#include "strings.h"

static Window *s_window;
static Layer *s_content;
static Layer *s_highlight;
static PropertyAnimation *s_anim;
static int s_shown_cursor;
static bool s_error;
static AppTimer *s_error_timer;

static GRect slot_box(int cursor) {
  GRect b = layer_get_bounds(s_content);
  int slot_w = b.size.w / 4;
  int x = slot_w * cursor + 4;
  return GRect(x, 96, slot_w - 8, 4);
}

static void content_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_text_color(ctx, GColorWhite);

  const char *title = S(g_ctx.state == APP_ENTER_START_PAGE
                            ? STR_START_PAGE : STR_END_PAGE);
  graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(4, 0, b.size.w - 8, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  GFont f = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  int slot_w = b.size.w / 4;
  for (int i = 0; i < 4; i++) {
    char d[2] = { (char)('0' + g_ctx.entry.digits[i]), 0 };
    graphics_draw_text(ctx, d, f, GRect(slot_w * i, 34, slot_w, 48),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  if (s_error) {
    graphics_context_set_text_color(ctx, GColorRed);
    graphics_draw_text(ctx, S(STR_END_LT_START),
                       fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(2, 108, b.size.w - 4, 44),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void highlight_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
}

static void move_highlight(void) {
  if (!s_highlight) return;
  GRect to = slot_box(g_ctx.entry.cursor);
  if (s_shown_cursor == g_ctx.entry.cursor) {
    layer_set_frame(s_highlight, to);
    return;
  }
  s_anim = property_animation_create_layer_frame(s_highlight, NULL, &to);
  Animation *a = property_animation_get_animation(s_anim);
  animation_set_curve(a, AnimationCurveEaseInOut);
  animation_set_duration(a, 120);
  animation_schedule(a);
  s_shown_cursor = g_ctx.entry.cursor;
}

static void clear_error(void *data) {
  s_error = false;
  s_error_timer = NULL;
  if (s_content) layer_mark_dirty(s_content);
}

void ui_digit_flash_error(void) {
  s_error = true;
  if (s_content) layer_mark_dirty(s_content);
  if (s_error_timer) app_timer_cancel(s_error_timer);
  s_error_timer = app_timer_register(1500, clear_error, NULL);
}

static void click_up(ClickRecognizerRef r, void *c)   { ui_dispatch(EV_UP); }
static void click_down(ClickRecognizerRef r, void *c) { ui_dispatch(EV_DOWN); }
static void click_select(ClickRecognizerRef r, void *c) { ui_dispatch(EV_SELECT); }
static void click_back(ClickRecognizerRef r, void *c) { ui_dispatch(EV_BACK); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, click_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_select);
  window_single_click_subscribe(BUTTON_ID_BACK, click_back);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  ui_setup_status_bar(window);
  GRect cf = GRect(0, STATUS_BAR_LAYER_HEIGHT, b.size.w,
                   b.size.h - STATUS_BAR_LAYER_HEIGHT);
  s_content = layer_create(cf);
  layer_set_update_proc(s_content, content_update);
  layer_add_child(root, s_content);

  s_shown_cursor = g_ctx.entry.cursor;
  s_highlight = layer_create(slot_box(g_ctx.entry.cursor));
  layer_set_update_proc(s_highlight, highlight_update);
  layer_add_child(s_content, s_highlight);
}

static void window_unload(Window *window) {
  layer_destroy(s_highlight);
  s_highlight = NULL;
  layer_destroy(s_content);
  s_content = NULL;
}

Window *ui_digit_window(void) {
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

void ui_digit_refresh(void) {
  if (s_content) {
    layer_mark_dirty(s_content);
    move_highlight();
  }
}
