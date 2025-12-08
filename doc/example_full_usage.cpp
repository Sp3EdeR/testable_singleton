#include <singleton.hpp>
#include <singleton_test.hpp>

class MySingleton : public Singleton<MySingleton>
{
protected:
    // A protected allows a mock to subclass this.
    MySingleton()
    {
        ++m_allocCounter;
    }
    friend BaseType;
    static int m_allocCounter;
public:
    virtual int GetAllocCount()
    {
        return m_allocCounter;
    }
};
int MySingleton::m_allocCounter = 0;

int main()
{
    // This returns the constructed singleton.
    auto& instance = MySingleton::Get();

    // This returns the real implementation's result.
    if (instance.GetAllocCount() != 1)
        return 1;

    // This reconstructs the singleton instance and returns the same instance as earlier.
    auto& instance2 = testing::SingletonTestApi<MySingleton>::Reconstruct();

    // This returns the real implementation's result from the fresh instance.
    if (instance.GetAllocCount() != 2 || &instance != &instance2)
        return 2;

    struct MyMockSingleton : public MySingleton
    {
        // Override the GetAllocCount behaviour in the mock.
        virtual int GetAllocCount() override
        {
            return -1;
        }
    };
    MyMockSingleton mock;

    // This injects the mock implementation.
    // The real instance is destroyed, the `instance` and `instance2` variables are invalidated.
    testing::SingletonTestApi<MySingleton>::Inject(&mock);

    // This returns the mock implementation.
    auto& instance3 = MySingleton::Get();

    // This calls the mocked GetAllocCount() instead of the normal one.
    if (instance3.GetAllocCount() != -1)
        return 3;

    return 0;
}
