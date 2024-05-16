//
//  object.hpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#ifndef object_hpp
#define object_hpp

#include <deque>

#include "chunk.hpp"
#include "common.hpp"
#include "gc.hpp"
#include "table.hpp"
#include "value.hpp"

namespace gc {
    namespace _string {
        struct SNode;
    }
}

namespace lox {
    
    // TODO:
    //
    // Relative performance of virtual functions vs switch-on-enum
    //
    // Using virtual functins for prototyping, contra clox
    
    struct VM;
    
    struct Object;
        
    struct ObjectBoundMethod;
    struct ObjectClass;
    struct ObjectClosure;
    struct ObjectFunction;
    struct ObjectInstance;
    struct ObjectNative;
    using ObjectString = ::gc::_string::SNode;
    struct ObjectUpvalue;
    
    struct AtomicValue;
    
    using NativeFn = Value (*)(int argCount, AtomicValue* args);
    
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

    struct Object : gc::Object {
        virtual void printObject() = 0;
        virtual bool callObject(VM& vm, int argCount);
    };
        
    struct ObjectBoundMethod : Object {
        ObjectBoundMethod(Value receiver, ObjectClosure* method);
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        Value receiver;
        ObjectClosure* method;
        virtual void _gc_scan(gc::ScanContext& context) const override;
        virtual std::size_t _gc_bytes() const override;
    };
    
    struct ObjectClass : Object {
        explicit ObjectClass(ObjectString* name);
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        ObjectString* name;
        Table methods;
        virtual void _gc_scan(gc::ScanContext& context) const override;
        virtual std::size_t _gc_bytes() const override;
    };
    
    struct ObjectClosure : Object {
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        ObjectFunction* function;
        int upvalueCount;
        ObjectUpvalue* upvalues[0];  // flexible array member
        explicit ObjectClosure(ObjectFunction* function);
        virtual void _gc_scan(gc::ScanContext& context) const override;
        virtual std::size_t _gc_bytes() const override;
    };
    
    
    struct ObjectFunction : Object {
        virtual void printObject() override;
        int arity;
        int upvalueCount;
        Chunk chunk;
        ObjectString* name;
        ObjectFunction();
        virtual void _gc_scan(gc::ScanContext& context) const override;
        virtual std::size_t _gc_bytes() const override;
    };
    
    struct ObjectInstance : Object {
        virtual void printObject() override;
        ObjectClass* class_;
        Table fields;
        explicit ObjectInstance(ObjectClass* class_);
        virtual void _gc_scan(gc::ScanContext& context) const override;
        virtual std::size_t _gc_bytes() const override;
};
    
    struct ObjectNative : gc::Leaf<Object> {
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        NativeFn function;
        explicit ObjectNative(NativeFn function);
        virtual std::size_t _gc_bytes() const override;
    };
        
    ObjectString* takeString(char* chars, int length);
    ObjectString* copyString(const char* chars, int length);
    
    struct ObjectUpvalue : Object {
        virtual void printObject() override;
        AtomicValue* location;
        AtomicValue closed;
        ObjectUpvalue* next;
        explicit ObjectUpvalue(AtomicValue* slot);
        virtual void _gc_scan(gc::ScanContext& context) const override;
        virtual std::size_t _gc_bytes() const override;
    };
    
    void printObject(Value value);
    
} // namespacxe lox
    
#endif /* object_hpp */
