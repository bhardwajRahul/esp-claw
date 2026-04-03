/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cap_time.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "claw_cap.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "cap_time";

#define CAP_TIME_SOURCE_URL     "https://api.telegram.org/"
#define CAP_TIME_HTTP_TIMEOUT_MS 10000

static const char *s_month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

typedef struct {
    char timezone[64];
    char date_header[64];
} cap_time_state_t;

static cap_time_state_t s_time = {
    .timezone = "UTC0",
};

static int cap_time_month_to_index(const char *month)
{
    size_t i;

    if (!month) {
        return -1;
    }

    for (i = 0; i < sizeof(s_month_names) / sizeof(s_month_names[0]); i++) {
        if (strcmp(month, s_month_names[i]) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool cap_time_parse_and_set_clock(const char *date_header,
                                         char *output,
                                         size_t output_size)
{
    int day = 0;
    int year = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int month = -1;
    char month_name[4] = {0};
    struct tm utc_tm = {0};
    struct timeval tv = {0};
    struct tm local_tm = {0};
    time_t epoch;

    if (!date_header || !output || output_size == 0) {
        return false;
    }

    if (sscanf(date_header,
               "%*[^,], %d %3s %d %d:%d:%d",
               &day,
               month_name,
               &year,
               &hour,
               &minute,
               &second) != 6) {
        return false;
    }

    month = cap_time_month_to_index(month_name);
    if (month < 0) {
        return false;
    }

    utc_tm.tm_mday = day;
    utc_tm.tm_mon = month;
    utc_tm.tm_year = year - 1900;
    utc_tm.tm_hour = hour;
    utc_tm.tm_min = minute;
    utc_tm.tm_sec = second;

    setenv("TZ", "UTC0", 1);
    tzset();
    epoch = mktime(&utc_tm);
    if (epoch < 0) {
        return false;
    }

    tv.tv_sec = epoch;
    if (settimeofday(&tv, NULL) != 0) {
        return false;
    }

    setenv("TZ", s_time.timezone[0] ? s_time.timezone : "UTC0", 1);
    tzset();
    localtime_r(&epoch, &local_tm);
    if (strftime(output, output_size, "%Y-%m-%d %H:%M:%S %Z (%A)", &local_tm) == 0) {
        return false;
    }

    return true;
}

static esp_err_t cap_time_http_event_handler(esp_http_client_event_t *event)
{
    if (!event || !event->user_data) {
        return ESP_OK;
    }

    if (event->event_id == HTTP_EVENT_ON_HEADER &&
            event->header_key &&
            event->header_value &&
            strcasecmp(event->header_key, "Date") == 0) {
        cap_time_state_t *state = (cap_time_state_t *)event->user_data;

        strlcpy(state->date_header, event->header_value, sizeof(state->date_header));
    }

    return ESP_OK;
}

static esp_err_t cap_time_fetch_current(char *output, size_t output_size)
{
    esp_http_client_config_t config = {
        .url = CAP_TIME_SOURCE_URL,
        .method = HTTP_METHOD_HEAD,
        .timeout_ms = CAP_TIME_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = cap_time_http_event_handler,
        .user_data = &s_time,
    };
    esp_http_client_handle_t client = NULL;
    esp_err_t err;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_time.date_header[0] = '\0';
    client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        return err;
    }

    if (s_time.date_header[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    if (!cap_time_parse_and_set_clock(s_time.date_header, output, output_size)) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t cap_time_execute(const char *input_json,
                                  const claw_cap_call_context_t *ctx,
                                  char *output,
                                  size_t output_size)
{
    esp_err_t err;

    (void)input_json;
    (void)ctx;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    err = cap_time_fetch_current(output, output_size);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: failed to fetch time (%s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", output);
        return err;
    }

    ESP_LOGI(TAG, "Time synced: %s", output);
    return ESP_OK;
}

static const claw_cap_descriptor_t s_time_descriptors[] = {
    {
        .id = "get_current_time",
        .name = "get_current_time",
        .family = "system",
        .description = "Fetch current network time, update the local clock, and return formatted local time.",
        .kind = CLAW_CAP_KIND_CALLABLE,
        .cap_flags = CLAW_CAP_FLAG_CALLABLE_BY_LLM,
        .input_schema_json = "{\"type\":\"object\"}",
        .execute = cap_time_execute,
    },
};

static const claw_cap_group_t s_time_group = {
    .group_id = "cap_time",
    .descriptors = s_time_descriptors,
    .descriptor_count = sizeof(s_time_descriptors) / sizeof(s_time_descriptors[0]),
};

esp_err_t cap_time_register_group(void)
{
    if (claw_cap_group_exists(s_time_group.group_id)) {
        return ESP_OK;
    }

    return claw_cap_register_group(&s_time_group);
}

esp_err_t cap_time_set_timezone(const char *timezone)
{
    if (!timezone || !timezone[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(s_time.timezone, timezone, sizeof(s_time.timezone));
    setenv("TZ", s_time.timezone, 1);
    tzset();
    return ESP_OK;
}
