/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>

#include "claw_core.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *session_root_dir;
    const char *long_term_memory_path;
    size_t max_session_messages;
    size_t max_message_chars;
} claw_memory_config_t;

esp_err_t claw_memory_init(const claw_memory_config_t *config);
esp_err_t claw_memory_session_load(const char *session_id, char *buf, size_t size);
esp_err_t claw_memory_session_append(const char *session_id,
                                     const char *user_text,
                                     const char *assistant_text);
esp_err_t claw_memory_long_term_read(char *buf, size_t size);
esp_err_t claw_memory_long_term_write(const char *content);
esp_err_t claw_memory_append_session_turn_callback(const char *session_id,
                                                   const char *user_text,
                                                   const char *assistant_text,
                                                   void *user_ctx);

extern const claw_core_context_provider_t claw_memory_long_term_provider;
extern const claw_core_context_provider_t claw_memory_session_history_provider;

#ifdef __cplusplus
}
#endif
