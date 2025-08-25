#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <json-c/arraylist.h>
#include <json-c/json_object.h>
#include <json-c/json_object_iterator.h>
#include <json-c/json_tokener.h>
#include <json-c/linkhash.h>

#include "desktop/tree/container.h"

#include "input/keybind.h"

#include "util/list.h"
#include "util/log.h"

#define NUM_MODIFIERS 8

static const char* modifiers_names[] = {"shift", "caps", "ctrl", "alt", "mod2", "mod3", "logo", "mod5"};

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

//TODO: comment
static bool parse_mod(const char* string, enum wlr_keyboard_modifier* mod)
{
    assert(string && mod);

    //TODO: case insensitive comparison

    for (int i = 0; i < NUM_MODIFIERS; i++)
    {
        if (strcmp(string, modifiers_names[i]) == 0)
        {
            *mod = (enum wlr_keyboard_modifier)(1 << i);
            return  true;
        }
    }

    return false;
}

//TODO: comment
static bool parse_mods(const char* string, uint32_t* mods)
{
    assert(string && mods);

    uint32_t result = 0;
    char* copy = strdup(string); //strtok edits string

    if (copy == NULL)
        return false;

    //TODO: don't use strtok, I don't like it
    const char* token = strtok(copy, "+");

    //parse modifiers separated by plus-sign

    while (token != NULL)
    {
        enum wlr_keyboard_modifier mod = WLR_MODIFIER_SHIFT; //overridden

        if (!parse_mod(token, &mod))
        {
            free(copy);
            return false;
        }

        result |= mod; //add modifier
        token = strtok(NULL, "+"); //next, strtok retains internal state, don't really like that
    }

    free(copy);
    copy = NULL;

    *mods = result;

    return true;
}

static bool config_parse_keybind(struct e_config* config, const struct json_object* obj)
{
    assert(config && obj);

    if (!json_object_is_type(obj, json_type_object))
    {
        e_log_error("config_parse_keybind: keybind must be an object!");
        return false;
    }

    xkb_keysym_t keysym = XKB_KEY_NoSymbol;
    uint32_t mods = WLR_MODIFIER_LOGO; //TODO: support other modifiers
    const char* command = NULL;

    struct json_object* command_val = NULL;
    if (json_object_object_get_ex(obj, "command", &command_val) == 1 && json_object_is_type(command_val, json_type_string))
    {
        command = json_object_get_string(command_val);
    }
    else
    {
        e_log_error("config_parse_keybind: keybind must have a command (string)!");
        return false;
    }

    struct json_object* keysym_val = NULL;
    if (json_object_object_get_ex(obj, "keysym", &keysym_val) == 1 && json_object_is_type(keysym_val, json_type_string))
        keysym = xkb_keysym_from_name(json_object_get_string(keysym_val), XKB_KEYSYM_CASE_INSENSITIVE);
    
    if (keysym == XKB_KEY_NoSymbol)
    {
        e_log_error("config_parse_keybind: keybind must have a recognizeable keysym (character)!");
        return false;
    }

    struct json_object* mods_val = NULL;
    bool parsed_mods = false;
    if (json_object_object_get_ex(obj, "mods", &mods_val) == 1 && json_object_is_type(mods_val, json_type_string))
        parsed_mods = parse_mods(json_object_get_string(mods_val), &mods);

    if (!parsed_mods)
    {
        e_log_error("config_parse_keybind: keybind must have mods AKA modifiers (separated by plus-signs!)");
        return false;
    }

    struct e_keybind* keybind = e_keybind_create(keysym, mods, command);

    if (keybind == NULL)
    {
        e_log_error("config_parse_keybind: keybind alloc fail!");
        return false;
    }

    e_list_add(&config->keyboard.keybinds, keybind);

    return true;
}

static bool config_parse_keybinds(struct e_config* config, const struct json_object* array)
{
    assert(config && array);

    if (!json_object_is_type(array, json_type_array))
    {
        e_log_error("config_parse_keybinds: keybinds must be an array!");
        return false;
    }

    size_t len = json_object_array_length(array);

    for (size_t i = 0; i < len; i++)
    {
        struct json_object* obj = json_object_array_get_idx(array, i);

        if (obj != NULL)
            config_parse_keybind(config, obj);
    }

    return true;
}

bool e_config_parse(struct e_config* config, const char* path)
{
    assert(config && path);

    struct json_object* file_obj = json_parse_file(path);
    
    if (file_obj == NULL)
    {
        //TODO: error
        e_log_error("e_config_parse: failed to parse json!");
        return false;
    }

    json_object_object_foreach(file_obj, key, val)
    {
        if (strcmp(key, "keybinds") == 0)
        {
            config_parse_keybinds(config, val);
        }
        else 
        {
            e_log_error("e_config_parse: unknown key %s", key);
            break;
        }
    }

    json_object_put(file_obj);

    return true;
}