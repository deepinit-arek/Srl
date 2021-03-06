#ifndef SRL_HASH_H
#define SRL_HASH_H

#include "Common.h"
#include "Heap.h"
#include "Aux.h"

#include <cmath>

namespace Srl { namespace Lib {

    namespace Aux {

        uint64_t hash_fnc(const uint8_t* bytes, size_t nbytes);

        inline size_t round_pow2(size_t sz)
        {
            return std::pow(2, std::ceil(std::log2(sz)));
        }
    }

    template<class T> struct HashSrl {
        uint64_t operator() (const T& t) const { return Aux::hash_fnc(t.data(), t.size()); }
    };

    template <class Key, class Val, class HashFnc = HashSrl<Key>>
    class HTable {

    public:
        HTable(size_t buckets = 64, double load_factor_ = 1.0)
            : load_factor(fmax(load_factor_, 0.1)), cap(Aux::round_pow2(buckets)) { }

        ~HTable() { destroy<Val>(); }

        HTable(const HTable& m) = default;

        HTable(HTable&& m) { *this = std::forward<HTable>(m); }

        HTable& operator= (HTable&& m);

        Val* get (const Key& key);
        /* fst -> exists? snd -> entry */
        template<class... Args>
        std::pair<bool, Val*> insert (const Key& key, Args&&... args);

        template<class... Args>
        std::pair<bool, Val*> insert_hash (uint64_t hash, Args&&... args);

        void foreach_entry(const std::function<void(uint64_t, Val&)>& fnc);

        void foreach_entry_cont(const std::function<bool(uint64_t, Val&)>& fnc);

        void remove(const Key& key);
        void remove_hash(uint64_t  hash);

        size_t num_entries() const { return this->elements; }

        void clear();

    private:
        struct Entry {

            template<class... Args> Entry (uint64_t hash_, Args&&... args)
                : hash(hash_), val(std::forward<Args>(args)...) { }

            uint64_t hash;
            Entry*   next = nullptr;
            Val      val;
        };

        double load_factor;
        size_t cap;

        size_t limit    = 0;
        size_t elements = 0;

        Entry** table   = nullptr;
        HashFnc hash_fnc;
        Heap    heap;

        void   redistribute();
        size_t get_bucket(uint64_t hash);

        Entry** alloc_table();

        Entry* get_rm (uint64_t hash);

        template<class T>
        typename std::enable_if<std::is_trivially_destructible<T>::value, void>::type
        destroy(Val&) { }

        template<class T>
        typename std::enable_if<!std::is_trivially_destructible<T>::value, void>::type
        destroy(Val&);

        template<class T>
        typename std::enable_if<std::is_trivially_destructible<T>::value, void>::type
        destroy() { }

        template<class T>
        typename std::enable_if<!std::is_trivially_destructible<T>::value, void>::type
        destroy();
    };


} }

#endif
