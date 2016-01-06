// Copyright (c) 2013 Grigory Demchenko

#pragma once

#include <string>
#include <functional>
#include <boost/intrusive/list.hpp>

namespace bi = boost::intrusive;

using Buffer = std::string;
using Handler = std::function<void()>;

using ListHook = bi::list_member_hook<bi::link_mode<bi::auto_unlink>>;

inline auto disposer()
{
    return [](auto* obj) { delete obj; };
}

template<typename T_type, ListHook T_type::* T_member>
struct List : bi::list
    <
        T_type,
        bi::constant_time_size<false>,
        bi::member_hook<T_type, ListHook, T_member>
    >
{
    using BaseList = bi::list
        <
            T_type,
            bi::constant_time_size<false>,
            bi::member_hook<T_type, ListHook, T_member>
        >;

    using BaseList::BaseList;

    void clearDispose()
    {
        this->clear_and_dispose(disposer());
    }

    T_type& popFront()
    {
        auto& v = this->front();
        this->pop_front();
        return v;
    }
};

struct IObject
{
    virtual ~IObject() {}
};

template<typename T, typename T_tag = T>
T& single()
{
    static T t;
    return t;
}

template<typename T>
void reCtor(T& t)
{
    T newT;
    t = std::move(newT);
    /* not exception-safe
    t.~T();
    new (&t)T();
    */
}

template<typename T_derived>
struct Initer
{
    void init()                 { reCtor(static_cast<T_derived&>(*this)); }
};

template<typename T>
struct Ptr
{
    T& operator*()              { return *ptr; }
    const T& operator*() const  { return *ptr; }
    T* operator->()             { return ptr; }
    const T* operator->() const { return ptr; }
private:
    T* ptr = nullptr;
};

template<typename T>
struct An
{
    T& operator*()              { return single<T>(); }
    const T& operator*() const  { return single<T>(); }
    T* operator->()             { return &single<T>(); }
    const T* operator->() const { return &single<T>(); }
};

