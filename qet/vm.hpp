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

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX + UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Obj* objects;
} VM;

extern VM vm;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;


void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
Value peek(int distance);

#endif /* vm_hpp */
