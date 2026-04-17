#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Control a water pump through a GPIO relay.
 * Input JSON: {"pin": <int optional, default 44>, "state": "on"|"off", "active_low": <bool optional>}
 */
esp_err_t tool_water_pump_control_execute(const char *input_json, char *output, size_t output_size);
