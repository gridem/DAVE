#define ELOG(D_msg)         JLOG("EMUL: " << D_msg)

struct Config
{
    int maxFailedNodes = 1;
    int maxIterations = 100000;
    int maxSteps = 50;
};

struct Nodes
{
    void setNodes(int nodesCount)
    {
        nodes.resize(nodesCount);
    }
    
    size_t size() const
    {
        return nodes.size();
    }
    
    // resize nodes if necessary
    Node& sizedNode(size_t nd)
    {
        if (nodes.size() <= nd)
            nodes.resize(nd + 1);
        return nodes[nd];
    }
    
    Node& node(size_t nd)
    {
        VERIFY(nd < nodes.size(), "Invalid node index");
        return nodes[nd];
    }
    
    void init()
    {
        for (Node& n: nodes)
            n.createProcesses();
        int i = 0;
        for (Node& n: nodes)
        {
            context() = {-1, i ++};
            n.initProcesses();
        }
        context() = {};
    }
    
    void disconnect(size_t nd)
    {
        VERIFY(nd < nodes.size(), "Invalid node index");
        VERIFY(nd != 0, "Zero node is protected against disconnects");
        ++ stats->disconnects;
        ++ globalStats->disconnects;
        int i = 0;
        for (Node& n: nodes)
        {
            if (i != nd)
            {
                ContextGuard guard({int(nd), i});
                n.disconnectProcesses();
            }
            else
            {
                n.shutdownProcesses();
            }
            ++ i;
        }
    }
    
    void shutdown()
    {
        for (Node& n: nodes)
            n.shutdownProcesses();
    }
    
private:
    std::vector<Node> nodes;
    An<Stats> stats;
    An<GlobalStats> globalStats;
};

template<typename T_service, typename T_msg>
std::string triggerName(const Context& ctx = context())
{
    return getTypeName<T_service>() + "::" + getTypeName<T_msg>()
            + " " + std::to_string(ctx.sourceNode) + "=>" + std::to_string(ctx.currentNode);
}

inline std::string disconnectionName(int dstNode)
{
    return "node disconnection: " + std::to_string(dstNode);
}

struct Emulator
{
    template<typename T_service, typename T_msg>
    void trigger(int dstNode, const T_msg& msg)
    {
        Context ctx = destinationContext(dstNode);
        auto& handler = NodeHandler::create(
            [this, ctx, dstNode, msg] {
                context() = ctx;
                nodes->node(dstNode).getProcess<T_service>().on(msg);
            },
            triggerName<T_service, T_msg>(ctx),
            EventType::Trigger
        );
        nodes->node(dstNode).getProcess<T_service>().push(handler);
    }
    
    std::vector<NodeHandler*> available()
    {
        std::vector<NodeHandler*> result;
        for (IHandlerQueue& q: *queues)
        {
            if (!q.empty())
                result.push_back(&q.front());
        }
        for (NodeHandler& h: nodeDisconnects)
            result.push_back(&h);
        return result;
    }
    
    void printAvailable()
    {
        ELOG("print available handlers");
        for (auto* h: available())
        {
            ELOG("available: " << h->name << " [" << h->type << "]");
        }
    }
    
    void init()
    {
        // clear all handler queues (process may be excluded on shutdown)
        queues->clear();
        nodes->shutdown();
        nodes->init();
        initDisconnections();
        printAvailable();
    }
    
private:
    void initDisconnections()
    {
        while (!nodeDisconnects.empty())
            delete &nodeDisconnects.back();
        VERIFY(nodeDisconnects.empty(), "handler disconnects clear failed");
        for (size_t dstNode = 1; dstNode < nodes->size(); ++ dstNode)
        {
            auto& handler = NodeHandler::create(
                [this, dstNode] {
                    nodes->disconnect(dstNode);
                },
                disconnectionName(dstNode),
                EventType::Disconnect
            );
            nodeDisconnects.push_back(handler);
        }
    }

    An<Nodes> nodes;
    An<Queues> queues;
    
    List<NodeHandler, &NodeHandler::queue> nodeDisconnects;
};

