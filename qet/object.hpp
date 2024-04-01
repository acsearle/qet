//
//  object.hpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#ifndef object_hpp
#define object_hpp

#include "common.hpp"
#include "chunk.hpp"
#include "table.hpp"
#include "value.hpp"

#define OBJECT_TYPE(value) (value.as_object()->type)

#define IS_BOUND_METHOD(value) isObjectType(value, OBJECT_BOUND_METHOD)
#define IS_CLASS(value) isObjectType(value, OBJECT_CLASS)
#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_INSTANCE(value) isObjectType(value, OBJECT_INSTANCE)
#define IS_NATIVE(value) isObjectType(value, OBJECT_NATIVE)
#define IS_STRING(value) isObjectType(value, OBJECT_STRING)

#define AS_BOUND_METHOD(value) ((ObjectBoundMethod*)value.as_object())
#define AS_CLASS(value) ((ObjectClass*)value.as_object())
#define AS_CLOSURE(value) ((ObjectClosure*)value.as_object())
#define AS_FUNCTION(value) ((ObjectFunction*)value.as_object())
#define AS_INSTANCE(value) ((ObjectInstance*)value.as_object())
#define AS_NATIVE(value) (((ObjectNative*)value.as_object())->function)
#define AS_STRING(value) ((ObjectString*)value.as_object())
#define AS_CSTRING(value) (((ObjectString*)value.as_object())->chars)

#define ENUMERATE_X_OBJECT \
    X(BOUND_METHOD)\
    X(CLASS)\
    X(CLOSURE)\
    X(FUNCTION)\
    X(INSTANCE)\
    X(NATIVE)\
    X(STRING)\
    X(UPVALUE)\

#define X(Z) OBJECT_##Z,
enum ObjectType { ENUMERATE_X_OBJECT };
#undef X

#define X(Z) [OBJECT_##Z] = "OBJECT_" #Z,
constexpr const char* ObjectTypeCString[] = { ENUMERATE_X_OBJECT };
#undef X


struct Object {
    
    // TODO: Object conflates two responsibilities, being a variant, and being GC
    // The variant could live in the Value, and we would pass around Value as a
    // fatter pointer
    
    // Variant
    
    ObjectType type; // 4
    
    // Garbage collection
    
    bool isMarked;   // 1
                     // padding 3
    Object* next;    // 8

    // TODO: tagged pointer would save nothing given the type
    
};

struct ObjectString;

struct ObjectFunction : Object {
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjectString* name;
};

using NativeFn = Value (*)(int argCount, Value* args);

struct ObjectNative : Object {
    NativeFn function;
};

struct ObjectString : Object {
    int length;
    char* chars;
    uint32_t hash;
};

struct ObjectUpvalue : Object {
    Value* location;
    Value closed;
    ObjectUpvalue* next;
};

struct ObjectClosure : Object {
    ObjectFunction* function;
    ObjectUpvalue** upvalues;
    int upvalueCount;
};

struct ObjectClass : Object {
    ObjectString* name;
    Table methods;
};

struct ObjectInstance : Object {
    ObjectClass* class_;
    Table fields;
};

struct ObjectBoundMethod : Object {
    Value receiver;
    ObjectClosure* method;
};

ObjectBoundMethod* newBoundMethod(Value receiver, 
                               ObjectClosure* method);
ObjectClass* newClass(ObjectString* name);
ObjectClosure* newClosure(ObjectFunction* function);
ObjectFunction* newFunction();
ObjectInstance* newInstance(ObjectClass* class_);
ObjectNative* newNative(NativeFn function);
ObjectString* takeString(char* chars, int length);
ObjectString* copyString(const char* chars, int length);
ObjectUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
    return value.is_object() && value.as_object()->type == type;
}

#endif /* object_hpp */
