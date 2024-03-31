//
//  chunk.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef chunk_hpp
#define chunk_hpp

#include <vector>

#include "common.hpp"
#include "value.hpp"

#define E \
    X(CONSTANT)\
    X(NIL)\
    X(TRUE)\
    X(FALSE)\
    X(POP)\
    X(GET_LOCAL)\
    X(SET_LOCAL)\
    X(GET_GLOBAL)\
    X(DEFINE_GLOBAL)\
    X(SET_GLOBAL)\
    X(GET_UPVALUE)\
    X(SET_UPVALUE)\
    X(GET_PROPERTY)\
    X(SET_PROPERTY)\
    X(GET_SUPER)\
    X(EQUAL)\
    X(GREATER)\
    X(LESS)\
    X(ADD)\
    X(SUBTRACT)\
    X(MULTIPLY)\
    X(DIVIDE)\
    X(NOT)\
    X(NEGATE)\
    X(PRINT)\
    X(JUMP)\
    X(JUMP_IF_FALSE)\
    X(LOOP)\
    X(CALL)\
    X(INVOKE)\
    X(SUPER_INVOKE)\
    X(CLOSURE)\
    X(CLOSE_UPVALUE)\
    X(RETURN)\
    X(CLASS)\
    X(INHERIT)\
    X(METHOD)\

#define X(Z) OPCODE_##Z,
enum OpCode : uint8_t { E };
#undef X

#define X(Z) [OPCODE_##Z] = "OPCODE_" #Z,
constexpr const char* OpCodeCString[] = { E };
#undef X

#undef E

struct Chunk {
            
    std::vector<uint8_t> code;
    std::vector<int> lines;
    
    std::vector<Value> constants;
    
    void write(uint8_t byte, int line);
    size_t add_constant(Value value);
    
    // TODO: gc; account for vector allocations in total
    // TODO: better lines (provide a pointer into the source for each opecode?)
    
};

#endif /* chunk_hpp */
