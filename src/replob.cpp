#include "verifier.h"

#include <set>

template<typename T>
std::set<T> operator-(const std::set<T>& s1, const std::set<T>& s2)
{
    std::set<T> res;
    for (auto&& t: s1)
        if (s2.count(t) == 0)
            res.insert(t);
    return res;
}

template<typename T>
void operator|=(std::set<T>& s, const T& t)
{
    s.insert(t);
}

template<typename T>
void operator|=(std::set<T>& s, const std::set<T>& t)
{
    s.insert(t.begin(), t.end());
}

template<typename T>
void operator&=(std::set<T>& s, const std::set<T>& t)
{
    std::set<T> newSet;
    for (auto&& e: s)
        if (t.count(e) == 1)
            newSet |= e;
    s = std::move(newSet);
}

template<typename T>
std::set<T> operator|(const std::set<T>& s1, const std::set<T>& s2)
{
    std::set<T> res = s1;
    res |= s2;
    return res;
}

struct MsgId
{
    int id = nextId();

    static int nextId()
    {
        static int i = 0;
        return ++ i;
    }

    bool operator<(const MsgId& m) const
    {
        return id < m.id;
    }
};

using NodeId = int;

using CarrySet = std::set<MsgId>;
using NodesSet = std::set<NodeId>;

struct Apply
{
    MsgId id;
};

struct Vote
{
    CarrySet carrySet;
    NodeId srcNode;
    NodesSet nodesSet;
};

struct Commit
{
    CarrySet carrySet;
};

struct Replob : Service<Replob>
{
    using Service<Replob>::on;

    enum struct State
    {
        Initial, // can vote only in initial state
        Voted,
        Completed,
    };

    void on(const Apply& apply)
    {
        SLOG("Apply");
        NodesSet nodesSet;
        for (NodeId nId = 0; nId < config->nodes; ++ nId)
            nodesSet |= nId;
        on(Vote{CarrySet{apply.id}, context().currentNode, nodesSet});
    }

    void on(const Vote& vote)
    {
        if (state_ == State::Completed)
            return;
        if (!nodes_.empty() && nodes_.count(context().sourceNode) == 0)
            return;
        carries_ |= vote.carrySet;
        if (nodes_.empty())
        {
            nodes_ = vote.nodesSet;
        }
        else if (nodes_ != vote.nodesSet)
        {
            state_ = State::Initial;
            nodes_ &= vote.nodesSet;
            voted_.clear(); // must be below nodes_ because nodesSet can be reference to voted_!!!
        }
        voted_ |= vote.srcNode;
        voted_ |= context().currentNode;
        if (voted_  == nodes_)
        {
            on(Commit{carries_});
        }
        else if (state_ == State::Initial)
        {
            state_ = State::Voted;
            // TODO: use broadcastSet().<method>(nodes_set, ...)
            this->triggerAllExceptSelf<Replob>(Vote{carries_, context().currentNode, nodes_});
        }

    }

    void on(const Commit& commit)
    {
        if (state_ == State::Completed)
            return;
        state_ = State::Completed;
        carries_ = commit.carrySet;
        this->triggerAllExceptSelf<Replob>(Commit{carries_});
        complete();
    }

    void on(const Disconnect&)
    {
        NodesSet nodesSet;
        for (NodeId nId = 0; nId < config->nodes; ++ nId)
            if (nId != context().sourceNode)
                nodesSet |= nId;
        on(Vote{carries_, context().currentNode, nodesSet});
    }

    void complete()
    {
        commited = carries_;
    }

    State state_ = State::Initial;
    NodesSet nodes_;
    NodesSet voted_;
    CarrySet carries_;
    CarrySet commited;
    An<Config> config;
};

struct Client : Service<Client>
{
    using Service<Client>::on;

    void on(const Init&)
    {
        this->trigger<Replob>(Apply{});
    }

    void on(const Disconnect&)
    {
        disconnected |= context().sourceNode;
    }

    void test()
    {
        CHECK3(!accessor.service<Replob>(0).commited.empty(), "must be commited");
    }

    NodesSet disconnected;
    ServiceAccessor accessor;
};

void replob()
{
    TLOG("Testing replob commit");
    ServiceCreator c;
    c.create<Client>(0);
    c.create<Replob>(0, c.config().nodes);
    
    ServiceAccessor a;
    TrueScheduler s {[&a] {
        //CHECK3(!a.service<Replob>(0).commited.empty(), "must be commited");
        a.service<Client>(0).test();
    }};
    s.run();
}
