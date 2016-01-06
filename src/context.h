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

struct Context
{
    Context(int src = -1, int cur = -1) : sourceNode(src), currentNode(cur) {}
    
    int sourceNode;
    int currentNode;
};

inline std::ostream& operator<<(std::ostream& o, const Context& ctx)
{
    return o << std::to_string(ctx.sourceNode) << " -> " << std::to_string(ctx.currentNode);
}

inline Context& context()
{
    return single<Context>();
}

inline Context destinationContext(int dst)
{
    return {context().currentNode, dst};
}

struct ContextGuard
{
    ContextGuard(const Context& ctx) : old(context())
    {
        context() = ctx;
    }
    
    ~ContextGuard()
    {
        context() = old;
    }
    
private:
    Context old;
};
