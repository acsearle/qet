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

namespace lox {
    
#define ENUMERATE_X_VALUE \
X(NIL)\
X(BOOL)\
X(INT64)\
X(OBJECT)\

#define X(Z) VALUE_##Z,
    enum ValueType { ENUMERATE_X_VALUE };
#undef X
    
#define X(Z) [VALUE_##Z] = "VALUE_" #Z,
    constexpr const char* ValueTypeCString[] { ENUMERATE_X_VALUE };
#undef X
    
    
    struct Object;
    
    struct Value {
        
        ValueType type;
        
        // 4 bytes padding
        
        union _as_t {
            
            int64_t int64;
            Object* object;         // garbage collected
            
            uint8_t bytes[8];
            
        } as;
        
        bool invariant() const;
        
        explicit Value() { type = VALUE_NIL; as.object = nullptr; }
        explicit Value(bool value) { type = VALUE_BOOL; as.int64 = value; }
        explicit Value(int64_t value) { type = VALUE_INT64; as.int64 = value; }
        explicit Value(Object* value) { type = VALUE_OBJECT; assert(value != nullptr); as.object = value; }
        
        explicit operator bool() const {
            return (type != VALUE_NIL) && ((type != VALUE_BOOL) || as.int64);
        }
        
        bool is_nil()     const { return type == VALUE_NIL    ; }
        bool is_bool()    const { return type == VALUE_BOOL   ; }
        bool is_int64()   const { return type == VALUE_INT64  ; }
        bool is_object()     const { return type == VALUE_OBJECT    ; }
        
        bool     as_bool()    const { assert(is_bool());    return (bool) as.int64; }
        int64_t  as_int64()   const { assert(is_int64());   return as.int64; }
        Object*     as_object()     const { assert(is_object());     return as.object; }
        
    };
    
    bool operator==(const Value& a, const Value& b);
    
    void printValue(Value value);
    
    decltype(auto) visit(const Value& value, auto&& visitor) {
        switch (value.type) {
            case VALUE_NIL:
                return std::forward<decltype(visitor)>(value.as.bytes);
            case VALUE_BOOL:
                return std::forward<decltype(visitor)>((bool) value.as.int64);
            case VALUE_INT64:
                return std::forward<decltype(visitor)>(value.as.int64);
            case VALUE_OBJECT:
                return std::forward<decltype(visitor)>(value.as.object);
        }
    }
    
} // namespace lox

#endif /* value_hpp */
