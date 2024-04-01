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

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<int> lines;
    std::vector<Value> constants;
    void write(uint8_t byte, int line);
    size_t add_constant(Value value);    
}; // struct Chunk

#endif /* chunk_hpp */
