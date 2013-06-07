#ifndef SRL_CONTEXT_HPP
#define SRL_CONTEXT_HPP

#include "Context.h"
#include "Node.h"
#include "String.h"

namespace Srl {

    template<class T>
    Context<Paste>& Context<Paste>::operator () (T& o)
    {
        return this->operator()(o, String());
    }

    template<class T>
    Context<Insert>& Context<Insert>::operator () (const T& o)
    {
        return this->operator()(o, String());
    }

    template<class T>
    Context<Insert>& Context<Insert>::operator () (const T& o, const String& name)
    {
        this->node->insert(name, o);
        return *this;
    }

    template<class T>
    Context<Paste>& Context<Paste>::operator () (T& o, const String& name)
    {
        auto type   = Lib::Switch<T>::type;
        auto& index = TpTools::is_scope(type) ? this->node_index : this->value_index;

        if(name.size() > 0) {
            this->node->paste_field(name, o);
        } else {
            this->node->paste_field(index, o);
        }
        index += 1;

        return *this;
    }
}

#endif
