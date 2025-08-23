#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <json-c/json_object.h>
#include <json-c/json_object_iterator.h>
#include <json-c/json_tokener.h>

#include "desktop/tree/container.h"

#include "input/keybind.h"

#include "util/list.h"
#include "util/log.h"

void e_config_init(struct e_config* config)
{
    assert(config);

    config->current_tiling_mode = E_TILING_MODE_HORIZONTAL;

    e_list_init(&config->keyboard.keybinds, 10);
    config->keyboard.repeat_rate_hz = 25;
    config->keyboard.repeat_delay_ms = 600;

    config->xwayland_lazy = true;
}

void e_config_fini(struct e_config* config)
{
    assert(config);
    
    for (int i = 0; i < config->keyboard.keybinds.count; i++)
    {
        struct e_keybind* keybind = e_list_at(&config->keyboard.keybinds, i);

        if (keybind != NULL)
            e_keybind_free(keybind);
    }

    e_list_fini(&config->keyboard.keybinds);
}

static long file_len(FILE* file)
{
    assert(file);

    //position to return to after getting length of file
    long pos = ftell(file);

    //go to the end of file and get position -> length
    fseek(file, 0, SEEK_END);
    long len = ftell(file);

    //go back
    fseek(file, pos, SEEK_SET);

    return len;
}

// Returned buffer must be free()'d.
// Outed len includes null-terminator.
// Returns NULL on fail.
static char* file_read_all(FILE* file, long* len)
{
    assert(file);

    long flen = file_len(file);

    if (flen <= 0)
    {
        e_log_error("file_read_all: something went wrong when retrieving length of file!");
        return false;
    }
    
    char* content = malloc((size_t)flen + 1);

    if (content == NULL)
    {
        e_log_error("file_read_all: content alloc fail");
        return NULL;
    }

    fread(content, 1, flen, file);
    content[flen] = '\0'; //null-terminate

    if (len != NULL)
        *len = flen + 1;

    return content;
}

// Free using json_object_put(obj)
// Returns NULL on fail.
static struct json_object* json_parse_file(const char* path)
{
    assert(path);

    FILE* file = fopen(path, "r");

    if (file == NULL)
    {
        e_log_error("json_parse_file: failed to open file %s for reading!", path);
        return false;
    }
    
    long len = 0;
    char* content = file_read_all(file, &len);

    fclose(file);
    file = NULL;

    if (content == NULL)
    {
        e_log_error("json_parse_file: failed to read file content");
        return false;
    }
    else if (len > INT32_MAX)
    {
        free(content);
        e_log_error("json_parse_file: file size larger than INT32_MAX (%i), json parser can't parse", INT32_MAX);
        return false;
    }

    struct json_object* object = json_tokener_parse(content);
    
    free(content);
    content = NULL;

    return object;
}

bool e_config_parse(struct e_config* config, const char* path)
{
    assert(config && path);

    struct json_object* object = json_parse_file(path);
    
    if (object == NULL)
    {
        //TODO: error
        e_log_error("e_config_parse: failed to parse json!");
        return false;
    }

    json_object_put(object);

    return true;
}