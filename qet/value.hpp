//
//  value.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef value_hpp
#define value_hpp

#include <cassert>

#include "common.hpp"

struct Obj;

enum ValueType {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ,
};

struct Value {
    
    ValueType type;
    
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
    
    bool is_bool() const { return type == VAL_BOOL; }
    bool is_nil() const { return type == VAL_BOOL; }
    bool is_number() const { return type == VAL_NUMBER; }
    bool is_obj() const { return type == VAL_OBJ; }
    
    bool as_bool() const { assert(is_bool()); return as.boolean; }
    double as_number() const { assert(is_number()); return as.number; }
    Obj* as_obj() const { assert(is_obj()); return as.obj; }
    
    explicit Value() { type = VAL_NIL; as.obj = nullptr; }
    
    explicit Value(bool value) { type = VAL_BOOL; as.boolean = value; }
    explicit Value(double value) { type = VAL_NUMBER; as.number = value; }
    explicit Value(Obj* value) { type = VAL_OBJ; as.obj = value; }
    
};

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)

#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value)    ((value).as.obj)

//#define BOOL_VAL(value)   ((Value){ VAL_BOOL, {.boolean = value}})
//#define NIL_VAL           ((Value){ VAL_NIL, {.number = 0.0}})
//#define NUMBER_VAL(value) ((Value){ VAL_NUMBER, {.number = value}})
//#define OBJ_VAL(object)    ((Value){ VAL_OBJ, {.obj = (Obj*)object}})

bool valuesEqual(Value a, Value b);
void printValue(Value value);

#endif /* value_hpp */
