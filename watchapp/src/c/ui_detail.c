#include "ui_detail.h"
#include "ui_common.h"
#include "session.h"

static Window *s_window;
static Layer *s_content;

static const DigestBook *current_book(void) {
  int n;
  const DigestBook *books = ui_books(&n);
  if (g_ctx.book_index < 0 || g_ctx.book_index >= n) {
    return &books[0];
  }
  return &books[g_ctx.book_index];
}

static void draw_page_dots(GContext *ctx, GRect bounds) {
  const int dots = 3;
  const int r = 3;
  const int gap = 12;
  int total_w = (dots - 1) * gap;
  int x0 = bounds.size.w / 2 - total_w / 2;
  int y = bounds.size.h - 12;
  for (int i = 0; i < dots; i++) {
    GPoint c = GPoint(x0 + i * gap, y);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorWhite);
    if (i == g_ctx.detail_page) {
      graphics_fill_circle(ctx, c, r);
    } else {
      graphics_draw_circle(ctx, c, r);
    }
  }
}

static void content_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const DigestBook *bk = current_book();
  GFont f_label = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f_big = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  graphics_context_set_text_color(ctx, GColorWhite);
  char line[64];

  /* Title on every page. */
  graphics_draw_text(ctx, bk->title, f_label, GRect(4, 0, b.size.w - 8, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (g_ctx.detail_page == 0) {
    snprintf(line, sizeof(line), "Pag. %d", bk->current_page);
    graphics_draw_text(ctx, line, f_big, GRect(4, 34, b.size.w - 8, 34),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    snprintf(line, sizeof(line), "%d.%d pag/ora%s",
             bk->pph_x100 / 100, (bk->pph_x100 % 100) / 10,
             bk->pph_is_estimate ? " (stima)" : "");
    graphics_draw_text(ctx, line, f_label, GRect(4, 74, b.size.w - 8, 24),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else if (g_ctx.detail_page == 1) {
    snprintf(line, sizeof(line), "Tempo tot %d.%02d h",
             bk->hours_x100 / 100, bk->hours_x100 % 100);
    graphics_draw_text(ctx, line, f_label, GRect(4, 40, b.size.w - 8, 24),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    int left = pages_left(bk->total_pages, bk->current_page);
    int eta = eta_minutes(left, bk->pph_x100);
    if (eta >= 60) {
      snprintf(line, sizeof(line), "Resta %dp ~%d h%s", left, eta / 60,
               bk->pph_is_estimate ? " (stima)" : "");
    } else {
      snprintf(line, sizeof(line), "Resta %dp ~%d min%s", left, eta,
               bk->pph_is_estimate ? " (stima)" : "");
    }
    graphics_draw_text(ctx, line, f_label, GRect(4, 68, b.size.w - 8, 44),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else {
    if (bk->total_pages == 0) {
      graphics_draw_text(ctx, "Pagine totali\nsconosciute", f_label,
                         GRect(4, 44, b.size.w - 8, 48),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    } else {
      int pct = (bk->current_page * 100) / bk->total_pages;
      if (pct > 100) pct = 100;
      snprintf(line, sizeof(line), "%d%%", pct);
      graphics_draw_text(ctx, line, f_big, GRect(4, 34, b.size.w - 8, 34),
                         GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      GRect bar = GRect(12, 78, b.size.w - 24, 10);
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_rect(ctx, bar);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(bar.origin.x, bar.origin.y,
                                    (bar.size.w * pct) / 100, bar.size.h),
                         0, GCornerNone);
    }
  }

  draw_page_dots(ctx, b);
}

static void click_up(ClickRecognizerRef r, void *c) {
  ui_dispatch(EV_UP);
}
static void click_down(ClickRecognizerRef r, void *c) {
  ui_dispatch(EV_DOWN);
}
static void click_select(ClickRecognizerRef r, void *c) {
  ui_dispatch(EV_SELECT);
}
static void click_back(ClickRecognizerRef r, void *c) {
  ui_dispatch(EV_BACK);
}

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
  s_content = layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, b.size.w,
                                 b.size.h - STATUS_BAR_LAYER_HEIGHT));
  layer_set_update_proc(s_content, content_update);
  layer_add_child(root, s_content);
}

static void window_unload(Window *window) {
  layer_destroy(s_content);
  s_content = NULL;
}

Window *ui_detail_window(void) {
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

void ui_detail_refresh(void) {
  if (s_content) {
    layer_mark_dirty(s_content);
  }
}
