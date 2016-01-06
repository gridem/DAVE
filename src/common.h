/*
 * Copyright 2013-2016 Grigory Demchenko (aka gridem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

