//
//  object.hpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#ifndef object_hpp
#define object_hpp

#include "array.hpp"
#include "chunk.hpp"
#include "common.hpp"
#include "memory.hpp"
#include "table.hpp"
#include "value.hpp"

namespace lox {
    
    struct Object;
    
    struct ObjectBoundMethod;
    struct ObjectClass;
    struct ObjectClosure;
    struct ObjectFunction;
    struct ObjectInstance;
    struct ObjectNative;
    struct ObjectString;
    struct ObjectUpvalue;
    
    using NativeFn = Value (*)(int argCount, Value* args);
    
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
X(RAW)\
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
        
        // If we abandon finalizers, we can do without the type?
        
        // Variant
        
        ObjectType type; // 4
        
        // Garbage collection
        
        bool isMarked;   // 1
                         // padding 3
        Object* next;    // 8
        
        // TODO: tagged pointer would save nothing given the type
        
        explicit Object(ObjectType type);
        
        static void* operator new(size_t count, int extra) {
            return reallocate(nullptr, 0, count + extra);
        }
        static void operator delete(void* ptr, size_t count) {
            reallocate(ptr, count, 0);
        }
        
    };
    
    //template<typename T>
    //struct ObjectArray : Object {
    //    Array<T> array;
    //};
    
    struct ObjectBoundMethod : Object {
        ObjectBoundMethod(Value receiver, ObjectClosure* method);
        Value receiver;
        ObjectClosure* method;
    };
    
    struct ObjectClass : Object {
        explicit ObjectClass(ObjectString* name);
        ObjectString* name;
        Table methods;
    };
    
    struct ObjectClosure final : Object {
        ObjectFunction* function;
        int upvalueCount;
        ObjectUpvalue* upvalues[];  // flexible array member
        explicit ObjectClosure(ObjectFunction* function);
    };
    
    
    struct ObjectFunction : Object {
        int arity;
        int upvalueCount;
        Chunk chunk;
        ObjectString* name;
        ObjectFunction();
    };
    
    struct ObjectInstance : Object {
        ObjectClass* class_;
        Table fields;
        explicit ObjectInstance(ObjectClass* class_);
    };
    
    struct ObjectNative : Object {
        NativeFn function;
        explicit ObjectNative(NativeFn function);
    };
    
    struct ObjectRaw : Object {
        unsigned char bytes[];
        static void* operator new(size_t count, size_t extra) {
            return :: operator new(count + extra);
        }
    };
    
    struct ObjectString final : Object {
        uint32_t hash;
        uint32_t length;
        char chars[0]; // flexible array member
        explicit ObjectString(uint32_t length);
        ObjectString(uint32_t hash, uint32_t length, const char* chars);
    };
    
    ObjectString* takeString(char* chars, int length);
    ObjectString* copyString(const char* chars, int length);
    
    struct ObjectUpvalue : Object {
        Value* location;
        Value closed;
        ObjectUpvalue* next;
        explicit ObjectUpvalue(Value* slot);
    };
    
    void printObject(Value value);
    
    inline bool isObjectType(Value value, ObjectType type) {
        return value.is_object() && value.as_object()->type == type;
    }
    
} // namespacxe lox
    
#endif /* object_hpp */
