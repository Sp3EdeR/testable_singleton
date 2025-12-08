#include <singleton.hpp>

class MySingleton : public Singleton<MySingleton>
{
private:
    MySingleton(int customArg1, double customArg2)
    {
    }
    friend BaseType;
};

int main()
{
    {
        // This returns a nullptr, because the singleton is uninitialized.
        auto instancePtr = MySingleton::TryGet();
    }
    {
        // This returns the constructed singleton.
        auto& instance = MySingleton::Get(42, 3.1415);
    }
    {
        // This returns a pointer to the initialized singleton.
        auto instancePtr = MySingleton::TryGet();
    }
    {
        // This returns the same singleton.
        // The difference in arguments is ignored.
        auto& instance = MySingleton::Get(42000, -3.1415);
    }

    return 0;
}
