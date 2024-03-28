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

ObjFunction* compile(const char* source);

#endif /* compiler_hpp */
