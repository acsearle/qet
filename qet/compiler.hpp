//
//  compiler.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef compiler_hpp
#define compiler_hpp

#include "object.hpp"

namespace lox {
    
    ObjectFunction* compile(const char* first, const char* last);
    
}

#endif /* compiler_hpp */
