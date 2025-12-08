#ifndef TESTABLE_SINGLETON_INCLUDED_H
#define TESTABLE_SINGLETON_INCLUDED_H

/** @file
  * @brief Defines a singleton type. This type can be used in unit tests.
  */

#include <mutex>
#include <new>
#include <utility>

namespace testing
{
    // Forward declaration of the testing interface.
    template <typename T>
    struct SingletonTestApi;
}

/// Implements the singleton pattern, but makes unit testing easy.
/** To use the Singleton class normally, inherit from it with the [CRTP] style, like:
  *
  * ```cpp
  * class MyClass : public Singleton<MyClass> { impl... };
  * ```
  *
  * After this, external code is able to get the `MyClass` instance by using `MyClass::Get()`.
  *
  * To test your singleton, use `::testing::SingletonTestApi<T>` from `singleton_test.hpp`.
  * 
  * [CRTP]: https://en.cppreference.com/w/cpp/language/crtp.html
  */
template <typename T>
struct Singleton
{
    // The singleton type, usable by the singleton class.
    using BaseType = Singleton<T>;

    /// Returns the instance of the class.
    /** @remark The constructor arguments are only used if the instance is not constructed yet.
      *         On subsequent calls, get may be called without arguments. Use `TryGet()` instead to
      *         check if the instance is already constructed.
      * @remark The singleton instance returned by this function should not be cached, because when
      *         using Inject in tests, the instance may change.
      */
    template <typename ...Args>
    static T& Get(Args&&... args)
    {
        std::call_once(g_onceFlag, &EmplaceHelper<Args...>, std::forward<Args>(args)...);
        return *static_cast<T*>(g_instance);
    }

    ///  Returns the instance of the class without construction.
    /** @return The instance of the singleton or a `nullptr` if unconstructed.
      * @remark This can be useful for singletons that have custom constructor arguments. Code can
      *         get an already initialied instance, if it has been initialized already, without
      *         having to specify the arguments.
      * @remark The singleton instance returned by this function should not be cached, because when
      *         using Inject in tests, the instance may change.
      */
    static T* TryGet() noexcept
    {
        // NOTE: Thread safety here assumes that pointers can be read/written atomically.
        return g_instance;
    }

protected:
    Singleton() noexcept = default;
private:
    Singleton(const Singleton&) = delete;
    Singleton& operator =(const Singleton&) = delete;

    /// Holds the instance of `T`, either locally constructed or injected.
    static struct Instance final
    {
        Instance() noexcept = default;
        Instance(const Instance&) = delete;
        ~Instance()
        {
            // Destroys the locally-initialized instance. Injected ones are ignored (no ownership).
            if (m_pExtern == LOCAL_INSTANCE_ID)
                GetBuffer().~T();
        }
        Instance& operator =(const Instance&) = delete;
        /// Returns the current instance.
        operator T* () noexcept
        {
            // m_pExtern is used only by test code, so thread safety is not a concern here.
            return m_pExtern == LOCAL_INSTANCE_ID ? &GetBuffer() : m_pExtern;
        }
        /// Constructs the singleton within the local buffer.
        template <typename ...Args>
        void Emplace(Args&&... args)
        {
            this->~Instance();
            try
            {
                new (&GetBuffer()) T(std::forward<Args>(args)...);
                m_pExtern = LOCAL_INSTANCE_ID;
            }
            catch (...)
            {
                m_pExtern = nullptr;
                throw;
            }
        }
        /// Sets an external object as the instance.
        /** @remark If `ptr` is `nullptr`, this is just reset.
          */
        void SetExtern(T* ptr)
        {
            this->~Instance();
            m_pExtern = ptr;
        }
    private:
        /// A special pointer value to specify that the local buffer is constructed.
        static T* const LOCAL_INSTANCE_ID;
        /// nullptr if empty; `LOCAL_INSTANCE_ID` if using internal buffer;
        /// pointer to external object otherwise.
        T* m_pExtern = nullptr;
        /// Returns an (uninitialized) internal buffer for storing T.
        static T& GetBuffer() noexcept
        {
            // Static, uninitialized buffer for the singleton's object
            static union U { T asT; U(){} ~U(){} } buffer;
            return buffer.asT;
        }
    } g_instance;

    /// Holds a flag used for `std::call_once()`.
    static struct OnceFlag final
    {
        OnceFlag() noexcept
        {
            new (&buffer.asOnceFlag) std::once_flag();
        }
        OnceFlag(const OnceFlag&) = delete;
        ~OnceFlag()
        {
            buffer.asOnceFlag.~once_flag();
        }
        OnceFlag& operator =(const OnceFlag&) = delete;
        /// Resets the OnceFlag to initial state (allowing another call)
        void Reset() noexcept
        {
            this->~OnceFlag();
            new (this) OnceFlag();
        }
        /// Returns the internal `std::once_flag`. May be uninitialized.
        operator std::once_flag& () noexcept
        {
            return buffer.asOnceFlag;
        }
    private:
        union U { std::once_flag asOnceFlag; U(){} ~U(){} } buffer;
    } g_onceFlag;

    /// Internal function, use the `::testing::SingletonTestApi<T>::Reconstruct()` function instead.
    template <typename ...Args>
    static T& Reset(Args&&... args)
    {
        g_onceFlag.Reset();
        return Get(std::forward<Args>(args)...);
    }

    /// Internal function, use the `::testing::SingletonTestApi<T>::Inject()` function instead.
    static void Inject(T* object)
    {
        if (object)
            std::call_once(g_onceFlag, []() {});
        else
            g_onceFlag.Reset();
        g_instance.SetExtern(object);
    }

    /// Internal helper for emplacing with perfect forwarding through std::call_once.
    /** @remark This avoids lambda capture issues with parameter packs in GCC 4.7-4.8.
      */
    template <typename ...Args>
    static void EmplaceHelper(Args&&... args)
    {
        g_instance.Emplace(std::forward<Args>(args)...);
    }

    // The testing interface can access testing-only functions.
    friend struct testing::SingletonTestApi<T>;
};
template <typename T>
typename Singleton<T>::Instance Singleton<T>::g_instance;
template <typename T>
typename Singleton<T>::OnceFlag Singleton<T>::g_onceFlag;
template <typename T>
T* const Singleton<T>::Instance::LOCAL_INSTANCE_ID = reinterpret_cast<T*>(1);

#endif
