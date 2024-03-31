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

ObjectBoundMethod* newBoundMethod(Value receiver, 
                               ObjectClosure* method) {
    ObjectBoundMethod* bound = ALLOCATE_OBJECT(ObjectBoundMethod, 
                                         OBJECT_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjectClass* newClass(ObjectString* name) {
    ObjectClass* class_ = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
    class_->name = name;
    initTable(&class_->methods);
    return class_;
}

ObjectClosure* newClosure(ObjectFunction* function) {
    ObjectUpvalue** upvalues = ALLOCATE(ObjectUpvalue*, 
                                     function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }
    
    ObjectClosure* closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjectInstance* newInstance(ObjectClass* class_) {
    ObjectInstance* instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
    instance->class_ = class_;
    initTable(&instance->fields);
    return instance;
}

ObjectFunction* newFunction() {
    ObjectFunction* function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    std::construct_at(&function->chunk);
    return function;
}

ObjectNative* newNative(NativeFn function) {
    ObjectNative* native = ALLOCATE_OBJECT(ObjectNative, OBJECT_NATIVE);
    native->function = function;
    return native;
}

ObjectUpvalue* newUpvalue(Value* slot) {
    ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
    upvalue->closed = Value();
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static ObjectString* allocateString(char* chars, int length, uint32_t hash) {
    ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    
    // TODO: idiom conflates vm roots and gc stack
    gc.roots.push_back(Value(string));
    tableSet(&gc.strings, string, Value());
    gc.roots.pop_back();
    
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
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocateString(chars, length, hash);
}

ObjectString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjectString* interned = tableFindString(&gc.strings, chars, length, hash);
    if (interned != NULL) return interned;
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
    
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
