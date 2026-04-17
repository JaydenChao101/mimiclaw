#include "tools/tool_camera.h"
#include "channels/telegram/telegram_bot.h"

#include "esp_camera.h"
#include "esp_log.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdbool.h>

static const char *TAG = "tool_camera";

/* Seeed Studio XIAO ESP32S3 Sense camera pin map. */
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39
#define CAM_PIN_D0      15  /* Y2 */
#define CAM_PIN_D1      17  /* Y3 */
#define CAM_PIN_D2      18  /* Y4 */
#define CAM_PIN_D3      16  /* Y5 */
#define CAM_PIN_D4      14  /* Y6 */
#define CAM_PIN_D5      12  /* Y7 */
#define CAM_PIN_D6      11  /* Y8 */
#define CAM_PIN_D7      48  /* Y9 */
#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

#define CAM_DEFAULT_WB_MODE       3  /* 0 auto, 1 sunny, 2 cloudy, 3 office, 4 home */
#define CAM_DEFAULT_WARMUP_FRAMES 2
#define CAM_MAX_WARMUP_FRAMES     5

static bool s_camera_ready;

static int json_int_or_default(cJSON *root, const char *name, int default_value)
{
    cJSON *item = cJSON_GetObjectItem(root, name);
    return cJSON_IsNumber(item) ? item->valueint : default_value;
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void camera_apply_tuning(int wb_mode, int brightness, int contrast, int saturation)
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        ESP_LOGW(TAG, "Camera sensor handle unavailable; skipping tuning");
        return;
    }

    wb_mode = clamp_int(wb_mode, 0, 4);
    brightness = clamp_int(brightness, -2, 2);
    contrast = clamp_int(contrast, -2, 2);
    saturation = clamp_int(saturation, -2, 2);

    if (sensor->set_whitebal) {
        sensor->set_whitebal(sensor, 1);
    }
    if (sensor->set_awb_gain) {
        sensor->set_awb_gain(sensor, 1);
    }
    if (sensor->set_wb_mode) {
        sensor->set_wb_mode(sensor, wb_mode);
    }
    if (sensor->set_brightness) {
        sensor->set_brightness(sensor, brightness);
    }
    if (sensor->set_contrast) {
        sensor->set_contrast(sensor, contrast);
    }
    if (sensor->set_saturation) {
        sensor->set_saturation(sensor, saturation);
    }
    if (sensor->set_lenc) {
        sensor->set_lenc(sensor, 1);
    }

    ESP_LOGI(TAG, "Camera tuning: wb_mode=%d brightness=%d contrast=%d saturation=%d",
             wb_mode, brightness, contrast, saturation);
}

static void camera_discard_warmup_frames(int frame_count)
{
    frame_count = clamp_int(frame_count, 0, CAM_MAX_WARMUP_FRAMES);
    for (int i = 0; i < frame_count; i++) {
        camera_fb_t *warmup = esp_camera_fb_get();
        if (!warmup) {
            ESP_LOGW(TAG, "Camera warmup frame %d failed", i + 1);
            return;
        }
        esp_camera_fb_return(warmup);
    }
}

static esp_err_t camera_init_once(void)
{
    if (s_camera_ready) {
        return ESP_OK;
    }

    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_camera_ready = true;
    camera_apply_tuning(CAM_DEFAULT_WB_MODE, 0, 0, 0);
    ESP_LOGI(TAG, "XIAO ESP32S3 Sense camera initialized");
    return ESP_OK;
}

esp_err_t tool_camera_send_photo_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json && input_json[0] ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *chat_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "chat_id"));
    const char *caption = cJSON_GetStringValue(cJSON_GetObjectItem(root, "caption"));
    if (!chat_id || !chat_id[0]) {
        snprintf(output, output_size, "Error: 'chat_id' required");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = camera_init_once();
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: camera init failed: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        return err;
    }

    int wb_mode = json_int_or_default(root, "wb_mode", CAM_DEFAULT_WB_MODE);
    int brightness = json_int_or_default(root, "brightness", 0);
    int contrast = json_int_or_default(root, "contrast", 0);
    int saturation = json_int_or_default(root, "saturation", 0);
    int warmup_frames = json_int_or_default(root, "warmup_frames", CAM_DEFAULT_WARMUP_FRAMES);
    camera_apply_tuning(wb_mode, brightness, contrast, saturation);
    camera_discard_warmup_frames(warmup_frames);

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        snprintf(output, output_size, "Error: camera capture failed");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    if (fb->format != PIXFORMAT_JPEG) {
        esp_camera_fb_return(fb);
        snprintf(output, output_size, "Error: camera frame is not JPEG");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = telegram_send_photo_jpeg(chat_id, fb->buf, fb->len, caption);
    size_t jpeg_len = fb->len;
    esp_camera_fb_return(fb);

    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: captured %d bytes but Telegram upload failed: %s",
                 (int)jpeg_len, esp_err_to_name(err));
        cJSON_Delete(root);
        return err;
    }

    snprintf(output, output_size,
             "Camera photo sent to Telegram chat %s (%d bytes, streamed from RAM, wb_mode=%d)",
             chat_id, (int)jpeg_len, clamp_int(wb_mode, 0, 4));
    cJSON_Delete(root);
    return ESP_OK;
}
