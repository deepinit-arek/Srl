#ifndef SRL_SUBSCRIPTION_H
#define SRL_SUBSCRIPTION_H

#include "String.h"
#include "Hash.h"
#include "Exception.h"
#include "Resolve.h"
#include <map>
#include <functional>

namespace Srl {

    struct TypeID { virtual const char* name() const = 0; };

    namespace Lib {

        class Registrations {

        public:
            template<class T>
            void add(const String& id)
            {
                auto hash = Tools::hash_fnv1a(id.data(), id.size());
                auto itr  = this->lookup.insert({ hash, [](){ return Ctor<T>::Create_New(); } });

                if(!itr.second) {
                    throw Exception("Class id " + id.unwrap(false) + " duplicated.");
                }
            }

            template<class T>
            T* create(const String& id)
            {
                auto hash = Tools::hash_fnv1a(id.data(), id.size());
                auto itr = this->lookup.find(hash);

                if(itr == this->lookup.end()) {
                    throw Exception("Class id " + id.unwrap(false) + " not registered.");
                }

                return static_cast<T*>(itr->second());
            }

        private:
            std::map<size_t, std::function<void*(void)>> lookup;
        };

        Registrations* registrations();

        template<class T>
        struct TypeRegistration : TypeID {

            const char* type_id;

            TypeRegistration(const char* type_id_) : type_id(type_id_)
            {
                Lib::registrations()->add<T>(type_id);
            }

            const char* name() const override { return this->type_id; }
        };
    }

    template<class T>
    Lib::TypeRegistration<T> register_type(const char* type_id)
    {
        return Lib::TypeRegistration<T>(type_id);
    }
}

#endif