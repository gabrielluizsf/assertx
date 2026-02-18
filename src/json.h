#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define JSON_BUFFER_SIZE 8192


/* =========================
   STRUCTS
========================= */

typedef struct {
    char buffer[JSON_BUFFER_SIZE];
    size_t length;
    int count;
    bool closed;
} JSON;


typedef struct {
    char buffer[JSON_BUFFER_SIZE];
    size_t length;
    int count;
    bool closed;
} JSONArray;


/* =========================
   INTERNAL SAFE APPEND
========================= */

static inline void json_appendf(JSON *json, const char *fmt, ...)
{
    if (json->length >= JSON_BUFFER_SIZE)
        return;

    va_list args;
    va_start(args, fmt);

    size_t remaining = JSON_BUFFER_SIZE - json->length;

    int written = vsnprintf(
        json->buffer + json->length,
        remaining,
        fmt,
        args);

    va_end(args);

    if (written < 0)
        return;

    if ((size_t)written >= remaining)
    {
        json->length = JSON_BUFFER_SIZE - 1;
        json->buffer[json->length] = '\0';
        return;
    }

    json->length += written;
}


static inline void json_array_appendf(JSONArray *arr, const char *fmt, ...)
{
    if (arr->length >= JSON_BUFFER_SIZE)
        return;

    va_list args;
    va_start(args, fmt);

    size_t remaining = JSON_BUFFER_SIZE - arr->length;

    int written = vsnprintf(
        arr->buffer + arr->length,
        remaining,
        fmt,
        args);

    va_end(args);

    if (written < 0)
        return;

    if ((size_t)written >= remaining)
    {
        arr->length = JSON_BUFFER_SIZE - 1;
        arr->buffer[arr->length] = '\0';
        return;
    }

    arr->length += written;
}


/* =========================
   COMMA HELPERS
========================= */

static inline void json_add_comma(JSON *json)
{
    if (json->count > 0 && json->length < JSON_BUFFER_SIZE - 1)
    {
        json->buffer[json->length++] = ',';
        json->buffer[json->length] = '\0';
    }

    json->count++;
}


static inline void json_array_add_comma(JSONArray *arr)
{
    if (arr->count > 0 && arr->length < JSON_BUFFER_SIZE - 1)
    {
        arr->buffer[arr->length++] = ',';
        arr->buffer[arr->length] = '\0';
    }

    arr->count++;
}


/* =========================
   INIT
========================= */

static inline JSON json_new()
{
    JSON json;

    json.buffer[0] = '{';
    json.buffer[1] = '\0';

    json.length = 1;
    json.count = 0;
    json.closed = false;

    return json;
}


static inline JSONArray json_array_new()
{
    JSONArray arr;

    arr.buffer[0] = '[';
    arr.buffer[1] = '\0';

    arr.length = 1;
    arr.count = 0;
    arr.closed = false;

    return arr;
}


/* =========================
   JSON ADD FUNCTIONS
========================= */

static inline void json_add_string(JSON *json, const char *key, const char *value)
{
    json_add_comma(json);
    json_appendf(json, "\"%s\":\"%s\"", key, value);
}


static inline void json_add_bool(JSON *json, const char *key, _Bool value)
{
    json_add_comma(json);
    json_appendf(json, "\"%s\":%s", key, value ? "true" : "false");
}


static inline void json_add_null(JSON *json, const char *key)
{
    json_add_comma(json);
    json_appendf(json, "\"%s\":null", key);
}


static inline void json_add_number_int(JSON *json, const char *key, long long value)
{
    json_add_comma(json);
    json_appendf(json, "\"%s\":%lld", key, value);
}


static inline void json_add_number_double(JSON *json, const char *key, double value)
{
    json_add_comma(json);
    json_appendf(json, "\"%s\":%g", key, value);
}


/* =========================
   ARRAY ADD FUNCTIONS
========================= */

static inline void json_array_add_string(JSONArray *arr, const char *value)
{
    json_array_add_comma(arr);
    json_array_appendf(arr, "\"%s\"", value);
}


static inline void json_array_add_number_int(JSONArray *arr, long long value)
{
    json_array_add_comma(arr);
    json_array_appendf(arr, "%lld", value);
}


static inline void json_array_add_number_double(JSONArray *arr, double value)
{
    json_array_add_comma(arr);
    json_array_appendf(arr, "%g", value);
}


static inline void json_array_close(JSONArray *arr)
{
    if (!arr->closed && arr->length < JSON_BUFFER_SIZE - 1)
    {
        arr->buffer[arr->length++] = ']';
        arr->buffer[arr->length] = '\0';
        arr->closed = true;
    }
}


/* =========================
   NESTED OBJECT / ARRAY
========================= */

static inline void json_add_object(JSON *json, const char *key, JSON *child)
{
    json_add_comma(json);

    if (!child->closed && child->length < JSON_BUFFER_SIZE - 1)
    {
        child->buffer[child->length++] = '}';
        child->buffer[child->length] = '\0';
        child->closed = true;
    }

    json_appendf(json, "\"%s\":%s", key, child->buffer);
}


static inline void json_add_array(JSON *json, const char *key, JSONArray *arr)
{
    json_add_comma(json);

    json_array_close(arr);

    json_appendf(json, "\"%s\":%s", key, arr->buffer);
}


/* =========================
   GENERIC MACROS
========================= */

#define json_add(json, key, value) \
    _Generic((value), \
        _Bool: json_add_bool, \
        char*: json_add_string, \
        const char*: json_add_string, \
        int: json_add_number_int, \
        long: json_add_number_int, \
        long long: json_add_number_int, \
        unsigned int: json_add_number_int, \
        unsigned long: json_add_number_int, \
        unsigned long long: json_add_number_int, \
        float: json_add_number_double, \
        double: json_add_number_double, \
        long double: json_add_number_double \
    )(json, key, value)



#define json_array_add(arr, value) \
    _Generic((value), \
        char*: json_array_add_string, \
        const char*: json_array_add_string, \
        int: json_array_add_number_int, \
        long: json_array_add_number_int, \
        long long: json_array_add_number_int, \
        unsigned: json_array_add_number_int, \
        unsigned long: json_array_add_number_int, \
        unsigned long long: json_array_add_number_int, \
        float: json_array_add_number_double, \
        double: json_array_add_number_double, \
        long double: json_array_add_number_double \
    )(arr, value)


/* =========================
   STRINGIFY
========================= */

static inline char *json_stringify(JSON *json)
{
    if (!json->closed && json->length < JSON_BUFFER_SIZE - 1)
    {
        json->buffer[json->length++] = '}';
        json->buffer[json->length] = '\0';
        json->closed = true;
    }

    return json->buffer;
}


#endif
