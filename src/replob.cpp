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

    bool operator==(const MsgId& m) const
    {
        return id == m.id;
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

    void on(const Init&)
    {
        for (NodeId nId = 0; nId < config->nodes; ++ nId)
            nodes_ |= nId;
    }

    void on(const Apply& apply)
    {
        SLOG("Apply");
        on(Vote{CarrySet{apply.id}, context().currentNode, nodes_});
    }

    void on(const Vote& vote)
    {
        if (state_ == State::Completed)
            return;
        if (nodes_.count(context().sourceNode) == 0)
            return;
        carries_ |= vote.carrySet;
        if (nodes_ != vote.nodesSet)
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
        if (carries_.empty())
            nodes_.erase(context().sourceNode);
        else
            on(Vote{carries_, context().currentNode, nodes_-NodesSet{context().sourceNode}});
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

struct Vote2
{
    CarrySet carrySet;
    NodesSet nodesSet;
    //NodesSet votedSet;
};

struct Commit2
{
    CarrySet carrySet;
};

struct Replob2 : Service<Replob2>
{
    using Service<Replob2>::on;

    enum struct State
    {
        ToVote, // can vote only in initial state
        MayCommit,
        CannotCommit,
        Completed,
    };

    void on(const Init&)
    {
        for (NodeId nId = 0; nId < config->nodes; ++ nId)
            nodes_ |= nId;
    }

    void on(const Apply& apply)
    {
        SLOG("Apply");
        on(Vote2{CarrySet{apply.id}, nodes_});
    }

    void on(const Vote2& vote)
    {
        if (state_ == State::Completed)
            return;
        if (nodes_.count(context().sourceNode) == 0)
            return;
        if (state_ == State::MayCommit && carries_ != vote.carrySet)
        {
            state_ = State::CannotCommit;
        }
        carries_ |= vote.carrySet;
        voted_ |= context().sourceNode;
        voted_ |= context().currentNode;
        if (nodes_ != vote.nodesSet)
        {
            if (state_ == State::MayCommit)
                state_ = State::CannotCommit;
            nodes_ &= vote.nodesSet;
            voted_ &= vote.nodesSet;
        }
        if (voted_ == nodes_)
        {
            if (state_ == State::MayCommit)
            {
                on(Commit2{carries_});
                return; // ?
            }
            else
            {
                state_ = State::ToVote;
            }
        }
        if (state_ == State::ToVote)
        {
            state_ = State::MayCommit;
            broadcast(Vote2{carries_, nodes_});
        }
    }

    void on(const Commit2& commit)
    {
        if (state_ == State::Completed)
            return;
        state_ = State::Completed;
        SLOG("Broadcasting commit");
        if (carries_ != commit.carrySet)
        {
            SLOG("Invalid carry");
            for (auto&& n: carries_)
            {
                RLOG("carries: " << n.id);
            }
            for (auto&& n: commit.carrySet)
            {
                RLOG("to commit: " << n.id);
            }
        }
        VERIFY(carries_ == commit.carrySet, "Invalid carry set on commit");
        carries_ = commit.carrySet;
        complete();
        broadcast(commit);
    }

    void on(const Disconnect&)
    {
        if (carries_.empty())
            nodes_.erase(context().sourceNode);
        else
            on(Vote2{carries_, nodes_-NodesSet{context().sourceNode}});
    }

    void complete()
    {
        commited = carries_;
    }

    template<typename T>
    void broadcast(const T& t)
    {
        this->triggerAllExceptSelf<Replob2>(t);
    }

    State state_ = State::ToVote;
    bool needReinit_ = false;
    NodesSet nodes_;
    NodesSet voted_;
    CarrySet carries_;
    CarrySet commited;
    An<Config> config;
};

template<typename T_replob>
struct Client : Service<Client<T_replob>>
{
    using Service<Client>::on;

    void on(const Init&)
    {
        this->template trigger<T_replob>(Apply{});
    }

    void on(const Disconnect&)
    {
        disconnected |= context().sourceNode;
    }

    void test()
    {
        CarrySet commited;
        for (int i = 0; i < config->nodes; ++ i)
        {
            bool isDisconnected = disconnected.count(i) != 0;
            auto& nodeCommited = accessor.service<T_replob>(i).commited;
            if (isDisconnected && nodeCommited.empty())
                continue;
            CHECK3(!nodeCommited.empty(), "must be commited for node: "
                   << i << ", is disconnected: " << isDisconnected);
            if (commited.empty())
                commited = nodeCommited;
            else
            {
                if (commited != nodeCommited)
                {
                    for (auto&& n: commited)
                    {
                        RLOG("commited: " << n.id);
                    }
                    for (auto&& n: nodeCommited)
                    {
                        RLOG("node commited: " << n.id);
                    }
                    RLOG("node: " << i);
                }
                CHECK3(commited == nodeCommited, "must be consensus for the same messages: " + std::to_string(i));
            }
        }
    }

    NodesSet disconnected;
    ServiceAccessor accessor;
    An<Config> config;
};

void replob()
{
    using R = Replob;
    using C = Client<R>;

    TLOG("Testing replob commit");
    ServiceCreator c;
    c.create<C>(0, 2);
    c.create<R>(0, c.config().nodes);
    
    ServiceAccessor a;
    TrueScheduler s {[&a] {
        a.service<C>(0).test();
    }};
    s.run();
    //s.checkVariant({0, 0, 2, 2, 2, 0, 0, 0, 0, 0});
}

void replob2(int clientCommits)
{
    using R = Replob2;
    using C = Client<R>;

    An<Config> config;
    config->maxFails = 1;
    config->maxIterations = 0;

    TLOG("Testing replob2 commit");
    ServiceCreator c;
    c.create<C>(0, clientCommits);
    c.create<R>(0, c.config().nodes);

    ServiceAccessor a;
    TrueScheduler s {[&a] {
        a.service<C>(0).test();
    }};
    s.run();
    //s.checkVariant({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0});
}
