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
#include <unordered_map>
#include <algorithm>

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
std::set<T> operator&(const std::set<T>& s, const std::set<T>& t)
{
    std::set<T> newSet;
    for (auto&& e: s)
        if (t.count(e) == 1)
            newSet |= e;
    return newSet;
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

template<typename T, typename K>
typename T::mapped_type findOrEmpty(const T& t, const K& key)
{
    auto it = t.find(key);
    if (it != t.end())
        return it->second;
    return {};
}

struct MsgId
{
    MsgId() = default;
    MsgId(int id_) : id{id_} {}

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

std::ostream& operator<<(std::ostream& o, const MsgId& m)
{
    return o << ":" << m.id;
}

using NodeId = int;

using CarrySet = std::set<MsgId>;
using NodesSet = std::set<NodeId>;

struct Apply
{
    MsgId id;
};

struct Replob : Service<Replob>
{
    using Service<Replob>::on;

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
            broadcast(Vote{carries_, context().currentNode, nodes_});
        }
    }

    void on(const Commit& commit)
    {
        if (state_ == State::Completed)
            return;
        state_ = State::Completed;
        carries_ = commit.carrySet;
        broadcast(Commit{carries_});
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
        committed = carries_;
    }

    State state_ = State::Initial;
    NodesSet nodes_;
    NodesSet voted_;
    CarrySet carries_;
    CarrySet committed;
    An<Config> config;
};

struct Replob2 : Service<Replob2>
{
    using Service<Replob2>::on;

    struct Vote
    {
        CarrySet carrySet;
        NodesSet nodesSet;
    };

    struct Commit
    {
        CarrySet carrySet;
    };

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
        on(Vote{CarrySet{apply.id}, nodes_});
    }

    void on(const Vote& vote)
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
                on(Commit{carries_});
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
            broadcast(Vote{carries_, nodes_});
        }
    }

    void on(const Commit& commit)
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
            on(Vote{carries_, nodes_-NodesSet{context().sourceNode}});
    }

    void complete()
    {
        committed = carries_;
    }

    State state_ = State::ToVote;
    NodesSet nodes_;
    NodesSet voted_;
    CarrySet carries_;

    CarrySet committed;
    An<Config> config;
};

struct Replob4 : Service<Replob4>
{
    using Service<Replob4>::on;

    struct Vote
    {
        CarrySet carries;
        NodesSet nodes;
        NodesSet votes;
    };

    struct Commit {};

    enum struct State
    {
        Voting,
        Committed,
    };

    void on(const Init&)
    {
        for (NodeId nId = 0; nId < config->nodes; ++ nId)
            nodes_ |= nId;
    }

    void on(const Apply& apply)
    {
        SLOG("Apply");
        on(Vote{CarrySet{apply.id}, nodes_, {}});
    }

    void on(const Vote& vote)
    {
        if (state_ == State::Committed)
            return;
        if (nodes_.count(context().sourceNode) == 0)
            return;
        NodesSet nodes = vote.nodes & nodes_;
        CarrySet carries = vote.carries | carries_;
        NodesSet votes = {context().currentNode};
        if (nodes == vote.nodes && carries == vote.carries)
            votes |= vote.votes;
        if (nodes == nodes_ && carries == carries_)
            votes |= votes_;
        votes &= nodes; // !!!! verify???
        if (nodes != nodes_ || carries != carries_ || votes != votes_)
        {
            nodes_ = nodes;
            votes_ = votes;
            carries_ = carries;
            if (nodes_ == votes_)
            {
                // commit
                SLOG("Need commit");
                on(Commit{});
            }
            else
            {
                // broadcast
                SLOG("Need broadcast");
                broadcast(Vote{carries_, nodes_, votes_});
            }
        }
    }

    void on(const Disconnect&)
    {
        SLOG("Disconnect");
        if (carries_.empty())
            nodes_.erase(context().sourceNode);
        else
            on(Vote{carries_, nodes_-NodesSet{context().sourceNode}, {}});
    }

    void on(const Commit&)
    {
        if (state_ == State::Committed)
            return;
        SLOG("Commit");
        state_ = State::Committed;
        VERIFY(committed.empty(), "Already committed");
        committed = carries_;
        broadcast(Commit{});
    }

    State state_ = State::Voting;
    NodesSet nodes_;
    NodesSet votes_;
    CarrySet carries_;

    CarrySet committed;
    An<Config> config;
};

struct Replob5 : Service<Replob5>
{
    using Service<Replob5>::on;

    struct Vote
    {
        CarrySet carries;
        NodesSet nodes;
        //NodesSet votes;
    };

    struct Commit {};

    enum struct State
    {
        Voting,
        Committed,
    };

    void on(const Init&)
    {
        for (NodeId nId = 0; nId < config->nodes; ++ nId)
            aliveNodes_ |= nId;
    }

    void on(const Apply& apply)
    {
        SLOG("Apply");
        on(Vote{CarrySet{apply.id}, aliveNodes_});
    }

    void on(const Vote& vote)
    {
        if (state_ == State::Committed)
            return;
        if (aliveNodes_.count(context().sourceNode) == 0)
            return;
        bool changedAliveNodes = aliveNodes_ != vote.nodes;
        bool firstVote = votes_.empty();
        aliveNodes_ &= vote.nodes;
        carries_ |= vote.carries;
        votes_ |= context().currentNode;
        votes_ |= context().sourceNode;
        votes_ &= aliveNodes_; // VERIFY???
        for (auto&& carry: vote.carries)
            carryVotes_[carry.id] |= context().sourceNode;

        if (firstVote)
        {
            SLOG("Need broadcast due to first vote");
            broadcast(Vote{carries_, aliveNodes_});
            return;
        }

        if (changedAliveNodes)
        {
            SLOG("Clearing votes");
            votes_.clear();
            votes_ |= context().currentNode;
            if (votes_ != aliveNodes_)
            {
                broadcast(Vote{carries_, aliveNodes_});
                return;
            }
        }

        if (votes_ != aliveNodes_)
        {
            SLOG("Need to wait more messages");
            return;
        }

        if (mayCommit())
        {
            on(Commit{});
        }
        else
        {
            SLOG("Need broadcast");
            broadcast(Vote{carries_, aliveNodes_});
        }
    }

    void on(const Disconnect&)
    {
        SLOG("Disconnect");
        if (carries_.empty())
            aliveNodes_.erase(context().sourceNode);
        else
            on(Vote{carries_, aliveNodes_-NodesSet{context().sourceNode}});
    }

    void on(const Commit&)
    {
        if (state_ == State::Committed)
            return;
        SLOG("Commit");
        state_ = State::Committed;
        VERIFY(committed.empty(), "Already committed");
        committed = carries_;
        broadcast(Commit{});
    }

    bool mayCommit() const
    {
        for (auto&& kv: carryVotes_)
            if (!(2 * kv.second.size() > aliveNodes_.size()))
                return false;
        return true;
    }

    State state_ = State::Voting;

    NodesSet aliveNodes_;
    // int = msg.id
    NodesSet votes_;
    std::unordered_map<int, NodesSet> carryVotes_;
    CarrySet carries_;

    CarrySet committed;
    An<Config> config;
};

using Messages = std::vector<MsgId>;

struct VectorMessageHash
{
    template<typename T>
    size_t operator()(const std::vector<T>& vec) const
    {
        size_t res = 0;
        for (auto&& t: vec)
        {
            res ^= t.id;
            res <<= 5;
        }
        return res;
    }
};

namespace std {

template<> struct hash<MsgId>
{
    size_t operator()(const MsgId& id) const
    {
        return id.id;
    }
};

}

int majority()
{
    An<Config> config;
    return config->nodes / 2 + 1;
}

struct ReplobRush : Service<ReplobRush>
{
    using Service<ReplobRush>::on;

    struct MessagesGeneration
    {
        Messages messages;
        int generation = 0;

        bool operator==(const MessagesGeneration& m) const
        {
            return messages == m.messages && generation == m.generation;
        }
    };

    struct State
    {
        CarrySet carries;
        std::vector<MessagesGeneration> nodesMessages;
        std::unordered_map<Messages, NodesSet, VectorMessageHash> promises;

        bool operator==(const State& s) const
        {
            return carries == s.carries && nodesMessages == s.nodesMessages && promises == s.promises;
        }

        bool operator!=(const State& s) const
        {
            return !this->operator==(s);
        }

        MsgId getMajorityId(int idx) const
        {
            std::unordered_map<MsgId, int> count;
            for (auto&& gen: nodesMessages)
            {
                if (idx < gen.messages.size())
                {
                    ++ count[gen.messages[idx]];
                }
            }
            for (auto&& c: count)
            {
                if (c.second >= majority())
                {
                    return c.first;
                }
            }
            return MsgId{-1};
        }

        MessagesGeneration& current()
        {
            return nodesMessages[context().currentNode];
        }
    };

    enum struct Status
    {
        Voting,
        Committed,
    };

    void on(const Init&)
    {
        state_.nodesMessages.resize(config->nodes);
    }

    void on(const Apply& apply)
    {
        SLOG("Apply");
        on(State{CarrySet{apply.id}, std::vector<MessagesGeneration>(config->nodes), std::unordered_map<Messages, NodesSet, VectorMessageHash>()});
    }

    void on(const State& incomingState)
    {
        if (status_ == Status::Committed)
            return;
        SLOG("incoming state");
        State newState;
        newState.nodesMessages = state_.nodesMessages;
        // union messages
        for (int i = 0; i < config->nodes; ++ i)
        {
            if (incomingState.nodesMessages[i].generation > newState.nodesMessages[i].generation)
            {
                newState.nodesMessages[i] = incomingState.nodesMessages[i];
            }
        }
        // add new messages
        newState.carries = state_.carries;
        for (auto&& msg: incomingState.carries - newState.carries)
        {
            newState.carries.insert(msg);
            newState.current().messages.push_back(msg);
            newState.current().generation = state_.current().generation + 1;
        }
        // iteration and promising
        bool sorted = false;
        Messages promiseMessages = committed;
        Messages commitMessages = committed;
        for (int i = committed.size(); i < newState.carries.size(); ++ i)
        {
            MsgId id = newState.getMajorityId(i);
            if (id.id >= 0)
            {
                // found majority, add promise
                SLOG("found majority, add promise: " << id.id);
                promiseMessages.push_back(id);
                NodesSet votes = NodesSet{context().currentNode}
                        | findOrEmpty(state_.promises, promiseMessages)
                        | findOrEmpty(incomingState.promises, promiseMessages);
                newState.promises[promiseMessages] = votes;
                if (votes.size() >= majority())
                {
                    // votes has majority => may be committed
                    SLOG("votes has majority => may be committed: " << id.id);
                    commitMessages = promiseMessages;
                }
            }
            else
            {
                if (!sorted)
                {
                    // try to sort
                    SLOG("try to sort from: " << i);
                    sorted = true;
                    auto&& messages = newState.current().messages;
                    std::sort(messages.begin() + i, messages.end());
                    // retry attempt
                    -- i;
                    continue;
                }
                else
                {
                    SLOG("there is no majority even with sorting: " << i);
                    break; // there is no majority even with sorting => no other promises
                }
            }
        }
        if (newState != state_)
        {
            // update state and generation
            SLOG("update state with generation: " << newState.current().generation);
            state_ = newState;
            broadcast(newState);
            if (commitMessages != committed)
            {
                SLOG("new committed messages" << commitMessages);
                VERIFY(commitMessages.size() > committed.size(), "new commit size must be more");
                // TODO: add prefix verification
                committed = commitMessages;
            }
        }
    }

    void on(const Disconnect&)
    {
        SLOG("Disconnect");
    }

    Status status_ = Status::Voting;
    State state_;

    Messages committed;
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
        decltype(T_replob::committed) committed;
        for (int i = 0; i < config->nodes; ++ i)
        {
            bool isDisconnected = disconnected.count(i) != 0;
            auto& nodeCommited = accessor.service<T_replob>(i).committed;
            if (isDisconnected)
            {
                CHECK3(nodeCommited.size() <= committed.size(), "invalid committed size");
                auto it1 = committed.begin();
                auto it2 = nodeCommited.begin();
                while (it2 != nodeCommited.end())
                {
                    CHECK3(*it1 == *it2, "invalid committed prefix");
                    ++ it1;
                    ++ it2;
                }
                continue;
            }
            CHECK3(!nodeCommited.empty(), "must be commited for node: "
                   << i << ", is disconnected: " << isDisconnected);
            if (committed.empty())
                committed = nodeCommited;
            else
            {
                if (committed != nodeCommited)
                {
                    for (auto&& n: committed)
                    {
                        RLOG("commited: " << n.id);
                    }
                    for (auto&& n: nodeCommited)
                    {
                        RLOG("node commited: " << n.id);
                    }
                    RLOG("node: " << i);
                }
                CHECK3(committed == nodeCommited, "must be consensus for the same messages: " + std::to_string(i));
            }
        }
    }

    NodesSet disconnected;
    ServiceAccessor accessor;
    An<Config> config;
};

template<typename T_replob>
void testReplobImpl(int clientCommits)
{
    using R = T_replob;
    using C = Client<R>;

    An<Config> config;
    config->maxFails = 1;
    config->maxIterations = 0;
    config->maxFailedNodes = 1;

    ServiceCreator c;
    c.create<C>(0, clientCommits);
    c.create<R>(0, c.config().nodes);

    ServiceAccessor a;
    TrueScheduler s {[&a] {
        a.service<C>(0).test();
    }};
    s.run();
    //s.checkVariant({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2});
}

#define DEF_REPLOB(D_replob) \
    void test##D_replob(int clientCommits) \
    { \
        TLOG("Checking " #D_replob); \
        testReplobImpl<D_replob>(clientCommits); \
    }

DEF_REPLOB(Replob)
DEF_REPLOB(Replob2)
DEF_REPLOB(Replob4)
DEF_REPLOB(Replob5)
DEF_REPLOB(ReplobRush)
