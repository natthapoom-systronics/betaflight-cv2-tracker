/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "pg/pg.h"

typedef struct cvTrackerConfig_s {
    uint8_t  enabled;            // 0 = disabled, 1 = enabled
    uint8_t  p_gain;             // Proportional gain 0-100 (scale: gain/100 * GIMBAL_MAX)
    uint16_t frame_width;        // Camera frame width in pixels (default 320)
    uint16_t frame_height;       // Camera frame height in pixels (default 240)
    uint16_t timeout_ms;         // Target lost timeout in ms (default 500)
    uint8_t  invert_pitch;       // Invert pitch error direction
    uint8_t  invert_yaw;         // Invert yaw error direction
    uint8_t  drone_follow;       // 1 = tilt drone to follow target, 0 = gimbal only
    uint8_t  drone_max_angle;    // Max lean angle in degrees when following (default 15)
    uint8_t  drone_p_gain;       // P gain for drone tilt 0-100 (default 30)
} cvTrackerConfig_t;

PG_DECLARE(cvTrackerConfig_t, cvTrackerConfig);
