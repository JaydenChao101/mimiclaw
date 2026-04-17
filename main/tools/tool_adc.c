#include "tools/tool_adc.h"
#include "tools/gpio_policy.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "cJSON.h"

#include <stdio.h>

static const char *TAG = "tool_adc";

static adc_oneshot_unit_handle_t s_adc1_handle;
static adc_oneshot_unit_handle_t s_adc2_handle;

static esp_err_t get_unit_handle(adc_unit_t unit, adc_oneshot_unit_handle_t *handle)
{
    adc_oneshot_unit_handle_t *slot;

    if (unit == ADC_UNIT_1) {
        slot = &s_adc1_handle;
    } else if (unit == ADC_UNIT_2) {
        slot = &s_adc2_handle;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if (*slot) {
        *handle = *slot;
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = unit,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, slot);
    if (err != ESP_OK) {
        return err;
    }

    *handle = *slot;
    return ESP_OK;
}

static int clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

esp_err_t tool_adc_init(void)
{
    ESP_LOGI(TAG, "ADC tool initialized");
    return ESP_OK;
}

esp_err_t tool_adc_read_raw(int pin, int *raw)
{
    if (!raw) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!gpio_policy_pin_is_allowed(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t err = adc_oneshot_io_to_channel(pin, &unit, &channel);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_unit_handle_t handle;
    err = get_unit_handle(unit, &handle);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    err = adc_oneshot_config_channel(handle, channel, &config);
    if (err != ESP_OK) {
        return err;
    }

    err = adc_oneshot_read(handle, channel, raw);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "adc_read: GPIO%d unit=%d channel=%d raw=%d",
             pin, (int)unit, (int)channel, *raw);
    return ESP_OK;
}

esp_err_t tool_adc_read_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *pin_obj = cJSON_GetObjectItem(root, "pin");
    if (!cJSON_IsNumber(pin_obj)) {
        snprintf(output, output_size, "Error: 'pin' required (integer)");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int pin = (int)pin_obj->valuedouble;
    if (!gpio_policy_pin_is_allowed(pin)) {
        if (gpio_policy_pin_forbidden_hint(pin, output, output_size)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(output, output_size, "Error: pin %d is not allowed", pin);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int raw = 0;
    esp_err_t err = tool_adc_read_raw(pin, &raw);
    if (err != ESP_OK) {
        if (err == ESP_ERR_INVALID_ARG) {
            snprintf(output, output_size, "Error: GPIO%d is not ADC-capable or not allowed", pin);
        } else {
            snprintf(output, output_size, "Error: failed to read GPIO%d ADC: %s",
                     pin, esp_err_to_name(err));
        }
        cJSON_Delete(root);
        return err;
    }

    cJSON *dry_obj = cJSON_GetObjectItem(root, "dry_raw");
    cJSON *wet_obj = cJSON_GetObjectItem(root, "wet_raw");
    if (cJSON_IsNumber(dry_obj) && cJSON_IsNumber(wet_obj)) {
        int dry_raw = (int)dry_obj->valuedouble;
        int wet_raw = (int)wet_obj->valuedouble;
        if (dry_raw == wet_raw) {
            snprintf(output, output_size,
                     "GPIO%d ADC raw=%d (dry_raw and wet_raw must be different)",
                     pin, raw);
        } else {
            int moisture = (dry_raw - raw) * 100 / (dry_raw - wet_raw);
            moisture = clamp_percent(moisture);
            snprintf(output, output_size,
                     "GPIO%d ADC raw=%d, moisture=%d%% (dry_raw=%d, wet_raw=%d)",
                     pin, raw, moisture, dry_raw, wet_raw);
        }
    } else {
        snprintf(output, output_size, "GPIO%d ADC raw=%d", pin, raw);
    }

    cJSON_Delete(root);
    return ESP_OK;
}
