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

/*
#define ALLOCATE_OBJECT(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Object* allocateObject(size_t size, ObjectType type) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    
    object->next = gc.objects;
    gc.objects = object;
    
#ifdef DEBUG_LOG_GC
    printf("%p allocateObject %zu bytes for ObjectType %d\n", (void*)object, size, type);
#endif
    
    return object;
}
*/
 
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

ObjectClosure* newObjectClosure(ObjectFunction* function) {
    ObjectClosure* closure = (ObjectClosure*) operator new(sizeof(ObjectFunction) + function->upvalueCount * sizeof(ObjectUpvalue*));
    new (closure) ObjectClosure(function);
    return closure;
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

ObjectString::ObjectString(const char* chars, int length, uint32_t hash)
: Object(OBJECT_STRING)
, hash(hash)
, length(length) {
    memcpy(this->chars, chars, length);
    this->chars[length] = '\0';
    gc.roots.push_back(Value(this));
    tableSet(&gc.strings, this, Value());
    gc.roots.pop_back();
}

ObjectString* newObjectString(const char* chars, int length, uint32_t hash) {
    ObjectString* string = (ObjectString*) operator new(sizeof(ObjectString) + length + 1);
    new (string) ObjectString(chars, length, hash);
    return string;
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
        interned = newObjectString(chars, length, hash);
    }
    FREE_ARRAY(char, chars, length + 1);
    return interned;
}

ObjectString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjectString* interned = tableFindString(&gc.strings, chars, length, hash);
    if (interned == nullptr) {
        interned = newObjectString(chars, length, hash);
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
        case OBJECT_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJECT_UPVALUE:
            printf("upvalue");
            break;
    }
}
