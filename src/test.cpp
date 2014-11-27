#include "verifier.h"

struct Broadcast {};
struct BroadcastAck {};

struct IConnection : HandlerQueue
{
};

template<typename T_service>
void createService(int node)
{
    An<Nodes>()->sizedNode(node).addProcess<T_service>();
}

template<typename T_msg>
struct Delegate
{
    void on(const T_msg& msg)
    {
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
    template<typename T_service, typename T_msg>
    void trigger(int dstNode, const T_msg& msg)
    {
        emulator->trigger<T_service>(dstNode, msg);
    }
    
    template<typename T_service, typename T_msg>
    void trigger(const T_msg& msg)
    {
        emulator->trigger<T_service>(context().currentNode, msg);
    }
    
    template<typename T_service, typename T_msg>
    void triggerAll(const T_msg& msg)
    {
        for (size_t i = 0; i < nodes->size(); ++ i)
            if (nodes->node(i).hasProcess<T_service>())
                emulator->trigger<T_service>(i, msg);
    }
    
    template<typename... T>
    void triggerSelf(T&&... t)
    {
        trigger<T_service>(std::forward<T>(t)...);
    }
    
    template<typename T_service>
    void attachTo()
    {
        this->attach(localService<T_service>());
    }
    
    template<typename T_msg, typename T_service>
    void attachToMessage()
    {
        static_cast<Delegate<T_msg>&>(*this).attach(localService<T_service>());
    }
    
    template<typename T_serviceDelegate>
    void bindTo()
    {
        localService<T_serviceDelegate>().attach(static_cast<T_service&>(*this));
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
    template<typename T_service>
    T_service& examineService(int node)
    {
        return nodes->node(node).getProcess<T_service>().getService();
    }
    
    template<typename T_service>
    T_service& localService()
    {
        return examineService<T_service>(context().currentNode);
    }
    
    An<Emulator> emulator;
    An<Nodes> nodes;
};

#define SLOG(D_msg)     JLOG("SRV: " << D_msg)

#define TLOG(D_msg)         JLOG("--- TEST: " << D_msg)

#define CHECK(D_cond, D_msg) \
    if (!(D_cond)) RLOG("Verification failed: " << #D_cond << ", " << D_msg); \
    else CLOG("Verification success: " << #D_cond)

#define CHECK2(D_cond, D_msg) \
    if (!(D_cond)) RLOG("Verification failed: " << #D_cond << ", " << D_msg)

static const int servers = 3;

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

struct Ack {};

template<typename T_val>
struct Write
{
    T_val val;
};

struct WriteReturn {};

template<typename T_msg>
struct BestEffortBroadcast : Service<BestEffortBroadcast<T_msg>, MsgSrc<T_msg>>
{
    using Deliver = MsgSrc<T_msg>;
    using Service<BestEffortBroadcast, Deliver>::on;
    
    void on(const T_msg& m)
    {
        this->triggerAll<BestEffortBroadcast>(Deliver{m, context().sourceNode});
    }
};

template<typename T_val>
struct RegularRegister1N : Service<RegularRegister1N<T_val>, WriteReturn>
{
    using BroadcastSrv = BestEffortBroadcast<T_val>;
    
    using Write = Write<T_val>;
    using Deliver = MsgSrc<T_val>;
    using Service<RegularRegister1N, WriteReturn>::on;

    T_val val {};
    int num = 0;
    
    void on(const Init&)
    {
        bindTo<BroadcastSrv>();
    }
    
    void on(const Write& w)
    {
        trigger<BroadcastSrv>(w.val);
    }
    
    void on(const Deliver& v)
    {
        val = v.msg;
        triggerSelf(v.srcNode, Ack{});
    }
    
    void on(const Ack&)
    {
        incNum();
    }
    
    void on(const Disconnect&)
    {
        incNum();
    }
    
    void incNum()
    {
        if (++ num == servers)
        {
            num = 0;
            triggerSelf(WriteReturn());
        }
    }
};

struct Client : Service<Client>
{
    using Service<Client>::on;
    struct Written {};
    
    void on(const Init&)
    {
        if (context().currentNode == 0)
        {
            Write<int> w{1};
            trigger<RegularRegister1N<int>>(1, w);
        }
        else
        {
            bindTo<RegularRegister1N<int>>();
        }
    }

    void on(const WriteReturn&)
    {
        triggerSelf(0, Written{});
    }
    
    void on(const Written&)
    {
        written = true;
    }
    
    void on(const Disconnect&)
    {
        lastDisconnected = context().sourceNode;
    }
    
    bool written = false;
    int lastDisconnected = -1;
};

struct ServiceCreator
{
    template<typename T_service>
    void create(int node, int nodeCount = 1)
    {
        for (int i = 0; i < nodeCount; ++ i)
            nodes->sizedNode(node + i).addProcess<T_service>();
    }
    
private:
    An<Nodes> nodes;
};

void test()
{
    TLOG("Testing RegularRegister1N");
    ServiceCreator c;
    c.create<Client>(0);
    c.create<RegularRegister1N<int>>(0, servers);
    c.create<BestEffortBroadcast<int>>(0, servers);
    
    ServiceAccessor a;
    TrueScheduler s {[&a] {
        TLOG("on end");
        CHECK2(a.service<Client>(0).written, "not written");
    }};
    s.run();
}

void test2()
{
    TLOG("Testing RegularRegister1N: sender may fail");
    ServiceCreator c;
    c.create<Client>(0, 2);
    c.create<RegularRegister1N<int>>(1, servers);
    c.create<BestEffortBroadcast<int>>(1, servers);
    
    ServiceAccessor a;
    TrueScheduler s {[&a] {
        TLOG("on end");
        Client& client = a.service<Client>(0);
        CHECK2(client.written || client.lastDisconnected == 1, "not written");
    }};
    s.run();
}
