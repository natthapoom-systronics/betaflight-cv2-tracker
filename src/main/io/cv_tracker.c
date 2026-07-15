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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#ifdef USE_CV_TRACKER

#include "build/debug.h"

#include "common/maths.h"

#include "drivers/serial.h"
#include "drivers/time.h"

#include "fc/runtime_config.h"

#include "io/cv_tracker.h"
#include "io/serial.h"

#include "io/gimbal_control.h"

#include "flight/autopilot.h"

#include "pg/cv_tracker.h"

#include "scheduler/scheduler.h"

/* -------------------------------------------------------------------------
 * Protocol constants
 * ---------------------------------------------------------------------- */

#define CV_TRACKER_HEADER_1      0xA5
#define CV_TRACKER_HEADER_2      0x5A
#define CV_TRACKER_FRAME_LEN     11   // 2 header + 8 payload + 1 checksum

/* Gimbal output range mirrors gimbal_control.c definitions */
#define CV_GIMBAL_PITCH_MAX      1750
#define CV_GIMBAL_PITCH_MIN     -1150
#define CV_GIMBAL_YAW_MAX        2047
#define CV_GIMBAL_YAW_MIN       -2047

/* -------------------------------------------------------------------------
 * Parser states
 * ---------------------------------------------------------------------- */

typedef enum {
    PARSE_HEADER_1 = 0,
    PARSE_HEADER_2,
    PARSE_PAYLOAD,
    PARSE_CHECKSUM,
} parseState_e;

/* -------------------------------------------------------------------------
 * Module-local state
 * ---------------------------------------------------------------------- */

static serialPort_t  *cvTrackerPort  = NULL;
static cvTrackerStatus_t cvStatus;

static parseState_e   parseState     = PARSE_HEADER_1;
static uint8_t        parseBuf[8];   // 8 payload bytes
static uint8_t        parseBufIdx    = 0;

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */

static void processFrame(const uint8_t *payload);

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

bool cvTrackerInit(void)
{
    if (!cvTrackerConfig()->enabled) {
        return false;
    }

    const serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_CV_TRACKER);
    if (!portConfig) {
        return false;
    }

    cvTrackerPort = openSerialPort(
        portConfig->identifier,
        FUNCTION_CV_TRACKER,
        NULL, NULL,
        115200,
        MODE_RX,
        SERIAL_NOT_INVERTED
    );

    if (!cvTrackerPort) {
        return false;
    }

    memset(&cvStatus, 0, sizeof(cvStatus));
    cvStatus.state = CV_TRACKER_STATE_IDLE;

    setTaskEnabled(TASK_CV_TRACKER, true);

    return true;
}

void cvTrackerUpdate(timeUs_t currentTimeUs)
{
    UNUSED(currentTimeUs);

    if (!cvTrackerPort) {
        setTaskEnabled(TASK_CV_TRACKER, false);
        return;
    }

    /* ---- 1. Drain serial ring buffer and parse frames ---- */
    uint32_t avail = serialRxBytesWaiting(cvTrackerPort);
    while (avail--) {
        uint8_t c = serialRead(cvTrackerPort);

        switch (parseState) {
        case PARSE_HEADER_1:
            if (c == CV_TRACKER_HEADER_1) {
                parseState = PARSE_HEADER_2;
            }
            break;

        case PARSE_HEADER_2:
            if (c == CV_TRACKER_HEADER_2) {
                parseBufIdx = 0;
                parseState  = PARSE_PAYLOAD;
            } else {
                parseState = PARSE_HEADER_1;
            }
            break;

        case PARSE_PAYLOAD:
            parseBuf[parseBufIdx++] = c;
            if (parseBufIdx == 8) {
                parseState = PARSE_CHECKSUM;
            }
            break;

        case PARSE_CHECKSUM: {
            uint8_t expected = 0;
            for (int i = 0; i < 8; i++) {
                expected ^= parseBuf[i];
            }
            if (c == expected) {
                processFrame(parseBuf);
            }
            parseState = PARSE_HEADER_1;
            break;
        }

        default:
            parseState = PARSE_HEADER_1;
            break;
        }
    }

    /* ---- 2. Handle target-lost timeout ---- */
    const timeMs_t nowMs = millis();
    if (cvStatus.state == CV_TRACKER_STATE_LOCKED) {
        if ((nowMs - cvStatus.last_seen_ms) > cvTrackerConfig()->timeout_ms) {
            cvStatus.state = CV_TRACKER_STATE_LOST;
        }
    }

    /* ---- 4. Drive drone follow (angle mode) ---- */
    if (cvTrackerConfig()->drone_follow && ARMING_FLAG(ARMED)) {
        const bool tracking = (cvStatus.state == CV_TRACKER_STATE_LOCKED);
        /* Enable / disable CV_TRACKER_MODE flight mode based on lock state */
        if (tracking) {
            ENABLE_FLIGHT_MODE(CV_TRACKER_MODE);
        } else {
            DISABLE_FLIGHT_MODE(CV_TRACKER_MODE);
        }
        /* Drone lean angles are set inside processFrame() below, or zeroed here on loss */
        if (!tracking) {
            cvTrackerSetDroneAngles(0.0f, 0.0f, false);
        }
    } else {
        DISABLE_FLIGHT_MODE(CV_TRACKER_MODE);
        cvTrackerSetDroneAngles(0.0f, 0.0f, false);
    }

    DEBUG_SET(DEBUG_CV_TRACKER, 0, cvStatus.state);
    DEBUG_SET(DEBUG_CV_TRACKER, 1, cvStatus.error_x);
    DEBUG_SET(DEBUG_CV_TRACKER, 2, cvStatus.error_y);
    DEBUG_SET(DEBUG_CV_TRACKER, 3, cvStatus.gimbal_yaw);
}

const cvTrackerStatus_t *cvTrackerGetStatus(void)
{
    return &cvStatus;
}

/* -------------------------------------------------------------------------
 * Private helpers
 * ---------------------------------------------------------------------- */

/*
 * processFrame — decode a validated 8-byte payload and drive the gimbal.
 *
 * Payload layout (all big-endian uint16):
 *   [0..1]  target_x
 *   [2..3]  target_y
 *   [4..5]  frame_w
 *   [6..7]  frame_h
 */
static void processFrame(const uint8_t *p)
{
    const uint16_t target_x = ((uint16_t)p[0] << 8) | p[1];
    const uint16_t target_y = ((uint16_t)p[2] << 8) | p[3];
    const uint16_t frame_w  = ((uint16_t)p[4] << 8) | p[5];
    const uint16_t frame_h  = ((uint16_t)p[6] << 8) | p[7];

    /* Use configured defaults when camera does not supply dimensions */
    const uint16_t fw = frame_w  ? frame_w  : cvTrackerConfig()->frame_width;
    const uint16_t fh = frame_h  ? frame_h  : cvTrackerConfig()->frame_height;

    if (fw == 0 || fh == 0) {
        return;
    }

    /* Signed pixel error from frame centre (crosshair/reticle = centre) */
    cvStatus.error_x = (int16_t)target_x - (int16_t)(fw / 2);
    cvStatus.error_y = (int16_t)target_y - (int16_t)(fh / 2);
    cvStatus.last_seen_ms = millis();
    cvStatus.state = CV_TRACKER_STATE_LOCKED;

    /*
     * Proportional control:
     *   gimbal_cmd = (error / half_frame) * gain/100 * gimbal_max
     *
     * error_x → YAW  (positive error = target right of centre → yaw right)
     * error_y → PITCH (positive error = target below centre  → pitch down)
     */
    const int16_t half_w = (int16_t)(fw / 2);
    const int16_t half_h = (int16_t)(fh / 2);
    const int16_t gain   = cvTrackerConfig()->p_gain;

    int16_t yaw_cmd = (int16_t)(
        ((int32_t)cvStatus.error_x * CV_GIMBAL_YAW_MAX * gain) /
        ((int32_t)half_w * 100)
    );

    int16_t pitch_cmd = (int16_t)(
        ((int32_t)cvStatus.error_y * CV_GIMBAL_PITCH_MAX * gain) /
        ((int32_t)half_h * 100)
    );

    /* Optionally invert axes */
    if (cvTrackerConfig()->invert_yaw) {
        yaw_cmd   = -yaw_cmd;
    }
    if (cvTrackerConfig()->invert_pitch) {
        pitch_cmd = -pitch_cmd;
    }

    /* Clamp to gimbal limits */
    cvStatus.gimbal_yaw   = constrain(yaw_cmd,   CV_GIMBAL_YAW_MIN,   CV_GIMBAL_YAW_MAX);
    cvStatus.gimbal_pitch = constrain(pitch_cmd, CV_GIMBAL_PITCH_MIN, CV_GIMBAL_PITCH_MAX);

    /* Feed into gimbal controller */
#ifdef USE_GIMBAL
    gimbalSetCvTrackerInput(cvStatus.gimbal_pitch, cvStatus.gimbal_yaw);
#endif

    /* ---- Drone follow: compute lean angles from pixel error ---- */
    if (cvTrackerConfig()->drone_follow) {
        const float max_angle  = (float)cvTrackerConfig()->drone_max_angle;
        const float drone_gain = (float)cvTrackerConfig()->drone_p_gain;

        /* Normalised error: -1.0 .. +1.0 */
        const float norm_x = constrainf((float)cvStatus.error_x / (float)half_w, -1.0f, 1.0f);
        const float norm_y = constrainf((float)cvStatus.error_y / (float)half_h, -1.0f, 1.0f);

        /* Proportional lean angle — positive norm_x (target right) → roll right (+)
         *                          positive norm_y (target below) → pitch forward (+) */
        float drone_roll  = norm_x * max_angle * drone_gain * 0.01f;
        float drone_pitch = norm_y * max_angle * drone_gain * 0.01f;

        if (cvTrackerConfig()->invert_yaw) {
            drone_roll = -drone_roll;
        }
        if (cvTrackerConfig()->invert_pitch) {
            drone_pitch = -drone_pitch;
        }

        drone_roll  = constrainf(drone_roll,  -max_angle, max_angle);
        drone_pitch = constrainf(drone_pitch, -max_angle, max_angle);

        cvTrackerSetDroneAngles(drone_roll, drone_pitch, true);
    }
}

#endif // USE_CV_TRACKER
