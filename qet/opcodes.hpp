//
//  opcodes.hpp
//  qet
//
//  Created by Antony Searle on 1/4/2024.
//

#ifndef opcodes_hpp
#define opcodes_hpp

#include <cstdint>

namespace lox {
    
#define ENUMERATEX_OPCODES \
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
    enum OpCode : uint8_t { ENUMERATEX_OPCODES };
#undef X
    
#define X(Z) [OPCODE_##Z] = "OPCODE_" #Z,
    constexpr const char* OpCodeCString[] = { ENUMERATEX_OPCODES };
#undef X
    
} // namespace lox
    
#endif /* opcodes_hpp */
