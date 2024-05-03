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
    
    // TODO: for sanity, mapping object type enum back to virtual functions
    // but, this may be a pessimization
    
    /*
#define OBJECT_TYPE(value) (value.as_object()->type)
    
#define IS_BOUND_METHOD(value) isObjectType(value, OBJECT_BOUND_METHOD)
#define IS_CLASS(value) isObjectType(value, OBJECT_CLASS)
#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_INSTANCE(value) isObjectType(value, OBJECT_INSTANCE)
#define IS_NATIVE(value) isObjectType(value, OBJECT_NATIVE)
#define IS_STRING(value) isObjectType(value, OBJECT_STRING)
*/

#define IS_BOUND_METHOD(value) (reinterpret_cast<ObjectBoundMethod*>(value.as_object()))
#define IS_CLASS(value) (reinterpret_cast<ObjectClass*>(value.as_object()))
#define IS_CLOSURE(value) (reinterpret_cast<ObjectClosure*>(value.as_object()))
#define IS_FUNCTION(value) (reinterpret_cast<ObjectFunction*>(value.as_object()))
#define IS_INSTANCE(value) (reinterpret_cast<ObjectInstance*>(value.as_object()))
#define IS_NATIVE(value) (reinterpret_cast<ObjectNative*>(value.as_object()))
#define IS_STRING(value) (reinterpret_cast<ObjectString*>(value.as_object()))
    
    
#define AS_BOUND_METHOD(value) ((ObjectBoundMethod*)value.as_object())
#define AS_CLASS(value) ((ObjectClass*)value.as_object())
#define AS_CLOSURE(value) ((ObjectClosure*)value.as_object())
#define AS_FUNCTION(value) ((ObjectFunction*)value.as_object())
#define AS_INSTANCE(value) ((ObjectInstance*)value.as_object())
#define AS_NATIVE(value) (((ObjectNative*)value.as_object())->function)
#define AS_STRING(value) ((ObjectString*)value.as_object())
#define AS_CSTRING(value) (((ObjectString*)value.as_object())->chars)
    /*
    
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
     */
    
    struct Object {
                
        // explicit Object(ObjectType type);
        virtual ~Object() = default;
        virtual void printObject() = 0;
        virtual bool callObject(int argCount);

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
        virtual void printObject();
        virtual bool callObject(int argCount);
        Value receiver;
        ObjectClosure* method;
    };
    
    struct ObjectClass : Object {
        explicit ObjectClass(ObjectString* name);
        virtual void printObject();
        virtual bool callObject(int argCount);
        ObjectString* name;
        Table methods;
    };
    
    struct ObjectClosure : Object {
        virtual void printObject();
        virtual bool callObject(int argCount);
        ObjectFunction* function;
        int upvalueCount;
        ObjectUpvalue* upvalues[];  // flexible array member
        explicit ObjectClosure(ObjectFunction* function);
    };
    
    
    struct ObjectFunction : Object {
        virtual void printObject();
        int arity;
        int upvalueCount;
        Chunk chunk;
        ObjectString* name;
        ObjectFunction();
    };
    
    struct ObjectInstance : Object {
        virtual void printObject();
        ObjectClass* class_;
        Table fields;
        explicit ObjectInstance(ObjectClass* class_);
    };
    
    struct ObjectNative : Object {
        virtual void printObject();
        virtual bool callObject(int argCount);
        NativeFn function;
        explicit ObjectNative(NativeFn function);
    };
    
    struct ObjectRaw : Object {
        virtual void printObject();
        unsigned char bytes[];
        static void* operator new(size_t count, size_t extra) {
            return :: operator new(count + extra);
        }
    };
    
    struct ObjectString final : Object {
        virtual void printObject();
        uint32_t hash;
        uint32_t length;
        char chars[0]; // flexible array member
        explicit ObjectString(uint32_t length);
        ObjectString(uint32_t hash, uint32_t length, const char* chars);
    };
    
    ObjectString* takeString(char* chars, int length);
    ObjectString* copyString(const char* chars, int length);
    
    struct ObjectUpvalue : Object {
        virtual void printObject();
        Value* location;
        Value closed;
        ObjectUpvalue* next;
        explicit ObjectUpvalue(Value* slot);
    };
    
    void printObject(Value value);
    
    //inline bool isObjectType(Value value, ObjectType type) {
    //    return value.is_object() && value.as_object()->type == type;
    //}
    
} // namespacxe lox
    
#endif /* object_hpp */
