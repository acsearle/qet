//
//  rational.hpp
//  qet
//
//  Created by Antony Searle on 31/3/2024.
//

#ifndef rational_hpp
#define rational_hpp

#include <cassert>
#include <cstdint>
#include <algorithm>

inline unsigned long long gcdll(unsigned long long u, unsigned long long v) {
    if (u == 0)
        return v;
    if (v == 0)
        return u;
    int i = __builtin_ctzll(u); u >>= i;
    int j = __builtin_ctzll(v); v >>= j;
    int k = std::min(i, j);
    for (;;) {
        assert(u & v & 1);
        if (u > v)
            std::swap(u, v);
        v -= u;
        assert(!(v & 1));
        if (v == 0)
            return u << k;
        v >>= __builtin_ctzll(v);
    }
}

inline unsigned gcd(unsigned u, unsigned v) {
    if (u == 0)
        return v;
    if (v == 0)
        return u;
    int i = __builtin_ctz(u); u >>= i;
    int j = __builtin_ctz(v); v >>= j;
    int k = std::min(i, j);
    for (;;) {
        assert(u & v & 1);
        if (u > v)
            std::swap(u, v);
        v -= u;
        assert(!(v & 1));
        if (v == 0)
            return u << k;
        v >>= __builtin_ctz(v);
    }
}

struct Rational {
    
    int32_t p;
    uint32_t q;
    
    
};

Rational operator+(const Rational& a, const Rational& b) {
    
    int64_t p = (int64_t)a.p * (int64_t)b.q + (int64_t)b.p * (int64_t)a.q;
    uint64_t q = (uint64_t)a.q * (uint64_t)b.q;

    uint64_t u = abs(p);
    uint64_t v = q;
    int i = __builtin_ctzll(u); u >>= i;
    int j = __builtin_ctzll(v); v >>= j;
    int k = std::min(i, j);
    p >>= k;
    q >>= k;
    for (;;) {
        assert(u & v & 1);
        if (u > v)
            std::swap(u, v);
        v -= u;
        assert(!(v & 1));
        if (v == 0)
            break;
        v >>= __builtin_ctzll(v);
    }
    p /= u;
    q /= v;
    
    int32_t s = (int32_t) p; assert(s == p);
    uint32_t t = (uint32_t) q; assert(t == q);

    return Rational{ (int32_t)p, (uint32_t)q };
}

#endif /* rational_hpp */
