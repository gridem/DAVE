struct GlobalStats
{
    int iterations = 0;
    int disconnects = 0;
};

struct Stats : Initer<Stats>
{
    int disconnects = 0;
};

inline std::ostream& operator<<(std::ostream& o, const GlobalStats& s)
{
    return o << "iterations: " << s.iterations << ", disconnects: " << s.disconnects;
}
