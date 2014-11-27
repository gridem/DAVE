struct IHandlerQueue : IObject
{
    virtual void clear() = 0;
    virtual bool empty() const = 0;
    virtual NodeHandler& front() = 0;
    virtual NodeHandler& pop() = 0;
    virtual void push(NodeHandler&) = 0;
    
    ListHook queues; // list: all queues
};

using Queues = List<IHandlerQueue, &IHandlerQueue::queues>;

template<typename T_queue>
auto queuePop(T_queue& q) -> decltype(q.front())
{
    auto& v = q.front();
    q.pop_front();
    return v;
}

struct HandlerQueue : IHandlerQueue
{
    void clear() override
    {
        while (!empty())
            delete &pop();
    }
    
    bool empty() const override
    {
        return queue.empty();
    }
    
    NodeHandler& front() override
    {
        return queue.front();
    }
    
    NodeHandler& pop() override
    {
        return queuePop(queue);
    }
    
    void push(NodeHandler& h) override
    {
        queue.push_back(h);
    }

private:
    List<NodeHandler, &NodeHandler::queue> queue;
};

