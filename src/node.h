struct Init {};
struct Disconnect {};

struct IProcess : HandlerQueue
{
    virtual void create() = 0;
    virtual void init() = 0;
    virtual void disconnect() = 0;
    
    ListHook node;
};

template<typename T_service>
struct Process : IProcess
{
    template<typename T_msg>
    void on(const T_msg& msg)
    {
        getService().on(msg);
    }
    
    T_service& getService()
    {
        return service;
    }
    
    void create() override
    {
        reCtor(service);
    }
    
    void init() override
    {
        on(Init());
    }
    
    void disconnect() override
    {
        on(Disconnect());
    }

private:
    T_service service;
};

struct Node
{
    template<typename T_service>
    Process<T_service>& addProcess()
    {
        auto& p = processes.add<Process<T_service>>();
        processList.push_back(p);
        return p;
    }
    
    template<typename T_service>
    Process<T_service>& getProcess() const
    {
        return processes.get<Process<T_service>>();
    }
    
    template<typename T_service>
    bool hasProcess() const
    {
        return on && processes.has<Process<T_service>>();
    }
    
    void createProcesses()
    {
        on = true;
        for (auto& p: processList)
        {
            p.create();
            queues->push_back(p);
        }
    }
    
    void initProcesses()
    {
        for (auto& p: processList)
            p.init();
    }
    
    // notify about disconnections
    void disconnectProcesses()
    {
        if (!on)
            return;
        for (auto& p: processList)
            p.disconnect();
    }
    
    void shutdownProcesses()
    {
        for (auto& p: processList)
            p.clear();
        on = false;
    }
    
private:
    bool on = false;
    
    TypeContainer processes;
    List<IProcess, &IProcess::node> processList;
    An<Queues> queues;
};
