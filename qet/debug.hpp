#ifndef debug_hpp
#define debug_hpp

#include "chunk.hpp"

void disassembleChunk(Chunk* chunk, const char* name);

ptrdiff_t disassembleInstruction(Chunk* chunk, ptrdiff_t offset);


#endif /* debug_hpp */
