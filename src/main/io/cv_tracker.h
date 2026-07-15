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

#include <stdbool.h>
#include <stdint.h>

#include "drivers/time.h"

/*
 * CV Tracker — receives target centroid data from a smart camera module
 * (e.g. Pixy2, OpenMV, Huskylens) over a UART serial port and steers the
 * gimbal so the target is kept at the frame centre (crosshair/reticle).
 *
 * Serial protocol (11 bytes per frame):
 *   Byte 0   : 0xA5  (header 1)
 *   Byte 1   : 0x5A  (header 2)
 *   Bytes 2-3: uint16_t target_x   (big-endian, target centroid X in pixels)
 *   Bytes 4-5: uint16_t target_y   (big-endian, target centroid Y in pixels)
 *   Bytes 6-7: uint16_t frame_w    (big-endian, frame width  in pixels)
 *   Bytes 8-9: uint16_t frame_h    (big-endian, frame height in pixels)
 *   Byte 10  : uint8_t  checksum   (XOR of bytes 2..9)
 *
 * To enable, assign the serial port using:
 *   serial <portId> 2097152 115200 57600 0 115200
 *
 * CLI parameters (under feature cv_tracker):
 *   set cv_tracker_enabled      = ON
 *   set cv_tracker_p_gain       = 30        (0-100)
 *   set cv_tracker_frame_width  = 320
 *   set cv_tracker_frame_height = 240
 *   set cv_tracker_timeout_ms   = 500
 *   set cv_tracker_invert_pitch = OFF
 *   set cv_tracker_invert_yaw   = OFF
 */

typedef enum {
    CV_TRACKER_STATE_IDLE    = 0,
    CV_TRACKER_STATE_LOCKED,
    CV_TRACKER_STATE_LOST,
} cvTrackerState_e;

typedef struct {
    cvTrackerState_e state;
    int16_t          error_x;   // signed pixel error from frame centre (positive = right)
    int16_t          error_y;   // signed pixel error from frame centre (positive = down)
    int16_t          gimbal_yaw;
    int16_t          gimbal_pitch;
    timeMs_t         last_seen_ms;
} cvTrackerStatus_t;

bool                       cvTrackerInit(void);
void                       cvTrackerUpdate(timeUs_t currentTimeUs);
const cvTrackerStatus_t   *cvTrackerGetStatus(void);
