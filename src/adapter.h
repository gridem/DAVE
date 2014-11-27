template<typename T, typename T_impl>
struct Adapter;

template<typename T, typename T_impl, typename T_impl2>
struct Adapter<Adapter<T, T_impl>, T_impl2> : Adapter<T, T_impl2> {};

template<typename T_adapter>
struct Adapter<Service, T_adapter> : T_adapter
{
    typedef typename T_adapter::Impl Impl;
    
    template<typename T_msg>
    void on(const T_msg& msg)
    {
        this->call(&Impl::on, msg);
    }
};

