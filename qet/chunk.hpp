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
    
    // Chunks only get stored as members of functions
    
    struct Chunk {
        
        std::vector<uint8_t> code;    // <-- bytecode
        std::vector<Value> constants; // <-- function literals table

        
        // debug cold
        
        // old
        std::vector<int> lines;       // <-- line number from scanner
        
        // new
        // shared source code metadata and text
        std::vector<const char*> where; // <-- location in text provoking bytecode
        
        
        
        void write(uint8_t byte, int line, const char* start);
        size_t add_constant(Value value);
        
        void scan(gc::ScanContext&) const;
        
    }; // struct Chunk
        
} // namespace lox

#endif /* chunk_hpp */
