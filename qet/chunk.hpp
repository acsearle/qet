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
    
    template<typename T>
    void scan(const std::vector<T>&, gc::ScanContext& context);

    struct Source
    : gc::Leaf {
                
        std::vector<char> text;
        std::vector<char> name;
        
    };
    
    // Chunks only get stored as members of functions
    
    // A chunk stores the bytecode and constants for one function
    //
    // For debugging, it contains metadata relating each bytecode to a source
    // location, and points to a shared source object that holds the program
    // text and some metadata
    
    struct Chunk {
        
        std::vector<uint8_t> code;    // <-- bytecode
        std::vector<Value> constants; // <-- function literals table

        
        void    write(uint8_t byte, int line, const char* start);
        size_t  add_constant(Value value);

        
        // cold/debug
        
        std::vector<int>            lines           ; // <-- line number from tokenizer
        std::vector<const char*>    where           ; // <-- location in text provoking bytecode
        Source*                     source = nullptr; // <-- shared source code
        
                
    }; // struct Chunk
    
    void scan(const Chunk&, gc::ScanContext&);
    
    template<typename T>
    void scan(const std::vector<T>& v, gc::ScanContext& context) {
        for (auto&& x : v)
            scan(x, context);
    }

        
} // namespace lox

#endif /* chunk_hpp */
