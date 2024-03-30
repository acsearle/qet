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
    
    union _as_t {
        
        bool boolean;
        double number;
        Obj* obj;         // garbage collected
        
        uint8_t bytes[8];
        
    } as;
    
    bool invariant() const;
    
    explicit Value() { type = VAL_NIL; as.obj = nullptr; }
    
    explicit Value(bool value) { type = VAL_BOOL; as.boolean = value; }
    explicit Value(double value) { type = VAL_NUMBER; as.number = value; }
    explicit Value(Obj* value) { type = VAL_OBJ; assert(value != nullptr); as.obj = value; }
    
    explicit operator bool() const {
        return (type != VAL_NIL) && ((type != VAL_BOOL) || as.boolean);
    }
        
    bool is_nil()    const { return type == VAL_NIL   ; }
    bool is_bool()   const { return type == VAL_BOOL  ; }
    bool is_number() const { return type == VAL_NUMBER; }
    bool is_obj()    const { return type == VAL_OBJ   ; }
    
    bool as_bool() const { assert(is_bool()); return as.boolean; }
    double as_number() const { assert(is_number()); return as.number; }
    Obj* as_obj() const { assert(is_obj()); return as.obj; }
    
};

bool operator==(const Value& a, const Value& b);

void printValue(Value value);

decltype(auto) visit(const Value& value, auto&& visitor) {
    switch (value.type) {
        case VAL_NIL:
            return std::forward<decltype(visitor)>(value.as.bytes);
        case VAL_BOOL:
            return std::forward<decltype(visitor)>(value.as.boolean);
        case VAL_NUMBER:
            return std::forward<decltype(visitor)>(value.as.number);
        case VAL_OBJ:
            return std::forward<decltype(visitor)>(value.as.obj);
    }
}

#endif /* value_hpp */
