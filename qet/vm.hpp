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

struct GC {
    Table strings;
    ObjectString* initString;
    size_t bytesAllocated;
    size_t nextGC;
    
    Object* objects;
    std::vector<Object*> grayStack;
    std::vector<Value> roots;
};

extern GC gc;

constexpr size_t FRAMES_MAX = 64;
constexpr size_t STACK_MAX  = FRAMES_MAX + UINT8_COUNT;

struct CallFrame {
    ObjectClosure* closure;
    uint8_t* ip;
    Value* slots;
};

struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    ObjectUpvalue* openUpvalues;
    
};

extern VM vm;

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
};

void initGC();
void freeGC();
void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
Value peek(int distance);

#endif /* vm_hpp */
