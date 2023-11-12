# Testable Singleton
This header-only C++ library implements the [Singleton pattern](https://en.wikipedia.org/wiki/Singleton_pattern) in a thread-safe and efficient way.

A great issue with singletons normally is that they make unit testing very hard. This library also makes testing relatively easy with additional interfaces that allow resetting the singleton's state to properly test its behaviour. Furthermore, it is also important to be able to test code that depends on singletons, and this implementation lets its users easily inject [mock implementations](https://en.wikipedia.org/wiki/Mock_object) of the singleton. In order to improve code security, the interfaces intended for testing are designed to be unaccessible in production code.

# Usage

To start using this library, include the `singleton.hpp` file into your code, and inherit from the `Singleton` class in your code and get its instance as follows:

```cpp
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
```

@note It is recommended to define a `private` or `protected` constructor to avoid misuse of the singleton. In this case, the `friend BaseType;` declaration must be added to the body of the class to allow class construction for the singleton.

It is also possible to use a non-default constructor for the singleton class. In this case, initialization arguments must be provided to the `Get()` function at every location where the singleton is accessed, or the `TryGet()` function must be used, which does not initialize the singleton.

```cpp
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
```

For more information about its usage, see the documentation within the [include/singleton.hpp](blob/main/include/singleton.hpp) file.

# Testing

The singleton has private methods which are designed for testing. These methods are normally inaccessible for production code, which is desired for software stability and quality.

The C++ standard states that during explicit template instantiations accessibility checking is not performed. It is possible to exploit this to gain access to private members. Why should we want to get around the accessibility limitation of private members? Because unit tests often need to be able to introspect objects. In the case of this library, the unsafe testing functionality is only accessible by exploiting this behaviour.

While it is complicated and impractical to write the template code to access private members - which is a good thing - there are multiple libraries designed to gain access to this functionality for unit tests. The following example shows the usage of the `Reset()` and `Inject()` methods through the [access_private library](https://github.com/martong/access_private/) by [Gábor Márton](https://github.com/martong).

```cpp
#include <singleton.hpp>
#include <access_private.hpp>

class MySingleton : public Singleton<MySingleton>
{
protected:
    // A protected allows a mock to subclass this.
    MySingleton()
    {
    }
    friend BaseType;

    virtual int MyFunction();
};
ACCESS_PRIVATE_STATIC_FUN(MySingleton, MySingleton&(), Reset);
ACCESS_PRIVATE_STATIC_FUN(MySingleton, void(MySingleton*), Inject);

int main()
{
    // This returns the constructed singleton.
    auto& instance = MySingleton::Get();

    // This returns the real implementation's result.
    auto realResult1 = instance.MyFunction();

    // This reconstructs the singleton instance and returns the same instance as earlier.
    auto& instance2 = call_private_static::MySingleton::Reset();

    // This returns the real implementation's result from the fresh instance.
    auto realResult2 = instance.MyFunction();

    struct MyMockSingleton : public MySingleton
    {
        // Override the MyFunction behaviour in the mock.
        virtual int MyFunction() override;
    };
    MyMockSingleton mock;

    // This injects the mock implementation.
    // The real instance is destroyed, the `instance` and `instance2` variables are invalidated.
    call_private_static::MySingleton::Inject(&mock);

    // This returns the mock implementation.
    auto& instance3 = MySingleton::Get();

    return 0;
}
```

@note To be able to properly use the `Inject` function, the production code should not cache a reference or pointer to the returned instance. Otherwise the injected mock doesn't take effect and the real instance is used, which is destroyed. This may cause a crash or an other memory corruption style issue.

For more examples and "requirements", it is recommended to view this library's [test code](blob/main/test/unit/singleton_test.cpp).
