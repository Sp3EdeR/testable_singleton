#include <singleton.hpp>

class MySingleton : public Singleton<MySingleton>
{
private:
    MySingleton() = default;
    friend BaseType;
};

int main()
{
    // This returns the constructed singleton.
    auto& instance = MySingleton::Get();

    // Call functions on `instance` here.

    return 0;
}
