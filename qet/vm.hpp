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
        Table strings;
        ObjectString* initString;
    };
    
    extern GC gc;
    
    constexpr size_t FRAMES_MAX = 64;
    constexpr size_t STACK_MAX  = FRAMES_MAX + UINT8_COUNT;
    
    struct CallFrame {
        ObjectClosure* closure;
        uint8_t* ip;
        Value* slots;
    };
    
    enum InterpretResult {
        INTERPRET_OK,
        INTERPRET_COMPILE_ERROR,
        INTERPRET_RUNTIME_ERROR,
    };
    
    struct VM {

        CallFrame frames[FRAMES_MAX];
        int frameCount;
        
        Value stack[STACK_MAX];
        Value* stackTop;
        Table globals;
        ObjectUpvalue* openUpvalues;

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
        ObjectUpvalue* captureUpvalue(Value* local);
        void closeUpvalues(Value* last);
        void defineMethod(ObjectString* name);
        void concatenate();
        InterpretResult run();
        InterpretResult interpret(const char* source);


        
    };
    
    // extern VM vm;
    
   
    
    void initGC();
    void freeGC();
    
} // namespace lox

#endif /* vm_hpp */
