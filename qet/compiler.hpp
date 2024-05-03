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

namespace lox {
    
    ObjectFunction* compile(const char* source);
    void markCompilerRoots();
    
}

#endif /* compiler_hpp */
