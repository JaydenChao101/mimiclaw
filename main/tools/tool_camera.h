#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Capture a JPEG frame from the XIAO ESP32S3 Sense camera and send it to Telegram.
 * The frame stays in RAM and is not written to SPIFFS.
 * Input JSON: {"chat_id": "<id>", "caption": "<optional>"}
 */
esp_err_t tool_camera_send_photo_execute(const char *input_json, char *output, size_t output_size);
