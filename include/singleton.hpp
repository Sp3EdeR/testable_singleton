#ifndef TESTABLE_SINGLETON_INCLUDED_H
#define TESTABLE_SINGLETON_INCLUDED_H

#include <mutex>
#include <utility>

/// Implements the singleton pattern, but makes unit testing easy.
/** To use the Singleton class normally, inherit from it with the CRTP style, like:
  *
  * ```cpp
  * class MyClass : public Singleton<MyClass> { impl... };
  * ```
  *
  * After this, external code is able to get the `MyClass` instance by using `MyClass::Get()`.
  *
  * To test your singleton, an access-private library implementation is required, such as
  * <https://github.com/martong/access_private>. Using the access-private library, invoke the
  * `MyClass::Inject()` function to inject a mock implementation into the singleton. It is also
  * possible to reconstruct the Singleton with different constructor arguments by using the
  * `MyClass::Reset()` function.
  * 
  * @remark The `Inject()` and `Reset()` functions are NOT thread safe! They should only be used
  *         in test code sections while implementation code is not running on a different thread.
  */
template <typename T>
struct Singleton
{
    using BaseType = Singleton<T>;

    /// Returns the instance of the class.
    /** @remark The constructor arguments are only used if the instance is not constructed yet.
      */
    template <typename ...Args>
    static T& Get(Args... args)
    {
        std::call_once(g_onceFlag, [](Args... args) {
                g_instance.Emplace(std::forward<Args>(args)...);
            }, args...);
        return *static_cast<T*>(g_instance);
    }

    ///  Returns the instance of the class without construction.
    /** @return The instance of the singleton or a `nullptr` if unconstructed.
      * @remark This can be useful for singletons that have custom constructor arguments. Code can
      *         get an already initialied instance, if it has been initialized already, without
      *         having to specify the arguments.
      */
    static T* TryGet()
    {
        return g_instance;
    }

protected:
    Singleton() noexcept = default;
private:
    Singleton(const Singleton&) = delete;
    Singleton& operator =(const Singleton&) = delete;

    /// Holds the instance of T, either locally constructed or injected.
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
        operator T* () { return m_pExtern == LOCAL_INSTANCE_ID ? &GetBuffer() : m_pExtern; }
        /// Constructs the singleton within the local buffer.
        template <typename ...Args>
        void Emplace(Args... args)
        {
            this->~Instance();
            new (&GetBuffer()) T(std::forward<Args>(args)...);
            m_pExtern = LOCAL_INSTANCE_ID;
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
        /// nullptr if empty; LOCAL_INSTANCE_ID if using internal buffer;
        /// pointer to external object otherwise.
        T* m_pExtern = nullptr;
        /// Returns an (uninitialized) internal buffer for storing T.
        static T& GetBuffer()
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
        void Reset()
        {
            this->~OnceFlag();
            new (this) OnceFlag();
        }
        /// Returns the internal std::once_flag. May be uninitialized.
        operator std::once_flag& ()
        {
            return buffer.asOnceFlag;
        }
    private:
        union U { std::once_flag asOnceFlag; U(){} ~U(){} } buffer;
    } g_onceFlag;

    /// (Re)constructs the internal singleton instance.
    /** If an existing singleton instance was already constructed, it is destroyed. If an external
      * instance was injected, it is overridden with the newly constructed instance.
      * 
      * @remark This function is not thread safe. It is intended for tests, not production code.
      */
    template <typename ...Args>
    static T& Reset(Args... args)
    {
        g_onceFlag.Reset();
        return Get(std::forward<Args>(args)...);
    }

    /// Injects an external instance into the singleton.
    /** If an external instance is injected, the `Get()` function 
      * @param object The object is taken without ownership and must be deleted by the caller.
      * @remark If `object` is `nullptr`, it resets the singleton to uninitialized state, and the
      *         next invocation of `Get()` reconstructs the instance.
      */
    static void Inject(T* object)
    {
        if (object)
            std::call_once(g_onceFlag, []() {});
        else
            g_onceFlag.Reset();
        g_instance.SetExtern(object);
    }
};
template <typename T>
typename Singleton<T>::Instance Singleton<T>::g_instance;
template <typename T>
typename Singleton<T>::OnceFlag Singleton<T>::g_onceFlag;
template <typename T>
T* const Singleton<T>::Instance::LOCAL_INSTANCE_ID = reinterpret_cast<T*>(1);

#endif
