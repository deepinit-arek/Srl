#ifndef SRL_HASH_H
#define SRL_HASH_H

#include <cstdint>
#include <cstddef>

namespace Srl { namespace Tools {

    /* Fowler–Noll–Vo hash function, suggested parameters from http://isthe.com/chongo/tech/comp/fnv/ */

    template<size_t machine_word_size> struct HashParamsFnv1;

    template<> struct HashParamsFnv1<8> {
        static const uint64_t Prime = 0x100000001B3;
        static const uint64_t Base  = 0xCBF29CE484222325;
    };

    template<> struct HashParamsFnv1<4> {
        static const uint32_t Prime = 0x01000193;
        static const uint32_t Base  = 0x811C9DC5;
    };

    template<size_t NBytes> size_t
    hash_fnv1a (const uint8_t* bytes, size_t hash_base = HashParamsFnv1<sizeof(size_t)>::Base);

    template<size_t N> constexpr size_t
    hash_fnv1a (const char(&str)[N], size_t hash_base = HashParamsFnv1<sizeof(size_t)>::Base);

    inline size_t
    hash_fnv1a (const uint8_t* bytes, size_t nbytes, size_t hash_base = HashParamsFnv1<sizeof(size_t)>::Base);

    template<typename T> struct HashFnv1a;

} }

#include "Hash.hpp"

#endif