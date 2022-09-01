/*
This file is part of Hackflight.

Hackflight is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Hackflight is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Hackflight. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <core/constrain.h>
#include <esc.h>
#include <time.h>

#include "platform.h"
#include "dshot_dev.h"
#include "dshot_bitbang.h"
#include "dshot_command.h"
#include "escdev.h"
#include "systemdev.h"

#define CONVERT_PARAMETER_TO_PERCENT(param) (0.01f * param)

float escGetDigitalIdOffset(void)
{
    uint16_t digitalIdleOffsetValue = 450;
    return CONVERT_PARAMETER_TO_PERCENT(digitalIdleOffsetValue * 0.01f);
}

bool escDevIsProtocolDshot(void)
{
    return true;
}

bool escCheckProtocolEnabled(bool *isProtocolDshot)
{
    bool enabled = false;
    bool isDshot = false;

    switch (ESC_PROTOCOL) {
    case ESC_STANDARD:
    case ESC_ONESHOT125:
    case ESC_ONESHOT42:
    case ESC_MULTISHOT:
    case ESC_BRUSHED:
        enabled = true;

        break;

    case ESC_DSHOT150:
    case ESC_DSHOT300:
    case ESC_DSHOT600:
    case ESC_PROSHOT1000:
        enabled = true;
        isDshot = true;

        break;
    default:

        break;
    }

    if (isProtocolDshot) {
        *isProtocolDshot = isDshot;
    }

    return enabled;
}

uint32_t escGetEnableTimeMs(void * escDevice_void)
{
    escDevice_t * escDevice = (escDevice_t *)escDevice_void;

    return escDevice->enableTimeMs;
}
