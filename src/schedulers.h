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

#define CLOG(D_msg)     JLOG("SCH: " << D_msg)

struct VerificationFail : std::runtime_error
{
    VerificationFail() : std::runtime_error{"Verification failed"} {}
};

using Variant = std::vector<int>;

template<typename T_stream, typename T_iterable>
T_stream& outStream(T_stream& o, const T_iterable& it)
{
    o << '{';
    bool first = true;
    for (auto&& i: it)
    {
        if (first)
            first = false;
        else
            o << ", ";
        o << i;
    }
    return o << '}';
}

template<typename T>
std::ostream& operator<<(std::ostream& o, const std::vector<T>& v)
{
    return outStream(o, v);
}

template<typename T>
std::ostream& operator<<(std::ostream& o, const std::set<T>& v)
{
    return outStream(o, v);
}

struct FrontScheduler
{
    FrontScheduler(bool useOptional = false) : useOpt(useOptional) {}

    void runIteration()
    {
        CLOG("init");
        emulator->init();
        CLOG("init done");
        while (true)
        {
            auto available = emulator->available();
            if (available.empty())
                break;
            NodeHandler& h = *available.front();
            if (h.type != EventType::Trigger && !useOpt)
                break;
            h.invoke();
        }
        CLOG("done");
    }
    
private:
    bool useOpt;
    An<Emulator> emulator;
};

struct Variants
{
    void add(Variant v)
    {
        CLOG("added variant: " << v);
        variants.push_back(std::move(v));
    }
    
    void addExtent(const Variant& v, int nv)
    {
        Variant newV = v;
        newV.push_back(nv);
        add(std::move(newV));
    }
    
    bool get(Variant& v)
    {
        if (variants.empty())
            return false;
        v = std::move(variants.back());
        variants.pop_back();
        return true;
    }
    
private:
    std::vector<Variant> variants;
};

struct ServiceAccessor
{
    template<typename T_service>
    T_service& service(int node)
    {
        return nodes->node(node).getProcess<T_service>().getService();
    }
    
private:
    An<Nodes> nodes;
};

struct Scheduler : IObject
{
    Scheduler(Handler end) : onEnd(std::move(end))
    {
    }
    
    virtual Variant runIteration(Variant v) = 0;
    
    void init()
    {
        emulator->init();
        stats->init();
    }
    
    void run()
    {
        variants->add({});
        for (int i = 0; i < config->maxIterations || config->maxIterations == 0; ++ i)
        {
            Variant v;
            if (!variants->get(v))
            {
                CLOG("no variants");
                break;
            }
            ++ globalStats->iterations;
            execVariant(v);
            auto vend = runIteration(std::move(v));
            if (finalize(vend))
                break;
            if ((i+1) % config->progressIterations == 0)
            {
                RLOG("global stats: " << *globalStats);
                RLOG("Variant: " << vend);
            }
        }
        RLOG("global stats: " << *globalStats);
    }

    bool finalize(const Variant& v)
    {
        CLOG("on end");
        try
        {
            onEnd();
        }
        catch (VerificationFail&)
        {
            RLOG("Failed sequence: " << v);
            execVariant(v, true);
            if (++ fails == config->maxFails)
            {
                RLOG("Max fails reached");
                return true;
            }
        }
        CLOG("on end done");
        return false;
    }
    
    void execVariant(const Variant& v, bool show = false)
    {
        CLOG("init");
        init();
        CLOG("executing variant from scratch: " << v);
        for (int i: v)
        {
            if (show)
                emulator->available().at(i)->rdump();
            emulator->available().at(i)->invoke();
        }
        CLOG("done exec");
    }

    void checkVariant(const Variant& v)
    {
        execVariant(v);
        onEnd();
    }
    
protected:
    bool allowedDisconnection()
    {
        return stats->disconnects < config->maxFailedNodes;
    }

    int fails = 0;
    Handler onEnd;
    An<Emulator> emulator;
    An<Stats> stats;
    An<GlobalStats> globalStats;
    An<Config> config;
    An<Variants> variants;
};

struct TrueScheduler : Scheduler
{
    using Scheduler::Scheduler;
    
    Variant runIteration(Variant v)
    {
        while (true)
        {
            if (int(v.size()) >= config->maxSteps)
            {
                RLOG("Iteration exceeds the amount of steps: " << v);
                break;
            }
            //emulator->printAvailable();
            auto available = emulator->available();
            if (available.empty())
                break;
            int ni = -1;
            for (size_t i = 0; i < available.size(); ++ i)
            {
                if (available[i]->type == EventType::Trigger)
                {
                    if (ni == -1)
                    {
                        ni = i;
                    }
                    else
                    {
                        variants->addExtent(v, i);
                    }
                }
                else
                {
                    if (allowedDisconnection())
                        variants->addExtent(v, i);
                }
            }
            if (ni == -1)
            {
                // no any moves
                break;
            }
            v.push_back(ni);
            available[ni]->invoke();
        }
        CLOG("runIteration done");
        return v;
    }
};
