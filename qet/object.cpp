//
//  object.cpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#include <cstdio>
#include <cstring>

#include "memory.hpp"
#include "object.hpp"
#include "table.hpp"
#include "value.hpp"
#include "vm.hpp"

namespace lox {
    
    Object::Object(ObjectType type) {
        this->type = type;
        isMarked = false;
        next = gc.objects;
        gc.objects = this;
    }
    
    
    ObjectBoundMethod::ObjectBoundMethod(Value receiver,
                                         ObjectClosure* method)
    : Object(OBJECT_BOUND_METHOD)
    , receiver(receiver)
    , method(method) {
    }
    
    ObjectClass::ObjectClass(ObjectString* name)
    : Object(OBJECT_CLASS)
    , name(name) {
        initTable(&methods);
    }
    
    ObjectClosure::ObjectClosure(ObjectFunction* function)
    : Object(OBJECT_CLOSURE)
    , function(function)
    , upvalueCount(function->upvalueCount) {
        for (int i = 0; i < upvalueCount; i++)
            upvalues[i] = nullptr;
    }
    
    ObjectInstance::ObjectInstance(ObjectClass* class_)
    : Object(OBJECT_INSTANCE) {
        this->class_ = class_;
        initTable(&fields);
    }
    
    ObjectFunction::ObjectFunction()
    : Object(OBJECT_FUNCTION)
    , arity(0)
    , upvalueCount(0)
    , name(nullptr) {
    }
    
    ObjectNative::ObjectNative(NativeFn function)
    : Object(OBJECT_NATIVE)
    , function(function) {
    }
    
    ObjectUpvalue::ObjectUpvalue(Value* slot)
    : Object(OBJECT_UPVALUE)
    , closed(Value())
    , location(slot)
    , next(nullptr) {
    }
    
    ObjectString::ObjectString(uint32_t length)
    : Object(OBJECT_STRING)
    , length(length) {
    }
    
    ObjectString::ObjectString(uint32_t hash, uint32_t length, const char* chars)
    : Object(OBJECT_STRING)
    , hash(hash)
    , length(length) {
        memcpy(this->chars, chars, length);
        this->chars[length] = '\0';
        gc.roots.push_back(Value(this));
        tableSet(&gc.strings, this, Value());
        gc.roots.pop_back();
    }
    
    static uint32_t hashString(const char* key, int length) {
        // FNV-1a
        uint32_t hash = 2166136261u;
        for (int i = 0; i < length; i++) {
            hash ^= (uint8_t)key[i];
            hash *= 16777619;
        }
        return hash;
    }
    
    ObjectString* takeString(char* chars, int length) {
        uint32_t hash = hashString(chars, length);
        ObjectString* interned = tableFindString(&gc.strings, chars, length, hash);
        if (interned == nullptr) {
            interned = new(length + 1) ObjectString(hash, length, chars);
        }
        reallocate(chars, length + 1, 0);
        return interned;
    }
    
    ObjectString* copyString(const char* chars, int length) {
        uint32_t hash = hashString(chars, length);
        ObjectString* interned = tableFindString(&gc.strings, chars, length, hash);
        if (interned == nullptr) {
            interned = new(length + 1) ObjectString(hash, length, chars);
        }
        return interned;
    }
    
    static void printFunction(ObjectFunction* function) {
        if (function->name == NULL) {
            printf("<script>");
            return;
        }
        printf("<fn %s>", function->name->chars);
    }
    
    void printObject(Value value) {
        switch (OBJECT_TYPE(value)) {
            case OBJECT_BOUND_METHOD:
                printFunction(AS_BOUND_METHOD(value)->method->function);
                break;
            case OBJECT_CLASS:
                printf("%s", AS_CLASS(value)->name->chars);
                break;
            case OBJECT_CLOSURE:
                printFunction(AS_CLOSURE(value)->function);
                break;
            case OBJECT_FUNCTION:
                printFunction(AS_FUNCTION(value));
                break;
            case OBJECT_INSTANCE:
                printf("%s instance", AS_INSTANCE(value)->class_->name->chars);
                break;
            case OBJECT_NATIVE:
                printf("<native fn>");
                break;
            case OBJECT_RAW:
                printf("<raw buffer>");
                break;
            case OBJECT_STRING:
                printf("%s", AS_CSTRING(value));
                break;
            case OBJECT_UPVALUE:
                printf("upvalue");
                break;
        }
    }
    
} // namespace lox
