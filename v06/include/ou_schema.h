#ifndef OU_SCHEMA_H
#define OU_SCHEMA_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *group;
    const char *name;
    const char *type_expression;
    const char *access;
    const char *unit;
    uint8_t requires_restart;
    uint8_t hidden_in_oem;
} ou_schema_field;

extern const ou_schema_field ou_schema_fields[];
extern const size_t ou_schema_field_count;

#endif
