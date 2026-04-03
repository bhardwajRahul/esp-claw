/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cmd_cap_skill.h"

#include <stdio.h>
#include <stdlib.h>

#include "argtable3/argtable3.h"
#include "claw_skill.h"
#include "esp_console.h"

static struct {
    struct arg_lit *list;
    struct arg_lit *catalog;
    struct arg_str *activate;
    struct arg_str *deactivate;
    struct arg_lit *clear;
    struct arg_str *session;
    struct arg_end *end;
} skill_args;

static void free_string_array(char **items, size_t count)
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

static int skill_func(int argc, char **argv)
{
    char **active_skill_ids = NULL;
    size_t active_skill_count = 0;
    const char *session_id;
    esp_err_t err;
    int nerrors = arg_parse(argc, argv, (void **)&skill_args);
    int operation_count;
    size_t i;

    if (nerrors != 0) {
        arg_print_errors(stderr, skill_args.end, argv[0]);
        return 1;
    }

    operation_count = skill_args.list->count + skill_args.catalog->count + skill_args.clear->count +
                      skill_args.activate->count + skill_args.deactivate->count;
    if (operation_count != 1) {
        printf("Exactly one operation must be specified\n");
        return 1;
    }

    session_id = skill_args.session->count ? skill_args.session->sval[0] : "default";

    if (skill_args.catalog->count) {
        char *buf = calloc(1, 4096);

        if (!buf) {
            printf("Out of memory\n");
            return 1;
        }

        err = claw_skill_read_skills_list(buf, 4096);
        if (err != ESP_OK) {
            printf("skill catalog failed: %s\n", esp_err_to_name(err));
            free(buf);
            return 1;
        }

        printf("%s\n", buf);
        free(buf);
        return 0;
    }

    if (skill_args.activate->count) {
        err = claw_skill_activate_for_session(session_id, skill_args.activate->sval[0]);
        if (err != ESP_OK) {
            printf("skill activate failed: %s\n", esp_err_to_name(err));
            return 1;
        }
    } else if (skill_args.deactivate->count) {
        err = claw_skill_deactivate_for_session(session_id, skill_args.deactivate->sval[0]);
        if (err != ESP_OK) {
            printf("skill deactivate failed: %s\n", esp_err_to_name(err));
            return 1;
        }
    } else if (skill_args.clear->count) {
        err = claw_skill_clear_active_for_session(session_id);
        if (err != ESP_OK) {
            printf("skill clear failed: %s\n", esp_err_to_name(err));
            return 1;
        }
    }

    err = claw_skill_load_active_skill_ids(session_id, &active_skill_ids, &active_skill_count);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        printf("skill list failed: %s\n", esp_err_to_name(err));
        return 1;
    }

    printf("session=%s\n", session_id);
    if (active_skill_count == 0) {
        printf("(no active skills)\n");
    } else {
        for (i = 0; i < active_skill_count; i++) {
            printf("%s\n", active_skill_ids[i]);
        }
    }

    free_string_array(active_skill_ids, active_skill_count);
    return 0;
}

void register_cap_skill(void)
{
    skill_args.list = arg_lit0("l", "list", "List active skills for one session");
    skill_args.catalog = arg_lit0(NULL, "catalog", "Print the skills catalog JSON");
    skill_args.activate = arg_str0("a", "activate", "<skill_id>", "Activate one skill");
    skill_args.deactivate = arg_str0("d", "deactivate", "<skill_id>", "Deactivate one skill");
    skill_args.clear = arg_lit0(NULL, "clear", "Clear all active skills for one session");
    skill_args.session = arg_str0("s", "session", "<session_id>", "Session id, defaults to 'default'");
    skill_args.end = arg_end(8);

    const esp_console_cmd_t skill_cmd = {
        .command = "skill",
        .help = "Skill operations.\n"
        "Examples:\n"
        " skill --catalog\n"
        " skill --list --session default\n"
        " skill --activate weather --session default\n"
        " skill --deactivate weather --session default\n"
        " skill --clear --session default\n",
        .func = skill_func,
        .argtable = &skill_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&skill_cmd));
}
