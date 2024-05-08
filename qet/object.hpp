//
//  object.hpp
//  qet
//
//  Created by Antony Searle on 21/3/2024.
//

#ifndef object_hpp
#define object_hpp

#include <deque>

#include "array.hpp"
#include "chunk.hpp"
#include "common.hpp"
#include "gc.hpp"
#include "memory.hpp"
#include "table.hpp"
#include "value.hpp"

namespace lox {
    
    struct VM;
    
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
    namespace gc2 {
        
        struct Object;
        
        using Color = std::intptr_t;
        
        struct Palette {
            
            Color _white = Color{-1};
            Color _alloc = Color{-1};
            
            Color white() const { return _white; }
            Color gray()  const { return 2; }
            Color black() const { return _white ^ 1; }
            Color alloc() const { return _alloc; }
            
        };
        
        struct Local : Palette {
            
            std::deque<Object*> _log;
            bool _dirty;
            
            
            // log new objects
            void log(Object* object) {
                _log.push_back(object);
            }
            
            // note that an object has been grayed
            void sully() {
                _dirty = true;
            }
            
        }; // struct Local
        
        struct Global : Palette {
            
            std::mutex mutex;
            
            
            
        };
        
        struct ScanContext {
            std::deque<Object*> _log;
            void log(Object* object) {
                _log.push_back(object);
            }
        };
        
        inline thread_local Local local;
        
        struct Object {
            
            mutable std::atomic<std::intptr_t> color;
            
            Object()
            : color(local.alloc()) {
                local.log(this);
            }
            
            Object(const Object& other)
            : Object() {
            }
            
            virtual ~Object() = default;
            
            virtual void shade() const {
                Color expected = local.white();
                if (color.compare_exchange_strong(expected,
                                                  local.gray(),
                                                  std::memory_order_relaxed,
                                                  std::memory_order_relaxed)) {
                    local.sully();
                }
            }
            
            virtual void scan(ScanContext&) const {
            }
            
            virtual Color sweep() const {
                return color.load(std::memory_order_relaxed);
            };
                        
        };
        
        template<typename T, std::enable_if_t<std::is_convertible_v<T*, Object*>, int> = 0>
        struct Leaf : T {
            virtual void shade() const override final {
                Color expected = local.white();
                Color desired = local.black();
                this->color.compare_exchange_strong(expected,
                                                    desired,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed);
            }
        };
        
    } // namespaec gc
     */
    
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
        virtual void scan(gc::ScanContext& context) const override;
        
    };
    
    struct ObjectClass : Object {
        explicit ObjectClass(ObjectString* name);
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        ObjectString* name;
        Table methods;
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    struct ObjectClosure : Object {
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        ObjectFunction* function;
        int upvalueCount;
        ObjectUpvalue* upvalues[];  // flexible array member
        explicit ObjectClosure(ObjectFunction* function);
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    
    struct ObjectFunction : Object {
        virtual void printObject() override;
        int arity;
        int upvalueCount;
        Chunk chunk;
        ObjectString* name;
        ObjectFunction();
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    struct ObjectInstance : Object {
        virtual void printObject() override;
        ObjectClass* class_;
        Table fields;
        explicit ObjectInstance(ObjectClass* class_);
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    struct ObjectNative : Object {
        virtual void printObject() override;
        virtual bool callObject(VM& vm, int argCount) override;
        NativeFn function;
        explicit ObjectNative(NativeFn function);
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    struct ObjectRaw : Object {
        virtual void printObject() override;
        unsigned char bytes[];
        static void* operator new(size_t count, size_t extra) {
            return :: operator new(count + extra);
        }
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    struct ObjectString final : Object {
        virtual void printObject() override;
        uint32_t hash;
        uint32_t length;
        char chars[0]; // flexible array member
        explicit ObjectString(uint32_t length);
        ObjectString(uint32_t hash, uint32_t length, const char* chars);
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    ObjectString* takeString(char* chars, int length);
    ObjectString* copyString(const char* chars, int length);
    
    struct ObjectUpvalue : Object {
        virtual void printObject() override;
        Value* location;
        Value closed;
        ObjectUpvalue* next;
        explicit ObjectUpvalue(Value* slot);
        virtual void scan(gc::ScanContext& context) const override;
    };
    
    void printObject(Value value);
        
} // namespacxe lox
    
#endif /* object_hpp */
