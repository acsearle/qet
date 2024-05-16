//
//  vm.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef vm_hpp
#define vm_hpp

#include "object.hpp"
#include "table.hpp"
#include "value.hpp"

namespace lox {
    
    struct GC {
        //Table strings;
        ObjectString* initString;
    };
    
    extern GC gc;
    
    constexpr size_t FRAMES_MAX = 64;
    constexpr size_t STACK_MAX  = FRAMES_MAX + UINT8_COUNT;
    
    struct CallFrame {
        gc::StrongPtr<ObjectClosure> closure;
        uint8_t* ip;
        AtomicValue* slots;
    };
    
    enum InterpretResult {
        INTERPRET_OK,
        INTERPRET_COMPILE_ERROR,
        INTERPRET_RUNTIME_ERROR,
    };
    
    struct VM : gc::Object {

        CallFrame frames[FRAMES_MAX];
        int frameCount;
        
        AtomicValue stack[STACK_MAX];
        AtomicValue* stackTop;
        Table globals;
        gc::StrongPtr<ObjectUpvalue> openUpvalues;

        // public?
        
        void initVM();
        void freeVM();
        void push(Value value);
        Value pop();
        Value peek(int distance);

        // private?
        
        void resetStack();
        void runtimeError(const char* format, ...);
        void defineNative(const char* name, NativeFn function);
        bool call(ObjectClosure* closure, int argCount);
        bool callValue(Value callee, int argCount);
        bool invokeFromClass(ObjectClass* class_, ObjectString* name, int argCount);
        bool invoke(ObjectString* name, int argCount);
        bool bindMethod(ObjectClass* class_, ObjectString* name);
        ObjectUpvalue* captureUpvalue(AtomicValue* local);
        void closeUpvalues(AtomicValue* last);
        void defineMethod(ObjectString* name);
        void concatenate();
        InterpretResult run();
        InterpretResult interpret(const char* first, const char* last);



        void _gc_scan(gc::ScanContext&) const override;
        virtual std::size_t _gc_bytes() const override;

        
    };
    
    // extern VM vm;
    
   
    
    void initGC();
    void freeGC();
    
} // namespace lox

#endif /* vm_hpp */
