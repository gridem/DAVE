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

template<typename T, typename T_impl>
struct Adapter;

template<typename T, typename T_impl, typename T_impl2>
struct Adapter<Adapter<T, T_impl>, T_impl2> : Adapter<T, T_impl2> {};

template<typename T_adapter>
struct Adapter<Service, T_adapter> : T_adapter
{
    typedef typename T_adapter::Impl Impl;
    
    template<typename T_msg>
    void on(const T_msg& msg)
    {
        this->call(&Impl::on, msg);
    }
};

