#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Read the soil moisture sensor connected to GPIO7 and optionally send
 * a Telegram explanation.
 * Input JSON: {"chat_id": "<optional>", "dry_raw": <optional>, "wet_raw": <optional>}
 */
esp_err_t tool_soil_moisture_report_execute(const char *input_json, char *output, size_t output_size);
