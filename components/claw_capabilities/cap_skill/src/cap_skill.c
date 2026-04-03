/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cap_skill.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "claw_cap.h"
#include "claw_skill.h"

static const char *CAP_SKILL_ACTIVATE = "activate_skill";
static const char *CAP_SKILL_DEACTIVATE = "deactivate_skill";

static void cap_skill_free_string_array(char **items, size_t count)
{
    size_t i;

    if (!items) {
        return;
    }

    for (i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static esp_err_t cap_skill_build_result(const char *action,
                                        const char *session_id,
                                        const char *skill_id,
                                        char *output,
                                        size_t output_size)
{
    char **active_skill_ids = NULL;
    size_t active_skill_count = 0;
    cJSON *root = NULL;
    cJSON *active = NULL;
    char *rendered = NULL;
    esp_err_t err;
    size_t i;

    if (!action || !output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    err = claw_skill_load_active_skill_ids(session_id, &active_skill_ids, &active_skill_count);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        return err;
    }

    root = cJSON_CreateObject();
    active = cJSON_CreateArray();
    if (!root || !active) {
        cJSON_Delete(root);
        cJSON_Delete(active);
        cap_skill_free_string_array(active_skill_ids, active_skill_count);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "action", action);
    cJSON_AddStringToObject(root, "session_id", session_id ? session_id : "");
    if (skill_id) {
        cJSON_AddStringToObject(root, "skill_id", skill_id);
    }
    cJSON_AddBoolToObject(root, "ok", true);
    for (i = 0; i < active_skill_count; i++) {
        cJSON_AddItemToArray(active, cJSON_CreateString(active_skill_ids[i]));
    }
    cJSON_AddItemToObject(root, "active_skill_ids", active);

    rendered = cJSON_PrintUnformatted(root);
    if (!rendered) {
        cJSON_Delete(root);
        cap_skill_free_string_array(active_skill_ids, active_skill_count);
        return ESP_ERR_NO_MEM;
    }

    snprintf(output, output_size, "%s", rendered);
    free(rendered);
    cJSON_Delete(root);
    cap_skill_free_string_array(active_skill_ids, active_skill_count);
    return ESP_OK;
}

static esp_err_t cap_skill_activate_execute(const char *input_json,
                                            const claw_cap_call_context_t *ctx,
                                            char *output,
                                            size_t output_size)
{
    cJSON *root = NULL;
    cJSON *skill_id_item = NULL;
    char skill_id_buf[64] = {0};
    esp_err_t err;

    if (!ctx || !ctx->session_id || !ctx->session_id[0] || !output || output_size == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    root = cJSON_Parse(input_json ? input_json : "{}");
    skill_id_item = root ? cJSON_GetObjectItemCaseSensitive(root, "skill_id") : NULL;
    if (!cJSON_IsString(skill_id_item) || !skill_id_item->valuestring || !skill_id_item->valuestring[0]) {
        cJSON_Delete(root);
        snprintf(output, output_size, "{\"ok\":false,\"error\":\"skill_id is required\"}");
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(skill_id_buf, sizeof(skill_id_buf), "%s", skill_id_item->valuestring);
    err = claw_skill_activate_for_session(ctx->session_id, skill_id_buf);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        snprintf(output,
                 output_size,
                 "{\"ok\":false,\"error\":\"failed to activate skill\",\"skill_id\":\"%s\"}",
                 skill_id_buf);
        return err;
    }

    return cap_skill_build_result(CAP_SKILL_ACTIVATE,
                                  ctx->session_id,
                                  skill_id_buf,
                                  output,
                                  output_size);
}

static esp_err_t cap_skill_deactivate_execute(const char *input_json,
                                              const claw_cap_call_context_t *ctx,
                                              char *output,
                                              size_t output_size)
{
    cJSON *root = NULL;
    cJSON *skill_id_item = NULL;
    char skill_id_buf[64] = {0};
    esp_err_t err;

    if (!ctx || !ctx->session_id || !ctx->session_id[0] || !output || output_size == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    root = cJSON_Parse(input_json ? input_json : "{}");
    skill_id_item = root ? cJSON_GetObjectItemCaseSensitive(root, "skill_id") : NULL;
    if (!cJSON_IsString(skill_id_item) || !skill_id_item->valuestring || !skill_id_item->valuestring[0]) {
        cJSON_Delete(root);
        snprintf(output, output_size, "{\"ok\":false,\"error\":\"skill_id is required\"}");
        return ESP_ERR_INVALID_ARG;
    }
    snprintf(skill_id_buf, sizeof(skill_id_buf), "%s", skill_id_item->valuestring);

    if (strcmp(skill_id_buf, "all") == 0) {
        err = claw_skill_clear_active_for_session(ctx->session_id);
    } else {
        err = claw_skill_deactivate_for_session(ctx->session_id, skill_id_buf);
    }
    cJSON_Delete(root);
    if (err != ESP_OK) {
        snprintf(output,
                 output_size,
                 "{\"ok\":false,\"error\":\"failed to deactivate skill\",\"skill_id\":\"%s\"}",
                 skill_id_buf);
        return err;
    }

    return cap_skill_build_result(CAP_SKILL_DEACTIVATE,
                                  ctx->session_id,
                                  skill_id_buf,
                                  output,
                                  output_size);
}

static const claw_cap_descriptor_t s_skill_descriptors[] = {
    {
        .id = "activate_skill",
        .name = "activate_skill",
        .family = "skill",
        .description = "Activate a skill by skill_id for the current session and load its skill documentation into the prompt.",
        .kind = CLAW_CAP_KIND_CALLABLE,
        .cap_flags = CLAW_CAP_FLAG_CALLABLE_BY_LLM,
        .input_schema_json =
        "{\"type\":\"object\",\"properties\":{\"skill_id\":{\"type\":\"string\"}},\"required\":[\"skill_id\"]}",
        .execute = cap_skill_activate_execute,
    },
    {
        .id = "deactivate_skill",
        .name = "deactivate_skill",
        .family = "skill",
        .description = "Deactivate one skill by skill_id, or use all to clear active skills and remove their skill documentation from the prompt for the current session.",
        .kind = CLAW_CAP_KIND_CALLABLE,
        .cap_flags = CLAW_CAP_FLAG_CALLABLE_BY_LLM,
        .input_schema_json =
        "{\"type\":\"object\",\"properties\":{\"skill_id\":{\"type\":\"string\"}},\"required\":[\"skill_id\"]}",
        .execute = cap_skill_deactivate_execute,
    },
};

static const claw_cap_group_t s_skill_group = {
    .group_id = "cap_skill",
    .descriptors = s_skill_descriptors,
    .descriptor_count = sizeof(s_skill_descriptors) / sizeof(s_skill_descriptors[0]),
};

esp_err_t cap_skill_register_group(void)
{
    if (claw_cap_group_exists(s_skill_group.group_id)) {
        return ESP_OK;
    }

    return claw_cap_register_group(&s_skill_group);
}
