//
//  memory.hpp
//  qet
//
//  Created by Antony Searle on 20/3/2024.
//

#ifndef memory_hpp
#define memory_hpp

#include "common.hpp"

#define ALLOCATE(type, count)\
    (type*)reallocate(NULL, 0, sizeof(type) * count)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : capacity * 2)

namespace lox {
    
    struct Value;
    struct Object;
    
    void* reallocate(void* pointer, size_t oldSize, size_t newSize);
    void markObject(Object* object);
    void markValue(Value value);
    void collectGarbage();
    void freeObjects();
    
}

#endif /* memory_hpp */
