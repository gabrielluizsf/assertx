#include <stdio.h>
#include "../src/json.h"
#include "xassert.h"


/* ===============================
   TEST JSON INIT
   =============================== */

void test_json_new()
{
    JSON json = json_new();

    assert_equal(json.buffer, "{", "json_new should initialize with '{'");
}


/* ===============================
   TEST ADD STRING
   =============================== */

void test_json_add_string()
{
    JSON json = json_new();

    json_add_string(&json, "name", "Gabriel");

    assert_equal(
        json_stringify(&json),
        "{\"name\":\"Gabriel\"}",
        "json_add_string should add string field"
    );
}


/* ===============================
   TEST ADD NUMBER INT
   =============================== */

void test_json_add_int()
{
    JSON json = json_new();

    json_add(&json, "age", 20);

    assert_equal(
        json_stringify(&json),
        "{\"age\":20}",
        "json_add should add int field"
    );
}


/* ===============================
   TEST ADD NUMBER DOUBLE
   =============================== */

void test_json_add_double()
{
    JSON json = json_new();

    json_add(&json, "height", 1.75);

    assert_equal(
        json_stringify(&json),
        "{\"height\":1.75}",
        "json_add should add double field"
    );
}


/* ===============================
   TEST ADD BOOL
   =============================== */

void test_json_add_bool()
{
    JSON json = json_new();

    json_add_bool(&json, "admin", true);

    assert_equal(
        json_stringify(&json),
        "{\"admin\":true}",
        "json_add_bool should add boolean field"
    );
}


/* ===============================
   TEST ADD NULL
   =============================== */

void test_json_add_null()
{
    JSON json = json_new();

    json_add_null(&json, "data");

    assert_equal(
        json_stringify(&json),
        "{\"data\":null}",
        "json_add_null should add null field"
    );
}


/* ===============================
   TEST MULTIPLE FIELDS
   =============================== */

void test_json_multiple_fields()
{
    JSON json = json_new();

    json_add(&json, "name", "Gabriel");
    json_add(&json, "age", 20);
    bool is_admin = true;
    json_add(&json, "admin", is_admin);

    assert_equal(
        json_stringify(&json),
        "{\"name\":\"Gabriel\",\"age\":20,\"admin\":true}",
        "json should support multiple fields"
    );
}


/* ===============================
   TEST ARRAY STRING
   =============================== */

void test_json_array_string()
{
    JSONArray arr = json_array_new();

    json_array_add(&arr, "admin");
    json_array_add(&arr, "user");

    arr.buffer[arr.length++] = ']';
    arr.buffer[arr.length] = '\0';

    assert_equal(
        arr.buffer,
        "[\"admin\",\"user\"]",
        "json_array_add should add string values"
    );
}


/* ===============================
   TEST ARRAY NUMBER
   =============================== */

void test_json_array_number()
{
    JSONArray arr = json_array_new();

    json_array_add(&arr, 10);
    json_array_add(&arr, 20);

    arr.buffer[arr.length++] = ']';
    arr.buffer[arr.length] = '\0';

    assert_equal(
        arr.buffer,
        "[10,20]",
        "json_array_add should add number values"
    );
}


/* ===============================
   TEST ADD ARRAY TO OBJECT
   =============================== */

void test_json_add_array()
{
    JSON json = json_new();

    JSONArray arr = json_array_new();

    json_array_add(&arr, "admin");
    json_array_add(&arr, "user");

    json_add_array(&json, "roles", &arr);

    assert_equal(
        json_stringify(&json),
        "{\"roles\":[\"admin\",\"user\"]}",
        "json_add_array should add array to object"
    );
}


/* ===============================
   TEST NESTED OBJECT
   =============================== */

void test_json_nested_object()
{
    JSON parent = json_new();
    JSON child = json_new();

    json_add(&child, "city", "Recife");

    json_add_object(&parent, "address", &child);

    assert_equal(
        json_stringify(&parent),
        "{\"address\":{\"city\":\"Recife\"}}",
        "json_add_object should support nested object"
    );
}
