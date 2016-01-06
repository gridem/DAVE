#include "verifier.h"

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
        this->template triggerAll<BestEffortBroadcast>(Deliver{m, context().sourceNode});
    }
};

template<typename T_val>
struct RegularRegister1N : Service<RegularRegister1N<T_val>, WriteReturn>
{
    using BroadcastSrv = BestEffortBroadcast<T_val>;
    
    using WriteVal = Write<T_val>;
    using Deliver = MsgSrc<T_val>;
    using Service<RegularRegister1N, WriteReturn>::on;

    T_val val {};
    int num = 0;
    
    void on(const Init&)
    {
        this->template bindTo<BroadcastSrv>();
    }
    
    void on(const WriteVal& w)
    {
        this->template trigger<BroadcastSrv>(w.val);
    }
    
    void on(const Deliver& v)
    {
        val = v.msg;
        this->triggerSelf(v.srcNode, Ack{});
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
        if (++ num == config->nodes)
        {
            num = 0;
            this->triggerSelf(WriteReturn());
        }
    }

    An<Config> config;
};

struct Client : Service<Client>
{
    using WriteVal = Write<int>;
    using Service<Client>::on;
    using Register = RegularRegister1N<int>;

    struct Written {};
    
    void on(const Init&)
    {
        if (context().currentNode == 0)
        {
            //this->bindTo<Register>();
            WriteVal w{1};
            this->triggerAny<Register>(w);
        }
        else
        {
            this->bindTo<Register>();
        }
    }

    void on(const WriteReturn&)
    {
        this->triggerSelf(0, Written{});
    }
    
    void on(const Written&)
    {
        written = true;
    }
    
    void on(const Disconnect&)
    {
        if (context().currentNode != 0)
            return;
        if (written)
            return;
        if (context().sourceNode != 1)
            return;
        WriteVal w{1};
        CHECK2(this->triggerAny<Register>(w) == 1, "trigger ok");
    }
    
    bool written = false;
};

void test()
{
    TLOG("Testing RegularRegister1N");
    ServiceCreator c;
    c.create<Client>(0);
    c.create<RegularRegister1N<int>>(0, c.config().nodes);
    c.create<BestEffortBroadcast<int>>(0, c.config().nodes);
    
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
    c.create<Client>(0, c.config().nodes + 1);
    c.create<RegularRegister1N<int>>(1, c.config().nodes);
    c.create<BestEffortBroadcast<int>>(1, c.config().nodes);
    
    ServiceAccessor a;
    TrueScheduler s {[&a] {
        TLOG("on end");
        Client& client = a.service<Client>(0);
        CHECK2(client.written, "not written");
    }};
    s.run();
}
