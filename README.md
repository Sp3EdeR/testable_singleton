[![unit test status](https://github.com/Sp3EdeR/testable_singleton/actions/workflows/Tests.yml/badge.svg?branch=main)](https://github.com/Sp3EdeR/testable_singleton/actions/workflows/Tests.yml?query=branch%3Amain)

# Testable Singleton
This header-only C++ library implements the [Singleton pattern](https://en.wikipedia.org/wiki/Singleton_pattern) in a thread-safe and efficient way.

A great issue with singletons normally is that they make unit testing very hard. This library also makes testing relatively easy with additional interfaces that allow resetting the singleton's state to properly test its behaviour. Furthermore, it is also important to be able to test code that depends on singletons, and this implementation lets its users easily inject [mock implementations](https://en.wikipedia.org/wiki/Mock_object) of the singleton. In order to improve code security, the interfaces intended for testing are designed to be unaccessible in production code.

# Usage

To start using this library, include the `singleton.hpp` file into your code, and inherit from the `Singleton` class in your code and get its instance as follows:

```cpp
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

The singleton has an interface designed for testing only. These interfaces should not be used in production code, because the software stability and quality cannot be guaranteed.

To access the testing interfaces, test code can include the `singleton_test.hpp` file. This provides access for the tests to use the `::testing::SingletonTestApi` class.
The following example shows the usage of the `::testing::SingletonTestApi<T>::Reconstruct()` and `::testing::SingletonTestApi<T>::Inject()` methods.

```cpp
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
```

> [!NOTE]
> To be able to properly use the `Inject` function, the production code should not cache a reference or pointer to the returned instance. Otherwise the injected mock doesn't take effect and the real instance is used, which is destroyed. This may cause a crash or an other memory corruption style issue.

For more examples and behavioral requirements, it is recommended to view this library's [test code](blob/main/test/unit/singleton_test.cpp).

> [!TIP]
> Earlier versions of this library used the [access_private library](https://github.com/martong/access_private/) by [Gábor Márton](https://github.com/martong) to access the Singleton's testing-only methods through explicit template instantiation's disabled accessibility check. This method is still supported, but it is untested. In version 2.0, the `Reset` method's signature is changed to use universal references, so `&&` must be added after all parameters.

# Building with CMake

This library is a header-only library, so there is no need to build it to use it. A CMake build script is provided to facilitate installation, documentation generation, and unit testing. To build the library's resources, follow [the CMake tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/Before%20You%20Begin.html)'s instructions. To build the documentation, run `cmake --build <build-directory> --target singleton_doc`. To install the library, run `cmake --install <build-directory>` after configuring the build system.

## Build Customization

By default, the CMake build script:
* installs the library's header files to the system include directory,
  * if the documentation is built, then it is also installed,
* creates a build target `singleton_doc` to builds the library's documentation, if Doxygen is found on the system,
* builds and registers its unit test with CTest.

To customize any of these behaviors, the following CMake options can be used:
* Set the CMake variable SINGLETON_DO_INSTALL=OFF to disable the installation of the library's header files.
* Set the CMake variable SINGLETON_BUILD_DOC=OFF to disable the documentation build target.
* Set the CMake variable SINGLETON_BUILD_TESTS=OFF to disable the unit test for this library.
* Or set the CMake variable BUILD_TESTING=OFF to disable all tests in the CMake build system, including this library's unit test.
