//
//  chunk.cpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#include "chunk.hpp"
#include "memory.hpp"
#include "vm.hpp"

void Chunk::write(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

size_t Chunk::add_constant(Value value) {
    push(value);
    constants.push_back(value);
    pop();
    return constants.size() - 1;
}

