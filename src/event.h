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

#define JLOG LOG
#define HLOG(D_msg)     JLOG("HNDL: " << D_msg)

enum class EventType
{
    Trigger,
    Disconnect
};

inline std::string toString(EventType type)
{
    switch (type)
    {
    case EventType::Trigger:
        return "trigger";
    
    case EventType::Disconnect:
        return "disconnect";
    
    default:
        RAISE("Unknown event type");
    }
}

inline std::ostream& operator<<(std::ostream& o, EventType type)
{
    return o << toString(type);
}

struct NodeHandler;

inline void attach(NodeHandler& h);

struct NodeHandler : Handler
{
    using Handler::Handler;
    
    // FIX: workaround GCC 4.9
    NodeHandler(Handler&& h) : Handler(std::move(h)) {}

    std::string name;
    EventType type;
    
    // container section
    ListHook queue; // list of scheduled handlers
    ListHook all;   // list of all handlers
    
    void dump()
    {
        HLOG("invoking: " << name << " [" << type << "]");
    }

    void rdump()
    {
        RLOG("invoking: " << name << " [" << type << "]");
    }

    void invoke()
    {
        dump();
        (*this)();
        delete this;
    }
    
    static NodeHandler& create(Handler handler, std::string name, EventType type)
    {
        HLOG("creating: " << name);
        auto* h = new NodeHandler(std::move(handler));
        attach(*h);
        h->name = std::move(name);
        h->type = type;
        return *h;
    }
};

using Handlers = List<NodeHandler, &NodeHandler::all>;

inline void attach(NodeHandler& h)
{
    An<Handlers>()->push_back(h);
}
