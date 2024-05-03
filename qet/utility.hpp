//
//  utility.hpp
//  gch
//
//  Created by Antony Searle on 28/4/2024.
//

#ifndef utility_hpp
#define utility_hpp

#include <cassert>
#include <cstdint>
#include <memory>

namespace gc {
    
    template<typename T, typename Base>
    struct BitMap : Base {
        
        std::uint64_t _bits;
        T _elements[0]; // flexible array member
        
        virtual ~BitMap() {
            std::destroy(begin(), end());
        }
        
        static std::uint64_t _flag(int key) {
            assert(key == (key & 63));
            return (std::uint64_t{1} << key);
        }
        
        static std::uint64_t _mask(int key) {
            return _flag(key) - 1;
        }
                
        int _position(int key) const {
            return __builtin_popcountll(_mask(key) & _bits);
        }

        bool empty() const {
            return !_bits;
        }
                
        int count(int key) const {
            return _flag(key) & _bits ? 1 : 0;
        }

        int size() const {
            return __builtin_popcountll(_bits);
        }
                
        T* begin() {
            return _elements;
        }
        
        T* end() {
            return _elements + size();
        }

        T* to(int key) const {
            return _elements + _position(key);
        }

        T& operator[](int key) const {
            return _elements[_position(key)];
        }
        
        void clear() {
            std::destroy(begin(), end());
            _bits = 0;
        }
        
        int erase(int key) {
            std::uint64_t a = _flag(key);
            if (a & _bits) {
                _bits ^= a;
                --a; // a is now mask
                int b = __builtin_popcountll(_bits & a);
                int c = __builtin_popcountll(_bits & ~a);
                std::destroy_at(_elements + b);
                std::memmove(_elements + b, _elements + b + 1, c - b);
                return 1;
            } else {
                return 0;
            }
        }
        
        
    };
    
    
}

#endif /* utility_hpp */
