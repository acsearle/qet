#ifndef debug_hpp
#define debug_hpp

#include "chunk.hpp"

void disassembleChunk(Chunk* chunk, const char* name);

int disassembleInstruction(Chunk* chunk, int offset);


#endif /* debug_hpp */
