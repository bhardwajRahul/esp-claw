/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cap_mcp_client_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/ip_addr.h"
#include "mdns.h"

static const char *TAG = "mcp_discover_core";

static const char *cap_mcp_find_txt_value(const mdns_result_t *result, const char *key)
{
    size_t i;

    if (!result || !key) {
        return NULL;
    }

    for (i = 0; i < result->txt_count; i++) {
        if (result->txt[i].key && strcmp(result->txt[i].key, key) == 0) {
            return result->txt[i].value;
        }
    }

    return NULL;
}

static const char *cap_mcp_pick_ip_string(const mdns_result_t *result,
                                          char *buf,
                                          size_t buf_size)
{
    const mdns_ip_addr_t *addr = NULL;

    if (!buf || buf_size == 0) {
        return NULL;
    }
    buf[0] = '\0';

    if (!result || !result->addr) {
        return NULL;
    }

    addr = result->addr;
    while (addr) {
        if (ipaddr_ntoa_r((const ip_addr_t *)&addr->addr, buf, buf_size)) {
            return buf;
        }
        addr = addr->next;
    }

    return NULL;
}

static esp_err_t cap_mcp_parse_discover_options(const char *input_json,
                                                int *timeout_ms,
                                                bool *include_self)
{
    cJSON *input = NULL;

    if (!timeout_ms || !include_self) {
        return ESP_ERR_INVALID_ARG;
    }
    *timeout_ms = CAP_MCP_DISCOVER_TIMEOUT_MS;
    *include_self = true;

    if (!input_json || !input_json[0]) {
        return ESP_OK;
    }

    input = cJSON_Parse(input_json);
    if (!input || !cJSON_IsObject(input)) {
        cJSON_Delete(input);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *timeout_item = cJSON_GetObjectItem(input, "timeout_ms");
    if (cJSON_IsNumber(timeout_item) && timeout_item->valueint > 0) {
        *timeout_ms = timeout_item->valueint;
    }

    cJSON *self_item = cJSON_GetObjectItem(input, "include_self");
    if (cJSON_IsBool(self_item)) {
        *include_self = cJSON_IsTrue(self_item);
    }

    cJSON_Delete(input);
    return ESP_OK;
}

esp_err_t cap_mcp_discover_services(const char *input_json, cJSON **result_out)
{
    int timeout_ms = CAP_MCP_DISCOVER_TIMEOUT_MS;
    bool include_self = true;
    mdns_result_t *results = NULL;
    cJSON *root = NULL;
    cJSON *devices = NULL;
    size_t found = 0;
    esp_err_t err;

    if (!result_out) {
        return ESP_ERR_INVALID_ARG;
    }
    *result_out = NULL;

    err = cap_mcp_parse_discover_options(input_json, &timeout_ms, &include_self);
    if (err != ESP_OK) {
        return err;
    }

    err = mdns_query_ptr(CAP_MCP_MDNS_SERVICE_TYPE,
                         CAP_MCP_MDNS_SERVICE_PROTO,
                         timeout_ms,
                         20,
                         &results);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_query_ptr failed: %s", esp_err_to_name(err));
        return err;
    }

    root = cJSON_CreateObject();
    devices = cJSON_CreateArray();
    if (!root || !devices) {
        mdns_query_results_free(results);
        cJSON_Delete(root);
        cJSON_Delete(devices);
        return ESP_ERR_NO_MEM;
    }

    for (mdns_result_t *result = results; result; result = result->next) {
        char ip_buf[64];
        const char *ip;
        const char *endpoint;
        const char *hostname;
        const char *instance;
        const char *host_for_url;
        cJSON *device;
        char server_url[320];
        char url[384];

        if (!include_self && result->hostname && strcmp(result->hostname, "clawgent") == 0) {
            continue;
        }

        ip = cap_mcp_pick_ip_string(result, ip_buf, sizeof(ip_buf));
        endpoint = cap_mcp_find_txt_value(result, "endpoint");
        if (!endpoint || !endpoint[0]) {
            endpoint = CAP_MCP_DEFAULT_ENDPOINT;
        }

        hostname = (result->hostname && result->hostname[0]) ? result->hostname : "(unknown)";
        instance = (result->instance_name && result->instance_name[0]) ?
                   result->instance_name : "(unknown)";
        host_for_url = (ip && ip[0]) ? ip : hostname;

        device = cJSON_CreateObject();
        if (!device) {
            mdns_query_results_free(results);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }

        snprintf(server_url, sizeof(server_url), "http://%s:%u", host_for_url, result->port);
        snprintf(url, sizeof(url), "%s/%s", server_url, endpoint);
        cJSON_AddStringToObject(device, "instance", instance);
        cJSON_AddStringToObject(device, "hostname", hostname);
        cJSON_AddStringToObject(device, "ip", ip ? ip : "(unresolved)");
        cJSON_AddNumberToObject(device, "port", result->port);
        cJSON_AddStringToObject(device, "endpoint", endpoint);
        cJSON_AddStringToObject(device, "server_url", server_url);
        cJSON_AddStringToObject(device, "url", url);
        cJSON_AddItemToArray(devices, device);
        found++;
    }

    mdns_query_results_free(results);
    cJSON_AddNumberToObject(root, "count", (double)found);
    cJSON_AddItemToObject(root, "devices", devices);
    *result_out = root;
    return ESP_OK;
}
