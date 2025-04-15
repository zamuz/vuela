/*
    Copyright (C) 2022 Gonzalo Munoz.

    This file is part of Vuela.

    Vuela is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Vuela.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "enamel.h"
#include "watch_model.h"
#include <pebble-events/pebble-events.h>
#include <ctype.h>
#include <stdlib.h>

static Window *window;
static Layer *clock_layer;
static Layer *seconds_layer;
static Layer *marks_layer;
static Layer *day_layer;
ClockState clock_state;
GFont digital_font;

ResHandle get_font_handle(void) {
    bool square = strcmp(enamel_get_clock_font(), "SQUARE") == 0;
    return resource_get_handle(square ? RESOURCE_ID_SILLYPIXEL_11 : RESOURCE_ID_PIXOLLETTA_10);
}

bool battery_saver_enabled(int hour) {
    if (enamel_get_battery_saver_enabled()) {
        int hours[] = { 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        int from = atoi(enamel_get_battery_saver_start());
        int to = atoi(enamel_get_battery_saver_stop());
        int hour_index  = -1;
        int i;
        for (i = from; i <= to; i++) {
            if (hours[i] == hour) {
                hour_index = i;
        	break;
            }
        }
        return (hour_index >= from) && (hour_index < to);
    }
    return false;
}

void watch_model_handle_clock_change(ClockState state) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "CLOCK update"); 
  clock_state = state;
  layer_mark_dirty(clock_layer);
  layer_mark_dirty(seconds_layer);
  layer_mark_dirty(day_layer);
}

void watch_model_handle_time_change(struct tm *tick_time) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "MINUTES update");
  clock_state.minute_angle = tick_time->tm_min * 6;
  clock_state.hour_angle = tick_time->tm_hour%12 * 30 + clock_state.minute_angle*.08;
  clock_state.second_angle = tick_time->tm_sec * 6;
  clock_state.date = tick_time->tm_mday;
  clock_state.hour = tick_time->tm_hour;
  layer_mark_dirty(clock_layer);
  layer_mark_dirty(seconds_layer);
  layer_mark_dirty(day_layer);
}

void watch_model_handle_seconds_change(struct tm *tick_time) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "SECONDS update");
  clock_state.second_angle = tick_time->tm_sec * 6;
  layer_mark_dirty(seconds_layer);
}

void watch_model_handle_config_change(void) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "CONFIG update");
  time_t t = time(NULL);
  struct tm *now = localtime(&t);
  update_subscriptions(now->tm_hour);
  fonts_unload_custom_font(digital_font);
  digital_font = fonts_load_custom_font(get_font_handle());
  layer_mark_dirty(clock_layer);
  layer_mark_dirty(seconds_layer);
  layer_mark_dirty(day_layer);
}

static bool font_is_square(void) {
    return strcmp(enamel_get_clock_font(), "SQUARE") == 0;
}

static void draw_day(Layer *layer, GContext *ctx) {
    if (enamel_get_display_date()) {
        GRect layer_bounds = layer_get_unobstructed_bounds(layer);
        int w = layer_bounds.size.w;
        int h = layer_bounds.size.h;
        graphics_context_set_fill_color(ctx, enamel_get_clock_fg_color());
        graphics_context_set_text_color(ctx, enamel_get_clock_bg_color());
        static char s_date[3];
        snprintf(s_date, sizeof(s_date), "%d", clock_state.date);
        GRect text_box = (GRect) {
          .size = GSize(16, 15),
          .origin = GPoint(w < 180 ? (w*.815) : (w*.83), font_is_square() ? (h*.5 - 7) : (h*.5 - 6))
        };
	GRect date_bg = grect_inset(text_box, GEdgeInsets(font_is_square() ? -1 : -2, 1, 0, 0));
        graphics_fill_rect(ctx, date_bg, 3, GCornersAll);
        graphics_draw_text(ctx, s_date, digital_font, text_box, GTextOverflowModeFill,
                           GTextAlignmentCenter, NULL);
    }
}

static void draw_seconds(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_unobstructed_bounds(layer);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    GPoint center_point = GPoint(w*.5, h*.5);
    GRect sec_frame = (GRect) { .size = GSize(w*.96, h*.96) };
    grect_align(&sec_frame, &layer_bounds, GAlignCenter, false);
    if (enamel_get_display_seconds() && !battery_saver_enabled(clock_state.hour)) {
        // seconds hand
	GPoint sec_to = gpoint_from_polar(sec_frame, GOvalScaleModeFitCircle,
                                          DEG_TO_TRIGANGLE(clock_state.second_angle));
        // draw seconds hand outline
	graphics_context_set_stroke_color(ctx, enamel_get_clock_bg_color());
        graphics_context_set_stroke_width(ctx, 5);
        graphics_draw_line(ctx, center_point, sec_to);
	//
	graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
	graphics_fill_circle(ctx, center_point, 4);
	// draw seconds hand
	graphics_context_set_fill_color(ctx, enamel_get_second_hand_color());
	graphics_fill_circle(ctx, center_point, 3);
	//
	graphics_context_set_stroke_color(ctx, enamel_get_second_hand_color());
        graphics_context_set_stroke_width(ctx, 3);
        graphics_draw_line(ctx, center_point, sec_to);
	// draw seconds hand center
	graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
	graphics_fill_circle(ctx, center_point, 1);
    }
}

static void draw_marks(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_unobstructed_bounds(layer);
    // screen background
#if defined(PBL_ROUND)
    graphics_context_set_fill_color(ctx, GColorBlack);
#else
    graphics_context_set_fill_color(ctx, enamel_get_screen_color());
#endif
    graphics_fill_rect(ctx, layer_bounds, 0, (GCornerMask)NULL);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    int angle_from;
    static char s_min_string[5];
    int min;
    GRect text_frame = (GRect) { .size = GSize(w*.78, h*.78) };
    grect_align(&text_frame, &layer_bounds, GAlignCenter, false);
    int text_position = text_frame.origin.y;
    text_frame.origin.y = text_position - 1;
    GRect circle_frame = (GRect) { .size = GSize(w, h) };
    grect_align(&circle_frame, &layer_bounds, GAlignCenter, false);
    // clock background
    graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
    graphics_fill_radial(ctx, circle_frame, GOvalScaleModeFitCircle, w*.49, 0, TRIG_MAX_ANGLE);
    GRect outer_frame = (GRect) { .size = GSize(w*.97, h*.97) };
    grect_align(&outer_frame, &layer_bounds, GAlignCenter, false);
    GRect inner_frame = (GRect) { .size = GSize(w*.9, h*.9) };
    grect_align(&inner_frame, &layer_bounds, GAlignCenter, false);
    // minute dial markers
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, enamel_get_clock_fg_color());
    graphics_context_set_text_color(ctx, enamel_get_clock_fg_color());
    for (min = 60; min > 0; min = min - 1) {
        angle_from = min * 6;
        if ((min % 5) == 0) {
	    // minute text
	    snprintf(s_min_string, sizeof(s_min_string), "%02d", min);
            GSize text_size = graphics_text_layout_get_content_size(s_min_string, digital_font,
                                                                    layer_bounds,
                                                                    GTextOverflowModeFill,
                                                                    GTextAlignmentCenter);
	    GRect text_box = grect_centered_from_polar(text_frame, GOvalScaleModeFitCircle,
                                                       DEG_TO_TRIGANGLE(angle_from), text_size);
            graphics_draw_text(ctx, s_min_string, digital_font, text_box,
                               GTextOverflowModeFill, GTextAlignmentCenter, NULL);
	}
        // minute marks
	GPoint mark_from = gpoint_from_polar(outer_frame, GOvalScaleModeFitCircle,
			                     DEG_TO_TRIGANGLE(angle_from));
	GPoint mark_to = gpoint_from_polar(inner_frame, GOvalScaleModeFitCircle,
		                          DEG_TO_TRIGANGLE(angle_from));
	graphics_draw_line(ctx, mark_from, mark_to);
    }
    // hour dial center
    GRect hour_frame = (GRect) { .size = GSize(w*.46, h*.46) };
    grect_align(&hour_frame, &layer_bounds, GAlignCenter, false);
    hour_frame.origin.x = hour_frame.origin.x + 1;
    hour_frame.origin.y = hour_frame.origin.y - (font_is_square() ? 1 : 0);
    int hour;
    char s_hour_string[5];
    graphics_context_set_text_color(ctx, enamel_get_clock_fg_color());
    for (hour = 12; hour > 0; hour = hour-1) {
        int hour_angle = hour * 30;
        snprintf(s_hour_string, sizeof(s_hour_string), "%d", hour);
        GSize hour_size = graphics_text_layout_get_content_size(s_hour_string, digital_font,
                                                                layer_bounds,
                                                                GTextOverflowModeFill,
                                                                GTextAlignmentCenter);
        GRect hour_box = grect_centered_from_polar(hour_frame, GOvalScaleModeFitCircle,
                                                   DEG_TO_TRIGANGLE(hour_angle), hour_size);
        graphics_draw_text(ctx, s_hour_string, digital_font, hour_box,
                           GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    // outline around hour dial
    if (enamel_get_draw_hour_circle()) {
	int radius = h*.26;
        GPoint center_point = GPoint(w*.5, h*.5);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_context_set_stroke_color(ctx, enamel_get_clock_fg_color());
        graphics_draw_circle(ctx, center_point, radius < 35 ? 35 : radius);
    }
}

static void draw_clock(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_unobstructed_bounds(layer);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    GPoint center_point = GPoint(w*.5, h*.5);
    // minute hand
    GRect slim_min_frame = (GRect) { .size = GSize(w*.19, h*.19) };
    grect_align(&slim_min_frame, &layer_bounds, GAlignCenter, false);
    GRect min_frame = (GRect) { .size = GSize(w*.92, h*.92) };
    grect_align(&min_frame, &layer_bounds, GAlignCenter, false);
    GPoint slim_min_to = gpoint_from_polar(slim_min_frame, GOvalScaleModeFitCircle,
		                           DEG_TO_TRIGANGLE(clock_state.minute_angle));
    GPoint min_from = slim_min_to;
    GPoint min_to = gpoint_from_polar(min_frame, GOvalScaleModeFitCircle,
                                      DEG_TO_TRIGANGLE(clock_state.minute_angle));
    // draw minute hand outline
    graphics_context_set_stroke_color(ctx, enamel_get_clock_bg_color());
    graphics_context_set_stroke_width(ctx, 5);
    //graphics_draw_line(ctx, center_point, slim_min_to);
    graphics_draw_line(ctx, center_point, min_to);
    graphics_context_set_stroke_width(ctx, 9);
    graphics_draw_line(ctx, min_from, min_to);
    // draw minute hand
    graphics_context_set_stroke_color(ctx, enamel_get_minute_hand_color());
    graphics_context_set_stroke_width(ctx, 3);
    //graphics_draw_line(ctx, center_point, slim_min_to);
    graphics_draw_line(ctx, center_point, min_to);
    graphics_context_set_stroke_width(ctx, 7);
    graphics_draw_line(ctx, min_from, min_to);
    // hour hand
    GRect hour_frame = (GRect) { .size = GSize(w*.5, h*.5) };
    grect_align(&hour_frame, &layer_bounds, GAlignCenter, false);
    GPoint slim_hour_to = gpoint_from_polar(slim_min_frame, GOvalScaleModeFitCircle,
                                            DEG_TO_TRIGANGLE(clock_state.hour_angle));
    GPoint hour_from = slim_hour_to;
    GPoint hour_to = gpoint_from_polar(hour_frame, GOvalScaleModeFitCircle,
                                       DEG_TO_TRIGANGLE(clock_state.hour_angle));
    // draw hour hand outline
    graphics_context_set_stroke_color(ctx, enamel_get_clock_bg_color());
    graphics_context_set_stroke_width(ctx, 5);
    if (!enamel_get_display_seconds() || battery_saver_enabled(clock_state.hour)) {
        graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
        graphics_fill_circle(ctx, center_point, 4);
    }
    //graphics_draw_line(ctx, center_point, slim_hour_to);
    graphics_draw_line(ctx, center_point, hour_to);
    graphics_context_set_stroke_width(ctx, 9);
    graphics_draw_line(ctx, hour_from, hour_to);
    // draw hour hand
    graphics_context_set_stroke_color(ctx, enamel_get_hour_hand_color());
    graphics_context_set_stroke_width(ctx, 3);
    //graphics_draw_line(ctx, center_point, slim_hour_to);
    graphics_draw_line(ctx, center_point, hour_to);
    graphics_context_set_stroke_width(ctx, 7);
    graphics_draw_line(ctx, hour_from, hour_to);
    if (!enamel_get_display_seconds() || battery_saver_enabled(clock_state.hour)) {
        graphics_context_set_fill_color(ctx, enamel_get_hour_hand_color());
        graphics_fill_circle(ctx, center_point, 3);
        graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
        graphics_fill_circle(ctx, center_point, 1);
    }
}

static void prv_app_did_focus(bool did_focus) {
  if (!did_focus) {
    return;
  }
  app_focus_service_unsubscribe();
  watch_model_init();
  watch_model_start_intro(clock_state);
}

int start_angle(int hour) {
  if (enamel_get_intro_enabled() && !battery_saver_enabled(hour)) {
    //int angles[] = { 45, 90, 135, 180, 225, 270, 315, 360 };
    int angles[] = { 180, 225, 270, 315, 360 };
    int direction = rand()%2 ? 1 : -1;
    //int angle = angles[rand()%8];
    int angle = angles[rand()%5];
    return angle * direction;
  }
  else
    return 0;
}

static void window_load(Window *window) {
  time_t tm = time(NULL);
  struct tm *tick_time = localtime(&tm);
  clock_state = (ClockState) {
    .minute_angle = tick_time->tm_min * 6 + start_angle(tick_time->tm_hour),
    .hour_angle = tick_time->tm_hour%12 * 30 + (tick_time->tm_min*6)*.08 + start_angle(tick_time->tm_hour),
    .second_angle = tick_time->tm_sec * 6 + start_angle(tick_time->tm_hour),
    .date = (enamel_get_intro_enabled() && !battery_saver_enabled(tick_time->tm_hour)) ? 0 : tick_time->tm_mday,
    .hour = tick_time->tm_hour
  };
  Layer *const window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  // minutes and hours marks layer
  marks_layer = layer_create(bounds);
  layer_set_update_proc(marks_layer, draw_marks);
  layer_add_child(window_layer, marks_layer);
  // day layer
  day_layer = layer_create(bounds);
  layer_set_update_proc(day_layer, draw_day);
  layer_add_child(window_layer, day_layer);
  // clock layer (hour an minute hands)
  clock_layer = layer_create(bounds);
  layer_set_update_proc(clock_layer, draw_clock);
  layer_add_child(window_layer, clock_layer);
  // seconds layer
  seconds_layer = layer_create(bounds);
  layer_set_update_proc(seconds_layer, draw_seconds);
  layer_add_child(window_layer, seconds_layer);
  // load font
  digital_font = fonts_load_custom_font(get_font_handle());
}

static void window_unload(Window *window) {
  fonts_unload_custom_font(digital_font);
  layer_destroy(clock_layer);
  layer_destroy(seconds_layer);
}

void accel_tap_handler(AccelAxisType axis, int32_t direction) {
    schedule_tap_animation(clock_state);
}

static void init(void) {
  enamel_init(0, 0);
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true /* animated */);
  app_focus_service_subscribe_handlers((AppFocusHandlers) {
    .did_focus = prv_app_did_focus,
  });
  events_app_message_open();
}

static void deinit(void) {
  enamel_deinit();
  watch_model_deinit();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
