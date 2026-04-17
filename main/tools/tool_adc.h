#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Initialize ADC tool state.
 */
esp_err_t tool_adc_init(void);

/**
 * Read a raw ADC value from an ADC-capable GPIO pin.
 */
esp_err_t tool_adc_read_raw(int pin, int *raw);

/**
 * Read an analog GPIO pin through ADC.
 * Input JSON: {"pin": <int>, "dry_raw": <int optional>, "wet_raw": <int optional>}
 */
esp_err_t tool_adc_read_execute(const char *input_json, char *output, size_t output_size);
