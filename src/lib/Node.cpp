#include "Srl/Srl.h"
#include "Srl/Internal.h"

using namespace std;
using namespace Srl;
using namespace Lib;

namespace {

    template<class T> const string& tp_name();
    template<> const string& tp_name<Node>()  { static const string str = "Node";  return str; }
    template<> const string& tp_name<Value>() { static const string str = "Value"; return str; }

    typedef uint8_t Option;

    const Option None            = 0;
    const Option Hash            = 1 << 0; /* compare by name hash */
    const Option Address         = 1 << 1; /* compare by address */
    const Option Index           = 1 << 2; /* access  by Index */
    const Option Remove          = 1 << 3; /* remove found link */
    const Option Throw           = 1 << 4; /* throw exception on not found or multiple findings */
    const Option Check_Duplicate = 1 << 5; /* check for multiple results */

    constexpr bool enabled(Option options, Option option)
    {
        return (options & option) > 0;
    }

    template<class T>
    using Cont = Lib::Items<T>;

    template<class T>
    using Itr = typename Cont<T>::iterator;

    template<Option Opt, class T> typename enable_if<enabled(Opt, Hash), bool>::type
    compare(Itr<T>& itr, size_t hash) { return itr->hash == hash; }

    template<Option Opt, class T> typename enable_if<enabled(Opt, Address), bool>::type
    compare(Itr<T>& itr, T* pointer) { return &itr->field == pointer; }

    template<Option Opt, class TKey, class T>
    typename enable_if<enabled(Opt, Index), Link<T>*>::type
    find_link(TKey index, Cont<T>& links)
    {
        if(index < links.size()) {
            auto itr = links.begin();
            advance(itr, index);
            auto* rslt = &*itr;
            if(enabled(Opt, Remove)) {
                links.erase(itr);
            }
            return rslt;

        } else {
            if(enabled(Opt, Throw)) {
                auto msg = "Cannot access " + tp_name<T>() + " at index " + to_string(index) + ". Index out of bounds.";
                throw Exception(msg);
            }
            return nullptr;
        }
    }

    template<Option Opt, class TKey, class T>
    typename enable_if<!enabled(Opt, Index), Link<T>*>::type
    find_link(TKey key, Cont<T>& links, const String& name = String())
    {
        Itr<T> end    = links.end();
        Link<T>* rslt = nullptr;

        for(auto itr = links.begin(); itr != end; itr++) {
            if(compare<Opt, T>(itr, key)) {

                if(enabled(Opt, Throw) && rslt) {
                    auto msg = tp_name<T>() + " duplication: " + name.unwrap(false) + ".";
                    throw Exception(msg);
                }

                rslt = &*itr;

                if(enabled(Opt, Remove)) {
                    links.erase(itr);
                    break;
                }

                if(!enabled(Opt, Check_Duplicate)) {
                    break;
                }
            }
        }

        if(enabled(Opt, Throw) && !rslt) {
            auto msg = tp_name<T>() + " " + name.unwrap(false) + " not found.";
            throw Exception(msg);
        }

        return rslt;
    }

    size_t hash_string(const String& str, Environment& env)
    {
        return str.encoding() == Environment::Str_Encoding 
            ? Aux::hash_fnc(str.data(), str.size())
            : env.hash_string(str);
    }

    template<class T>
    T* find_link_remove(const String& name, Cont<T>& links, Environment& env)
    {
        auto* link = links.size() > 0 
            ? find_link<Hash | Remove>(hash_string(name, env), links)
            : nullptr;

        return link ? &link->field : nullptr;
    }

    bool compare(const String& a, const MemBlock& b)
    {
        return a.size() == b.size && memcmp(a.data(), b.ptr, b.size) == 0;
    }
}

void Node::insert_value(const Value& new_value, const String& name_)
{
    if(this->env->parsing) {
        this->env->tree->write(new_value, name_);

    } else {
        this->env->store_value(*this, new_value, name_);
    }
}

Node& Node::insert_node(const Node& new_node, const String& name_)
{
    return this->env->store_node(*this, new_node, name_)->field;
}

Node& Node::insert_node(Type node_type, const String& name_)
{
    return this->insert_node(Node(this->env->tree, node_type), name_);
}

Value& Node::value(const String& name_)
{
    auto hash  = hash_string(name_, *this->env);
    auto* link = find_link<Throw | Hash>(hash, this->values, name_);

    return link->field;
}

Value& Node::value(size_t index)
{
    auto* link = find_link<Throw | Index>(index, this->values);
    return link->field;
}

Node& Node::node(size_t index)
{
    return find_link<Throw | Index>(index, this->nodes)->field;
}

Node& Node::node(const String& name_)
{
    auto hash = hash_string(name_, *this->env);
    return find_link<Throw | Hash>(hash, this->nodes, name_)->field;
}

void Node::read_source(In& source, Parser& parser)
{
    while(true) {

        MemBlock seg_name; Value val;
        tie(seg_name, val) = parser.read(source);

        if(val.type() == Type::Scope_End) {
            this->parsed = true;
            break;
        }

        auto field_name = String(seg_name, Encoding::UTF8);

        if(!TpTools::is_scope(val.type())) {
            /* new value */
            this->env->store_value(*this, val, field_name);

        } else {
            /* new node */
            auto* link = this->env->store_node(*this, Node(this->env->tree, val.type()), field_name);

            link->field.read_source(source, parser);
        }
    }
}

Node Node::consume_node(const String& id)
{
    auto* stored = find_link_remove(id, this->nodes, *this->env);

    if(stored) {
        return *stored;
    }

    auto id_conv = this->env->conv_string(id);

    while(!this->parsed) {

        MemBlock seg_name; Value val;
        tie(seg_name, val) = this->env->parser->read(*this->env->in);

        auto tp = val.type();

        if(tp == Type::Scope_End) {
            this->parsed = true;
            break;
        }

        if(TpTools::is_scope(tp)) {
            if(compare(id_conv, seg_name)) {
                return Node(this->env->tree, tp, false);
            }

            auto* link = this->env->store_node(*this, Node(this->env->tree, tp), seg_name);
            link->field.read_source(*this->env->in, *this->env->parser);

        } else {
            this->env->store_value(*this, val, seg_name);
        }
    }

    throw Exception("Field " + id.unwrap(false) + " not found");
}

Value Node::consume_value(const String& id)
{
    auto* stored = find_link_remove(id, this->values, *this->env);

    if(stored) {
        return *stored;
    }

    auto name_conv = this->env->conv_string(id);

    while(!this->parsed) {

        MemBlock seg_name; Value val;
        tie(seg_name, val) = this->env->parser->read(*this->env->in);

        auto tp = val.type();

        if(tp == Type::Scope_End) {
            this->parsed = true;
            break;
        }

        if(!TpTools::is_scope(tp)) {
            if(compare(name_conv, seg_name)) { 
                return val;
            }

            this->env->store_value(*this, val, seg_name);

        } else {
            auto* link = this->env->store_node(*this, Node(this->env->tree, tp), seg_name);
            link->field.read_source(*this->env->in, *this->env->parser);
        }
    }

    throw Exception("Field " + id.unwrap(false) + " not found");
}


Node Node::consume_node(bool throw_exception)
{
    if(this->nodes.size() > 0) {
        auto itr = this->nodes.begin();
        auto& field = itr->field;
        this->nodes.erase(itr);

        return move(field);
    }

    while(!this->parsed) {
        MemBlock seg_name; Value val;
        tie(seg_name, val) = this->env->parser->read(*this->env->in);

        auto tp = val.type();

        if(tp == Type::Scope_End) {
            this->parsed = true;
            break;
        }

        if(TpTools::is_scope(tp)) {
            return Node(this->env->tree, tp, false);

        } else {
            this->env->store_value(*this, val, seg_name);
        }
    }

    if(throw_exception) {
        throw Exception("Cannot access Node. Index out of bounds");
    }

    return Node(*this->env->tree);
}

Value Node::consume_value(bool throw_exception)
{
    if(this->values.size() > 0) {
        auto itr = this->values.begin();
        auto& field = itr->field;
        this->values.erase(itr);

        return field;
    }

    while(!this->parsed) {
        MemBlock seg_name; Value val;
        tie(seg_name, val) = this->env->parser->read(*this->env->in);

        auto tp = val.type();

        if(tp == Type::Scope_End) {
            this->parsed = true;
            break;
        }

        if(!TpTools::is_scope(tp)) {
            return val;

        } else {
            auto* link = this->env->store_node(*this, Node(this->env->tree, tp), seg_name);
            link->field.read_source(*this->env->in, *this->env->parser);
        }
    }

    if(throw_exception) {
        throw Exception("Cannot access Value. Index out of bounds");
    }

    return Value(Type::Null);
}

void Node::to_source()
{
    Value scope_start = Value(this->scope_type);
    this->env->tree->write(scope_start, *this->name_ptr);

    for(auto& v : this->values) {
        this->env->tree->write(v.field, v.field.name());
    }

    for(auto& n : this->nodes) {
        n.field.to_source();
    }

    Value scope_end = Value(Type::Scope_End);
    this->env->tree->write(scope_end, *this->name_ptr);
}

void Node::consume_scope()
{
    int depth = 0;

    auto& parser = *this->env->parser;
    auto& source = *this->env->in;

    while(!this->parsed) {
        auto tp = parser.read(source).second.type();
        if(TpTools::is_scope(tp)) {
            depth++;
            continue;
        }

        if(tp == Type::Scope_End) {
            depth--;
            if(depth <= 0) {
                this->parsed = true;
            }
        }
    }
}

pair<bool, size_t> Node::insert_shared (const void* obj)
{
    bool exists; size_t* key;
    auto& table = this->env->shared_table_store;

    tie(exists, key) = table.insert(obj, table.num_entries());

    return { exists, *key };
}

pair<bool, shared_ptr<void>*> Node::find_shared (size_t key, const function<shared_ptr<void>(void)>& create)
{
    auto& table = this->env->shared_table_restore;
    auto* sptr = table.get(key);
    bool inserted_new = false;

    if(!sptr) {
        sptr = table.insert(key, create()).second;
        inserted_new = true;
    }

    return { inserted_new, sptr };
}

void Node::foreach_node(const function<void(Node&)>& fnc, bool recursive)
{
    for(auto& link : this->nodes) {

        fnc(link.field);

        if(recursive) {
            link.field.foreach_node(fnc, recursive);
        }
    }
}

void Node::foreach_value(const function<void(Value&)>& fnc, bool recursive)
{
    for(auto& link : this->values) {
        fnc(link.field);
    }

    if(recursive) {
        this->foreach_node([&fnc](Node& node) {
            node.foreach_value(fnc);
        }, true);
    }
}

list<Node*> Node::find_nodes(const String& name_, bool recursive)
{
    list<Node*> rslt;
    auto hash = hash_string(name_, *this->env);

    this->foreach_node([&rslt, this, hash] (Node& node) {
        if(hash_string(*node.name_ptr, *this->env) == hash) {
            rslt.push_back(&node);
        }
    }, recursive);

    return move(rslt);
}

list<Value*> Node::find_values(const String& name_, bool recursive)
{
    list<Value*> rslt;
    auto hash = hash_string(name_, *this->env);

    this->foreach_value([&rslt, this, hash] (Value& value) {
        if(hash_string(value.name(), *this->env) == hash) {
            rslt.push_back(&value);
        }
    }, recursive);

    return move(rslt);
}

list<Node*> Node::all_nodes(bool recursive)
{
    list<Node*> rslt;

    this->foreach_node([&rslt] (Node& node) {
        rslt.push_back(&node);
    }, recursive);

    return move(rslt);
}

list<Value*> Node::all_values(bool recursive)
{
    list<Value*> rslt;

    this->foreach_value([&rslt] (Value& value) {
        rslt.push_back(&value);
    }, recursive);

    return move(rslt);
}

void Node::remove_node(const String& name_)
{
    auto hash = hash_string(name_, *this->env);
    find_link<Remove | Hash>(hash, this->nodes, name_);
}

void Node::remove_node(size_t index)
{
    find_link<Remove | Index>(index, this->nodes);
}

void Node::remove_node(Node* to_remove)
{
    find_link<Remove | Address>(to_remove, this->nodes);
}

void Node::remove_value(const String& name_)
{
    auto hash = hash_string(name_, *this->env);
    find_link<Remove | Hash>(hash, this->values, name_);
}

void Node::remove_value(size_t index)
{
    find_link<Remove | Index>(index, this->values);
}

void Node::remove_value(Value* to_remove)
{
    find_link<Remove | Address>(to_remove, this->values);
}

bool Node::has_node(const String& field_name)
{
    auto hash = hash_string(field_name, *this->env);
    return find_link<Hash>(hash, this->nodes, field_name) != nullptr;
}

bool Node::has_value(const String& field_name)
{
    auto hash = hash_string(field_name, *this->env);
    return find_link<Hash>(hash, this->values, field_name) != nullptr;
}
