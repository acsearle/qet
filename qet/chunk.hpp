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
#include "gc.hpp"
#include "value.hpp"

namespace lox {
    
    struct Chunk {
        
        std::vector<uint8_t> code;
        std::vector<int> lines;
        std::vector<Value> constants;
        
        void write(uint8_t byte, int line);
        size_t add_constant(Value value);
        
        void scan(gc::ScanContext&) const;
        
    }; // struct Chunk
        
} // namespace lox

#endif /* chunk_hpp */
