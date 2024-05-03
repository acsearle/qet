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
    
    /*
    Object::Object(ObjectType type) {
        this->type = type;
        isMarked = false;
        next = gc.objects;
        gc.objects = this;
    }
     */
    
    
    ObjectBoundMethod::ObjectBoundMethod(Value receiver,
                                         ObjectClosure* method)
    : receiver(receiver)
    , method(method) {
    }
    
    ObjectClass::ObjectClass(ObjectString* name)
    : name(name) {
        initTable(&methods);
    }
    
    ObjectClosure::ObjectClosure(ObjectFunction* function)
    : function(function)
    , upvalueCount(function->upvalueCount) {
        for (int i = 0; i < upvalueCount; i++)
            upvalues[i] = nullptr;
    }
    
    ObjectInstance::ObjectInstance(ObjectClass* class_) {
        this->class_ = class_;
        initTable(&fields);
    }
    
    ObjectFunction::ObjectFunction()
    : arity(0)
    , upvalueCount(0)
    , name(nullptr) {
    }
    
    ObjectNative::ObjectNative(NativeFn function)
    : function(function) {
    }
    
    ObjectUpvalue::ObjectUpvalue(Value* slot)
    : closed(Value())
    , location(slot)
    , next(nullptr) {
    }
    
    ObjectString::ObjectString(uint32_t length)
    : length(length) {
    }
    
    ObjectString::ObjectString(uint32_t hash, uint32_t length, const char* chars)
    : hash(hash)
    , length(length) {
        memcpy(this->chars, chars, length);
        this->chars[length] = '\0';
        //gc.roots.push_back(Value(this));
        tableSet(&gc.strings, this, Value());
        //gc.roots.pop_back();
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
        value.as_object()->printObject();
    }
    
    void ObjectBoundMethod::printObject() {
        printFunction(method->function);
    }
    
    void ObjectClass::printObject() {
        printf("%s", name->chars);
    }

    void ObjectClosure::printObject() {
        printFunction(function);
    }

    void ObjectFunction::printObject() {
        printFunction(this);
    }
    
    void ObjectInstance::printObject() {
        printf("%s instance", class_->name->chars);
    }

    void ObjectNative::printObject() {
        printf("<native fn>");
    }
    
    void ObjectRaw::printObject() {
        printf("<raw buffer>");
    }

    void ObjectString::printObject() {
        printf("%s", chars);
    }
    
    void ObjectUpvalue::printObject() {
        printf("upvalue");
    }


} // namespace lox
