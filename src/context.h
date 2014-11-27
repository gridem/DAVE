struct Context
{
    Context(int src = -1, int cur = -1) : sourceNode(src), currentNode(cur) {}
    
    int sourceNode;
    int currentNode;
};

inline std::ostream& operator<<(std::ostream& o, const Context& ctx)
{
    return o << std::to_string(ctx.sourceNode) << " -> " << std::to_string(ctx.currentNode);
}

inline Context& context()
{
    return single<Context>();
}

inline Context destinationContext(int dst)
{
    return {context().currentNode, dst};
}

struct ContextGuard
{
    ContextGuard(const Context& ctx) : old(context())
    {
        context() = ctx;
    }
    
    ~ContextGuard()
    {
        context() = old;
    }
    
private:
    Context old;
};
