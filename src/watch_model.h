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

#include <pebble.h>

#define CLOCK_ANIMATION_DELAY 0

typedef struct {
  int32_t minute_angle;
  int32_t hour_angle;
  int32_t second_angle;
  int date;
  int hour;
} ClockState;

void watch_model_start_intro(ClockState start_state);
void watch_model_init(void);
void watch_model_deinit(void);

void watch_model_handle_clock_change(ClockState state);
void watch_model_handle_time_change(struct tm *tick_time);
void watch_model_handle_seconds_change(struct tm *tick_time);
void watch_model_handle_config_change(void);
void schedule_tap_animation(ClockState current_state);
void accel_tap_handler(AccelAxisType axis, int32_t direction);
void update_subscriptions(int hour);
bool battery_saver_enabled(int hour);
