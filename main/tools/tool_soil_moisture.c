#include "tools/tool_soil_moisture.h"
#include "tools/tool_adc.h"
#include "channels/telegram/telegram_bot.h"

#include "cJSON.h"

#include <stdio.h>

#define SOIL_MOISTURE_GPIO       7
#define SOIL_MOISTURE_DRY_RAW    3200
#define SOIL_MOISTURE_WET_RAW    1200

static int clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

static const char *soil_status_text(int moisture)
{
    if (moisture < 25) {
        return "偏乾，建議盡快淋水。";
    }
    if (moisture < 45) {
        return "有少少乾，可以留意或者少量補水。";
    }
    if (moisture < 75) {
        return "濕度正常，盆栽狀態幾穩定。";
    }
    return "偏濕，暫時唔建議再淋水。";
}

esp_err_t tool_soil_moisture_report_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json && input_json[0] ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    int dry_raw = SOIL_MOISTURE_DRY_RAW;
    int wet_raw = SOIL_MOISTURE_WET_RAW;
    cJSON *dry_obj = cJSON_GetObjectItem(root, "dry_raw");
    cJSON *wet_obj = cJSON_GetObjectItem(root, "wet_raw");
    if (cJSON_IsNumber(dry_obj)) {
        dry_raw = (int)dry_obj->valuedouble;
    }
    if (cJSON_IsNumber(wet_obj)) {
        wet_raw = (int)wet_obj->valuedouble;
    }

    if (dry_raw == wet_raw) {
        snprintf(output, output_size, "Error: dry_raw and wet_raw must be different");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int raw = 0;
    esp_err_t err = tool_adc_read_raw(SOIL_MOISTURE_GPIO, &raw);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: failed to read soil sensor on GPIO%d: %s",
                 SOIL_MOISTURE_GPIO, esp_err_to_name(err));
        cJSON_Delete(root);
        return err;
    }

    int moisture = (dry_raw - raw) * 100 / (dry_raw - wet_raw);
    moisture = clamp_percent(moisture);

    char message[512];
    snprintf(message, sizeof(message),
             "盆栽而家泥土濕度大約係 %d%%。%s\n\n"
             "讀數：GPIO%d raw=%d，校準 dry_raw=%d，wet_raw=%d。",
             moisture, soil_status_text(moisture), SOIL_MOISTURE_GPIO,
             raw, dry_raw, wet_raw);

    const char *chat_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "chat_id"));
    if (chat_id && chat_id[0]) {
        err = telegram_send_message(chat_id, message);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: soil moisture read OK but Telegram send failed: %s\n%s",
                     esp_err_to_name(err), message);
            cJSON_Delete(root);
            return err;
        }
        snprintf(output, output_size, "Telegram soil moisture report sent to %s\n%s", chat_id, message);
    } else {
        snprintf(output, output_size, "%s", message);
    }

    cJSON_Delete(root);
    return ESP_OK;
}
