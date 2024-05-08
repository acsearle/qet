//
//  array.hpp
//  qet
//
//  Created by Antony Searle on 2/4/2024.
//

#ifndef array_hpp
#define array_hpp

#include <cassert>

#include "common.hpp"
#include "memory.hpp"

namespace lox {
    
    // Box<std::array<>> or Box<std::vector<>>?
    
    template<typename T>
    struct Array {
        int capacity;
        int count;
        T* elements;
    };
    
    template<typename T>
    void initValueArray(Array<T>* self) {
        self->capacity = 0;
        self->count = 0;
        self->elements = nullptr;
    }
    
    /*
    template<typename T>
    void pushArray(Array<T>* self, T value) {
        if (self->capacity < self->count + 1) {
            int oldCapacity = self->capacity;
            self->capacity = GROW_CAPACITY(oldCapacity);
            self->elements = (T*)reallocate(self->elements, oldCapacity * sizeof(T), self->capacity * sizeof(T));
        }
        self->elements[self->count] = value;
        self->count++;
    }
     */
    
    template<typename T>
    void popArray(Array<T>* self) {
        assert(self && self->count);
        self->count--;
    }
    
    template<typename T>
    void freeValueArray(Array<T>* self) {
        reallocate(self->elements, sizeof(T) * self->count, 0);
        initValueArray(self);
    }
    
} // namespace qet

#endif /* array_hpp */
