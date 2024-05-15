//
//  object.cpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#include <cstdio>
#include <cstring>

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
    
    void ObjectBoundMethod::scan(gc::ScanContext &context) const {
        lox::scan(receiver, context);
        context.push(method);
    }
    
    
    ObjectClass::ObjectClass(ObjectString* name)
    : name(name) {
        initTable(&methods);
    }

    void ObjectClass::scan(gc::ScanContext &context) const {
        context.push(name);
        methods.scan(context);
    }
    

    ObjectClosure::ObjectClosure(ObjectFunction* function)
    : function(function)
    , upvalueCount(function->upvalueCount) {
        for (int i = 0; i < upvalueCount; i++)
            upvalues[i] = nullptr;
    }
    
    void ObjectClosure::scan(gc::ScanContext &context) const {
        context.push(function);
        for (int i = 0; i < upvalueCount; i++)
            context.push(upvalues[i]);
    }
    
    ObjectFunction::ObjectFunction()
    : arity(0)
    , upvalueCount(0)
    , name(nullptr) {
    }
    
    void ObjectFunction::scan(gc::ScanContext &context) const {
        lox::scan(chunk, context);
        context.push(name);
    }

    ObjectInstance::ObjectInstance(ObjectClass* class_) {
        this->class_ = class_;
        initTable(&fields);
    }
    
    void ObjectInstance::scan(gc::ScanContext &context) const {
        context.push(class_);
        fields.scan(context);
    }
    
    ObjectNative::ObjectNative(NativeFn function)
    : function(function) {
    }
    
    void ObjectNative::scan(gc::ScanContext&) const {
    }
    
    void ObjectRaw::scan(gc::ScanContext&) const {        
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
    
    void ObjectString::scan(gc::ScanContext& context) const {        
    }
    
    ObjectUpvalue::ObjectUpvalue(AtomicValue* slot)
    : closed(Value())
    , location(slot)
    , next(nullptr) {
    }
    
    void ObjectUpvalue::scan(gc::ScanContext& context) const {
        using lox::scan;
        scan(*location, context);
        scan(closed, context);
        scan(next, context);
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
            interned = new(gc::extra_val_t{(std::size_t)length + 1}) ObjectString(hash, length, chars);
        }
        operator delete(chars);
        return interned;
    }
    
    ObjectString* copyString(const char* chars, int length) {
        uint32_t hash = hashString(chars, length);
        ObjectString* interned = tableFindString(&gc.strings, chars, length, hash);
        if (interned == nullptr) {
            interned = new(gc::extra_val_t{(std::size_t)length + 1}) ObjectString(hash, length, chars);
        }
        return interned;
    }
    
    static void printFunction(const ObjectFunction* function) {
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
