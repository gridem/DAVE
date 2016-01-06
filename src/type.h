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

using Type = std::type_index;

template<typename T>
Type getType()
{
    return {typeid(T)};
}

template<typename T>
std::string getTypeName()
{
    std::string n = typeid(T).name();
    if (n.substr(0, 7) == "struct ")
        return n.substr(7);
    return n;
}

struct TypeContainer
{
    using ObjectHolder = std::unique_ptr<void, void(*)(void*)>;
    
    template<typename T>
    T& add()
    {
        T* ptr = new T;
        auto obj = ObjectHolder(ptr, [](void* p) { delete (T*)p; });
        objects.emplace(getType<T>(), std::move(obj));
        return *ptr;
    }
    
    template<typename T>
    T& get() const
    {
        auto res = objects.find(getType<T>());
        VERIFY(res != objects.end(), "Type is absent: " + getTypeName<T>());
        return *(T*)res->second.get();
    }
    
    template<typename T>
    bool has() const
    {
        return objects.find(getType<T>()) != objects.end();
    }
    
private:
    std::unordered_map<Type, ObjectHolder> objects;
};

