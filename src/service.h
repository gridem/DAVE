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

#include "verifier.h"

struct IConnection : HandlerQueue
{
};

template<typename T_msg>
struct Delegate
{
    void on(const T_msg& msg)
    {
        VERIFY(delegate != nullptr, "Delegate " + getTypeName<T_msg>() + " must be init");
        delegate(msg);
    }
    
    template<typename T_service>
    void attach(T_service& service)
    {
        delegate = [&service](const T_msg& msg) { service.on(msg); };
    }

private:
    std::function<void(const T_msg& msg)> delegate;
};

template<typename T_service, typename... T_delegateMessage>
struct Service : Delegate<T_delegateMessage>...
{
    template<typename T_dstService, typename T_msg>
    bool trigger(int dstNode, const T_msg& msg)
    {
        return emulator->trigger<T_dstService>(dstNode, msg);
    }
    
    template<typename T_dstService, typename T_msg>
    bool trigger(const T_msg& msg)
    {
        return emulator->trigger<T_dstService>(context().currentNode, msg);
    }
    
    template<typename T_dstService, typename T_msg>
    int triggerAny(const T_msg& msg, int count = 1)
    {
        int triggerCount = 0;
        for (size_t i = 0; i < nodes->size(); ++ i)
            if (this->trigger<T_dstService>(i, msg))
                if (++ triggerCount == count)
                    break;
        return triggerCount;
    }

    template<typename T_dstService, typename T_msg>
    int triggerAllExceptSelf(const T_msg& msg)
    {
        int triggerCount = 0;
        for (size_t i = 0; i < nodes->size(); ++ i)
            if (i != context().currentNode)
                if (this->trigger<T_dstService>(i, msg))
                    ++ triggerCount;
        return triggerCount;
    }

    template<typename T_dstService, typename T_msg>
    int triggerAll(const T_msg& msg)
    {
        return this->triggerAny<T_dstService>(msg, -1);
    }

    template<typename... T>
    void triggerSelf(T&&... t)
    {
        trigger<T_service>(std::forward<T>(t)...);
    }
    
    template<typename T_localService>
    void attachTo()
    {
        this->attach(localService<T_localService>());
    }
    
    template<typename T_msg, typename T_localService>
    void attachToMessage()
    {
        static_cast<Delegate<T_msg>&>(*this).attach(localService<T_localService>());
    }
    
    template<typename T_serviceDelegate>
    void bindTo()
    {
        this->localService<T_serviceDelegate>().attach(static_cast<T_service&>(*this));
    }
    
    void on(const Init&)
    {
    }
    
    void on(const Disconnect&)
    {
    }

    template<typename T>
    void on(const T& t)
    {
        Delegate<T>::on(t);
    }
    
private:
    template<typename T_dstService>
    T_dstService& examineService(int node)
    {
        return nodes->node(node).getProcess<T_dstService>().getService();
    }
    
    template<typename T_localService>
    T_localService& localService()
    {
        return this->examineService<T_localService>(context().currentNode);
    }
    
    An<Emulator> emulator;
    An<Nodes> nodes;
};

struct ServiceCreator
{
    template<typename T_service>
    void create(int node, int nodeCount = 1)
    {
        for (int i = 0; i < nodeCount; ++ i)
            nodes->sizedNode(node + i).addProcess<T_service>();
    }

    Config& config()
    {
        return *conf;
    }

private:
    An<Nodes> nodes;
    An<Config> conf;
};

#define SLOG(D_msg)         JLOG("SRV: " << D_msg)

#define TLOG(D_msg)         JLOG("--- TEST: " << D_msg)

#define CHECK(D_cond, D_msg) \
    if (!(D_cond)) RLOG("Verification failed: " << #D_cond << ", " << D_msg); \
    else CLOG("Verification success: " << #D_cond)

#define CHECK2(D_cond, D_msg) \
    if (!(D_cond)) RLOG("Verification failed: " << #D_cond << ", " << D_msg)

#define CHECK3(D_cond, D_msg) \
    if (!(D_cond)) { \
        RLOG("Verification failed: " << #D_cond << ", " << D_msg); \
        throw VerificationFail{}; \
    }

struct Void {};

template<typename T, typename T_tag = Void>
struct Send
{
    T t;
};

// message with source node
template<typename T = Void>
struct MsgSrc
{
    T msg;
    int srcNode;
};
