//
//  vm.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef vm_hpp
#define vm_hpp

#include "chunk.hpp"
#include "table.hpp"
#include "value.hpp"

constexpr size_t STACK_MAX = 256;

struct VM {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Obj* objects;
};

extern VM vm;

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
};


void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
Value peek(int distance);

#endif /* vm_hpp */
