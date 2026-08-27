#include "ui_timer.h"
#include "ui_common.h"
#include "strings.h"
#include "session.h"

static Window *s_window;
static Layer *s_content;
static ActionBarLayer *s_action_bar;
static Layer *s_icon_layer;
static GDrawCommandImage *s_play;
static GDrawCommandImage *s_pause;

static const DigestBook *current_book(void) {
  int n;
  const DigestBook *books = ui_books(&n);
  int i = g_ctx.book_index;
  if (i < 0 || i >= n) i = 0;
  return &books[i];
}

static void content_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int now = (int)time(NULL);
  int secs = live_session_elapsed(&g_ctx.live, now);

  char clock[12];
  ui_format_duration(clock, sizeof(clock), secs);
  bool long_form = (secs >= 3600);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, clock,
                     fonts_get_system_font(long_form
                         ? FONT_KEY_GOTHIC_28_BOLD
                         : FONT_KEY_BITHAM_34_MEDIUM_NUMBERS),
                     GRect(0, long_form ? 26 : 18, b.size.w, 42),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_draw_text(ctx, current_book()->title,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(4, 66, b.size.w - 8, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (g_ctx.state == APP_PAUSED) {
    graphics_context_set_text_color(ctx, GColorChromeYellow);
    graphics_draw_text(ctx, S(STR_PAUSE), fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       GRect(0, 96, b.size.w, 24),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void icon_update(Layer *layer, GContext *ctx) {
  GDrawCommandImage *img = (g_ctx.state == APP_PAUSED) ? s_play : s_pause;
  if (!img) return;
  GSize sz = gdraw_command_image_get_bounds_size(img);
  GRect b = layer_get_bounds(layer);
  GPoint origin = GPoint((b.size.w - sz.w) / 2, (b.size.h - sz.h) / 2);
  gdraw_command_image_draw(ctx, img, origin);
}

static void click_select(ClickRecognizerRef r, void *c) { ui_dispatch(EV_SELECT); }
static void click_back(ClickRecognizerRef r, void *c) { ui_dispatch(EV_BACK); }

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, click_select);
  window_single_click_subscribe(BUTTON_ID_BACK, click_back);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  ui_setup_status_bar(window);

  s_content = layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT,
                                 b.size.w - ACTION_BAR_WIDTH,
                                 b.size.h - STATUS_BAR_LAYER_HEIGHT));
  layer_set_update_proc(s_content, content_update);
  layer_add_child(root, s_content);

  s_action_bar = action_bar_layer_create();
  action_bar_layer_set_background_color(s_action_bar, GColorBlack);
  action_bar_layer_add_to_window(s_action_bar, window);
  action_bar_layer_set_click_config_provider(s_action_bar, click_config);

  s_icon_layer = layer_create(GRect(b.size.w - ACTION_BAR_WIDTH,
                                    STATUS_BAR_LAYER_HEIGHT, ACTION_BAR_WIDTH,
                                    b.size.h - STATUS_BAR_LAYER_HEIGHT));
  layer_set_update_proc(s_icon_layer, icon_update);
  layer_add_child(root, s_icon_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_icon_layer);
  s_icon_layer = NULL;
  action_bar_layer_destroy(s_action_bar);
  s_action_bar = NULL;
  layer_destroy(s_content);
  s_content = NULL;
}

Window *ui_timer_window(void) {
  if (!s_window) {
    s_play = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_PLAY);
    s_pause = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_PAUSE);
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  return s_window;
}

void ui_timer_refresh(void) {
  if (s_content) layer_mark_dirty(s_content);
  if (s_icon_layer) layer_mark_dirty(s_icon_layer);
}
