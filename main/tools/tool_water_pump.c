#include "tools/tool_water_pump.h"
#include "tools/gpio_policy.h"

#include "driver/gpio.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define WATER_PUMP_DEFAULT_GPIO 44

static bool state_is_on(const char *state)
{
    return state &&
           (strcasecmp(state, "on") == 0 ||
            strcasecmp(state, "1") == 0 ||
            strcasecmp(state, "true") == 0);
}

static bool state_is_off(const char *state)
{
    return state &&
           (strcasecmp(state, "off") == 0 ||
            strcasecmp(state, "0") == 0 ||
            strcasecmp(state, "false") == 0);
}

esp_err_t tool_water_pump_control_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *pin_obj = cJSON_GetObjectItem(root, "pin");
    const char *state = cJSON_GetStringValue(cJSON_GetObjectItem(root, "state"));
    if (!state_is_on(state) && !state_is_off(state)) {
        snprintf(output, output_size, "Error: 'state' required ('on' or 'off')");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int pin = cJSON_IsNumber(pin_obj) ? (int)pin_obj->valuedouble : WATER_PUMP_DEFAULT_GPIO;
    if (!gpio_policy_pin_is_allowed(pin)) {
        if (gpio_policy_pin_forbidden_hint(pin, output, output_size)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(output, output_size, "Error: pin %d is not allowed", pin);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *active_low_obj = cJSON_GetObjectItem(root, "active_low");
    bool active_low = active_low_obj ? cJSON_IsTrue(active_low_obj) : true;
    bool turn_on = state_is_on(state);
    int level = active_low ? (turn_on ? 0 : 1) : (turn_on ? 1 : 0);

    esp_err_t err = gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    if (err == ESP_OK) {
        err = gpio_set_level(pin, level);
    }
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: failed to set water pump on pin %d: %s",
                 pin, esp_err_to_name(err));
        cJSON_Delete(root);
        return err;
    }

    snprintf(output, output_size, "Water pump on GPIO%d is %s (GPIO level=%s, active_low=%s)",
             pin, turn_on ? "ON" : "OFF", level ? "HIGH" : "LOW",
             active_low ? "true" : "false");
    cJSON_Delete(root);
    return ESP_OK;
}
