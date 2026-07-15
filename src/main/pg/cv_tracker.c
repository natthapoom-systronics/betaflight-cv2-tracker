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

#include "platform.h"

#ifdef USE_CV_TRACKER

#include "pg/pg.h"
#include "pg/pg_ids.h"

#include "cv_tracker.h"

PG_REGISTER_WITH_RESET_TEMPLATE(cvTrackerConfig_t, cvTrackerConfig, PG_CV_TRACKER_CONFIG, 0);

PG_RESET_TEMPLATE(cvTrackerConfig_t, cvTrackerConfig,
    .enabled       = 0,
    .p_gain        = 30,
    .frame_width   = 320,
    .frame_height  = 240,
    .timeout_ms    = 500,
    .invert_pitch  = 0,
    .invert_yaw    = 0,
    .drone_follow  = 0,
    .drone_max_angle = 15,
    .drone_p_gain  = 30,
);

#endif // USE_CV_TRACKER
