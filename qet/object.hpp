//
//  object.hpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#ifndef object_hpp
#define object_hpp

#include "common.hpp"
#include "value.hpp"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

enum ObjType {
    OBJ_STRING,
};

struct Obj {
    ObjType type;
    Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif /* object_hpp */
