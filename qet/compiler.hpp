//
//  compiler.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef compiler_hpp
#define compiler_hpp

#include "object.hpp"
#include "vm.hpp"

bool compile(const char* source, Chunk* chunk);

#endif /* compiler_hpp */
