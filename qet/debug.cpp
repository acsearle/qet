#include <cstdio>

#include "debug.hpp"
#include "object.hpp"
#include "opcodes.hpp"
#include "value.hpp"

static ptrdiff_t simpleInstruction(Chunk* chunk, ptrdiff_t offset) {
    printf("\n");
    return offset + 1;
}

ptrdiff_t constantInstruction(Chunk* chunk, ptrdiff_t offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%4d '", constant);
    printValue(chunk->constants[constant]);
    printf("'\n");
    return offset + 2;
}

ptrdiff_t invokeInstruction(Chunk* chunk, ptrdiff_t offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("(%d args) %4d '", argCount, constant);
    printValue(chunk->constants[constant]);
    printf("'\n");
    return offset + 3;
}

ptrdiff_t byteInstruction(Chunk* chunk, ptrdiff_t offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%4d\n", slot);
    return offset + 2;
}

ptrdiff_t jumpInstruction(Chunk* chunk, ptrdiff_t offset) {
    int sign = 1;
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%4ld -> %ld\n", offset, offset + 3 + sign * jump);
    return offset + 3;
}

ptrdiff_t loopInstruction(Chunk* chunk, ptrdiff_t offset) {
    int sign = -1;
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%4ld -> %ld\n", offset, offset + 3 + sign * jump);
    return offset + 3;
}

ptrdiff_t closureInstruction(Chunk* chunk, ptrdiff_t offset) {
    offset++;
    uint8_t constant =  chunk->code[offset++];
    printf("%4d ", constant);
    printValue(chunk->constants[constant]);
    printf("\n");
    
    ObjectFunction* function = AS_FUNCTION(chunk->constants[constant]);
    for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04ld      |                     %s %d\n",
               offset - 2, isLocal ? "local  " : "upvalue", index);
    }
    
    return offset;
}

using disassembleFunctionType = ptrdiff_t (*)(Chunk* chunk, ptrdiff_t offset);

disassembleFunctionType disassembleFunctionTable[UINT8_COUNT] = {
    [OPCODE_CONSTANT] = constantInstruction,
    [OPCODE_NIL] = simpleInstruction,
    [OPCODE_TRUE] = simpleInstruction,
    [OPCODE_FALSE] = simpleInstruction,
    [OPCODE_POP] = simpleInstruction,
    [OPCODE_GET_LOCAL] = byteInstruction,
    [OPCODE_SET_LOCAL] = byteInstruction,
    [OPCODE_GET_GLOBAL] = constantInstruction,
    [OPCODE_DEFINE_GLOBAL] = constantInstruction,
    [OPCODE_SET_GLOBAL] = constantInstruction,
    [OPCODE_GET_UPVALUE] = byteInstruction,
    [OPCODE_SET_UPVALUE] = byteInstruction,
    [OPCODE_GET_PROPERTY] = constantInstruction,
    [OPCODE_SET_PROPERTY] = constantInstruction,
    [OPCODE_GET_SUPER] = constantInstruction,
    [OPCODE_EQUAL] = simpleInstruction,
    [OPCODE_GREATER] = simpleInstruction,
    [OPCODE_LESS] = simpleInstruction,
    [OPCODE_ADD] = simpleInstruction,
    [OPCODE_SUBTRACT] = simpleInstruction,
    [OPCODE_MULTIPLY] = simpleInstruction,
    [OPCODE_DIVIDE] = simpleInstruction,
    [OPCODE_NOT] = simpleInstruction,
    [OPCODE_NEGATE] = simpleInstruction,
    [OPCODE_PRINT] = simpleInstruction,
    [OPCODE_JUMP] = jumpInstruction,
    [OPCODE_JUMP_IF_FALSE] = jumpInstruction,
    [OPCODE_LOOP] = loopInstruction,
    [OPCODE_CALL] = byteInstruction,
    [OPCODE_INVOKE] = invokeInstruction,
    [OPCODE_SUPER_INVOKE] = invokeInstruction,
    [OPCODE_CLOSURE] = closureInstruction,
    [OPCODE_CLOSE_UPVALUE] = simpleInstruction,
    [OPCODE_RETURN] = simpleInstruction,
    [OPCODE_CLASS] = constantInstruction,
    [OPCODE_INHERIT] = simpleInstruction,
    [OPCODE_METHOD] = constantInstruction,
};

ptrdiff_t disassembleInstruction(Chunk* chunk, ptrdiff_t offset) {
    printf("%04ld ", offset);
    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%04d ", chunk->lines[offset]);
    }
    
    uint8_t instruction = chunk->code[offset];
    disassembleFunctionType fp = disassembleFunctionTable[instruction];
    if (fp != nullptr) {
        const char* name = OpCodeCString[chunk->code[offset]];
        printf("%-16s ", name);
        return fp(chunk, offset);
    } else {
        printf("Unknown opcode %d\n", (int) instruction);
        return offset + 1;
    }
}

void disassembleChunk(Chunk* chunk, const char *name) {
    printf("== %s ==\n", name);
    for (ptrdiff_t offset = 0; offset < chunk->code.size();) {
        offset = disassembleInstruction(chunk, offset);
    }
}
