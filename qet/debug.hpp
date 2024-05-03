#ifndef debug_hpp
#define debug_hpp

#include "chunk.hpp"

namespace lox {
    
    ptrdiff_t disassembleInstruction(Chunk* chunk, ptrdiff_t offset);
    void disassembleChunk(Chunk* chunk, const char* name);
    
}

#endif /* debug_hpp */
