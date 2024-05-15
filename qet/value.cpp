//
//  value.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "object.hpp"
#include "value.hpp"

namespace lox {
    
    void scan(const Value& self, gc::ScanContext& context) {
        if (self.type == VALUE_OBJECT)
            scan(self.as.object, context);
    }
    
    void scan(const AtomicValue& self, gc::ScanContext& context) {
        scan(self.load(), context);
    }
    
    bool Value::invariant() const {
        switch (type) {
            case VALUE_NIL:
                return as.int64 == 0; // caution punning
            case VALUE_BOOL:
                return (as.int64 == 0) || (as.int64 == 1);
            case VALUE_INT64:
                return true;
            case VALUE_OBJECT:
                return as.object != nullptr;
            default:
                return false;
        }
    }
    
    void printValue(Value value) {
        switch (value.type) {
            case VALUE_BOOL:
                printf(value.as_bool() ? "true" : "false");
                break;
            case VALUE_NIL: printf("nil"); break;
            case VALUE_INT64: printf("%" PRId64, value.as_int64()); break;
            case VALUE_OBJECT: printObject(value); break;
        }
    }
    
    bool equalityAsReals(int64_t a, uint64_t b) {
        return (a >= 0) && (a == b);
    }
    
    bool equalityAsReals(int64_t a, double b) {
        return (a == (int64_t)b) && ((double)a == b);
    }
    
    bool equalityAsReals(uint64_t a, double b) {
        return (a == (uint64_t)b) && ((double)a == b);
    }
    
    bool operator==(const Value& a, const Value& b) {
        if (a.type != b.type)
            return false;
        switch (a.type) {
            case VALUE_NIL:
            case VALUE_BOOL:
            case VALUE_INT64:
                return a.as.int64 == b.as.int64;
            case VALUE_OBJECT:
                return a.as.object == b.as.object;
        }
    }
    
    void Value::shade() const {
        gc::shade(as_object());
    }
    
} // namespace lox
