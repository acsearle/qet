//
//  value.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include <cstdio>
#include <cstring>

#include "memory.hpp"
#include "object.hpp"
#include "value.hpp"

bool Value::invariant() const {
    switch (type) {
        case VAL_NIL:
            return as.obj == nullptr; // caution punning
        case VAL_BOOL:
            return true;
        case VAL_NUMBER:
            return true;
        case VAL_OBJ:
            return as.obj != nullptr;
        default:
            return false;
    }
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(value.as_bool() ? "true" : "false");
            break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", value.as_number()); break;
        case VAL_OBJ: printObject(value); break;
    }
}

bool operator==(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:   return a.as_bool() == b.as_bool();
        case VAL_NIL:    return true;
        case VAL_NUMBER: return a.as_number() == b.as_number();
        case VAL_OBJ:    return a.as_obj() == b.as_obj();
        default:         return false; // Unreachable.
    }
}
