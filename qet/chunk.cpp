//
//  chunk.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include "chunk.hpp"
#include "vm.hpp"

namespace lox {
    
    void Chunk::write(uint8_t byte, int line, const char* start) {
        code.push_back(byte);
        lines.push_back(line);
        this->where.push_back(start);
    }
    
    size_t Chunk::add_constant(Value value) {
        value.shade();
        constants.push_back(value);
        return constants.size() - 1;
    }
    
    void Chunk::scan(gc::ScanContext& context) const {
        for (const Value& value : constants)
            value.scan(context);
    }
    
} // namespace lox

