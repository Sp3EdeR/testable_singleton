#ifndef TESTABLE_SINGLETON_INCLUDED_H
#define TESTABLE_SINGLETON_INCLUDED_H

#include <atomic>
#include <mutex>
#include <type_traits>
#include <utility>

#ifndef ENABLE_LOAD_TIME_SINGLETON
#if defined(__GNUC__)    || \
    defined(__clang__)   || \
    defined(__MINGW32__) || \
    defined(__MINGW64__) || \
    defined(__CYGWIN__)  || \
    defined(_MSC_VER)
#define ENABLE_LOAD_TIME_SINGLETON 1
#else
#define ENABLE_LOAD_TIME_SINGLETON 0
#endif
#endif // ENABLE_LOAD_TIME_SINGLETON

/**
 * @brief Types of singletons
 */
enum class SingletonType : int32_t {
    /**
     * @brief Default case, 99% of the time this is enough
     */
    STATIC
#if ENABLE_LOAD_TIME_SINGLETON
    ,
    /**
     * @brief Load-time singleton: use for singleton that must outlive static destruction phase.
     *
     * @note
     * Static instances don't always play nice with process termination:
     * Problem with atexit handlers:
     * - When a static instance is created (e.g. meyers singleton at first use) after
     *   the atexit handler is registered, then by the time the atexit handler runs,
     *   this static instance has already been destroyed.
     * - This is not a problem if the static instance is created before the atexit handler
     *   is registered, but when your library is used by some third party or customer processes,
     *   you cannot rely on this.
     * So a singleton type created with this LOAD_TIME parameter provides an alternative:
     * - The instance is created at first use as usual.
     * - However, its destruction is deferred until "unload time", when your library is unloaded,
     *   or if linked statically during process termination, but after static destruction phase.
     */
    LOAD_TIME
#endif // ENABLE_LOAD_TIME_SINGLETON
};

// Specialiaze some details of the singleton class in "private" Detail namespace
namespace Detail {

template <typename T, SingletonType instanceType>
struct SingletonInstance;

// Static lifetime singleton specialization (default)
template <typename T>
struct SingletonInstance<T, SingletonType::STATIC> {
    SingletonInstance() noexcept = default;
    SingletonInstance(const SingletonInstance&) = delete;
    ~SingletonInstance()
    {
        // Destroys the locally-initialized instance. Injected ones are ignored (no ownership).
        if (m_pExtern == LOCAL_INSTANCE_ID)
            GetBuffer().~T();
    }
    SingletonInstance& operator=(const SingletonInstance&) = delete;
    /// Returns the current instance.
    operator T*() { return m_pExtern == LOCAL_INSTANCE_ID ? &GetBuffer() : m_pExtern; }
    /// Constructs the singleton within the local buffer.
    template <typename... Args>
    void Emplace(Args&&... args)
    {
        this->~SingletonInstance();
        new (&GetBuffer()) T(std::forward<Args>(args)...);
        m_pExtern = LOCAL_INSTANCE_ID;
    }
    /// Sets an external object as the instance.
    /** @remark If `ptr` is `nullptr`, this is just reset.
     */
    void SetExtern(T* ptr)
    {
        this->~SingletonInstance();
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
};

template <typename T>
T* const SingletonInstance<T, SingletonType::STATIC>::LOCAL_INSTANCE_ID = reinterpret_cast<T*>(1);

#if ENABLE_LOAD_TIME_SINGLETON
// Load time singleton impl:
// Some details:
// This is platform specific:
// The instance is created at first use as usual, but not as a static variable
// but on the heap. Its deleter function is saved to a global registry, in reverse order of creation.
// With the help of GCC's attribute((destructor)) (Clang compatible), the destroy functions
// are called at "unload" time.

using DeleteLoadTimeSingletonFunc = void (*)();
struct LoadTimeSingletonEntry {
    bool m_isValid; // To delete or not (to avoid double free between Reset and __attribute__(destructor))
    DeleteLoadTimeSingletonFunc m_deleteFunc;
    LoadTimeSingletonEntry* m_next;
};

// global stack for load time singletons dtors
inline std::atomic<LoadTimeSingletonEntry*>& LoadTimeSingletonEntryStack()
{
    static std::atomic<LoadTimeSingletonEntry*> stack { nullptr };
    return stack;
}

// - Register load time singletons for destruction at "unload" time.
// - The order of destruction is the reverse of the order of registration.
// - The input parameter must point to a static instance.
inline void RegisterLoadTimeSingleton(LoadTimeSingletonEntry* entry)
{
    // - push to head
    // from cppreference example:
    // make new entry the new head, but if the head
    // is no longer what's stored in new entry->m_next
    // (some other thread must have inserted am entry just now)
    // then put that new head into new entry->m_next and try again
    entry->m_next = LoadTimeSingletonEntryStack().load(std::memory_order_relaxed);
    while (!LoadTimeSingletonEntryStack().compare_exchange_weak(
        entry->m_next, entry, std::memory_order_release, std::memory_order_relaxed))
        ;
}

// - Platform independent part: delete in reverse order
inline void DestroyLoadTimeSingletons()
{
    // memory safety is no concern here
    for (auto entry = LoadTimeSingletonEntryStack().load(); entry != nullptr; entry = entry->m_next) {
        if (entry->m_isValid)
            entry->m_deleteFunc();
    }
}

#if defined(_MSC_VER)
// MSVC: Use .CRT$XPU section for late destruction
typedef void (__cdecl *_PVFV)(void);
inline void __cdecl DeinitializeLoadTimeSingletons()
{
    DestroyLoadTimeSingletons();
}
#pragma section(".CRT$XPU", long, read)
__declspec(allocate(".CRT$XPU")) _PVFV p_deinit = DeinitializeLoadTimeSingletons;
#else
// GCC/Clang/CYGWIN/MINGW: Use __attribute__((destructor))
inline void __attribute__((destructor)) DeinitializeLoadTimeSingletons()
{
    DestroyLoadTimeSingletons();
}
#endif

// Load-time singleton specialization
// API is compatible with static singleton specialization
// however it has no custom dtor (as that would violate trivial destructibility)
template <typename T>
struct SingletonInstance<T, SingletonType::LOAD_TIME> {
    /// Returns the current instance.
    operator T*() { return m_pExtern == LOCAL_INSTANCE_ID ? g_pInternal : m_pExtern; }
    /// Constructs the singleton on the heap, and pushes its deleter to the unload-time stack.
    template <typename... Args>
    void Emplace(Args&&... args)
    {
        Reset();
        CreateInternalInstance(std::forward<Args>(args)...);
        RegisterLoadTimeSingleton(&g_entry);
        m_pExtern = LOCAL_INSTANCE_ID;
    }
    /// Sets an external object as the instance.
    /** @remark If `ptr` is `nullptr`, this is just reset.
     */
    void SetExtern(T* ptr)
    {
        Reset();
        m_pExtern = ptr;
    }

private:
    // Swapped the dtor on static instance for a Reset function
    void Reset()
    {
         // Destroys the locally-initialized instance. Injected ones are ignored (no ownership).
        if (m_pExtern == LOCAL_INSTANCE_ID)
            DestroyInternalInstance();
    }
    // Manage internal instance on heap:
    template <typename... Args>
    static void CreateInternalInstance(Args&&... args)
    {
        g_pInternal = new T(std::forward<Args>(args)...);
        g_entry.m_isValid = true;
    }
    static void DestroyInternalInstance()
    {
        delete std::exchange(g_pInternal, nullptr);
        g_entry.m_isValid = false;
    }

    /// A special pointer value to specify that the local buffer is constructed.
    static T* const LOCAL_INSTANCE_ID;
    /// nullptr if empty; LOCAL_INSTANCE_ID if using internal buffer;
    /// pointer to external object otherwise.
    T* m_pExtern = nullptr;
    /// used instead of static buffer
    static T* g_pInternal;
    /// used for deleting this singleton
    static LoadTimeSingletonEntry g_entry;
};

template <typename T>
T* const SingletonInstance<T, SingletonType::LOAD_TIME>::LOCAL_INSTANCE_ID = reinterpret_cast<T*>(1);
template <typename T>
T* SingletonInstance<T, SingletonType::LOAD_TIME>::g_pInternal = nullptr;
template <typename T>
LoadTimeSingletonEntry SingletonInstance<T, SingletonType::LOAD_TIME>::g_entry
    = { false, SingletonInstance<T, SingletonType::LOAD_TIME>::DestroyInternalInstance, nullptr };
#endif // ENABLE_LOAD_TIME_SINGLETON

} // namespace Detail

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
template <typename T, SingletonType type = SingletonType::STATIC>
struct Singleton
{
    using BaseType = Singleton<T, type>;

    /// Returns the instance of the class.
    /** @remark The constructor arguments are only used if the instance is not constructed yet.
      */
    template <typename ...Args>
    static T& Get(Args&&... args)
    {
        std::call_once(g_onceFlag, [](Args&&... args) {
                g_instance.Emplace(std::forward<Args>(args)...);
            }, std::forward<Args>(args)...);
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
    using Instance = Detail::SingletonInstance<T, type>;
#if ENABLE_LOAD_TIME_SINGLETON
    // note to maintainer: Load time singleton instance must be trivially destructible,
    // otherwise it (or some of its members) will get destroyed during static destruction
    // phase before "unload" time.
    // this remains here to catch future regressions
    static_assert(type != SingletonType::LOAD_TIME || std::is_trivially_destructible_v<Instance>,
        "Load-time singletons must be trivially destructible");
#endif // ENABLE_LOAD_TIME_SINGLETON

    Singleton(const Singleton&) = delete;
    Singleton& operator =(const Singleton&) = delete;

    static Instance g_instance;

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
    static T& Reset(Args&&... args)
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
template <typename T, SingletonType type>
typename Singleton<T, type>::OnceFlag Singleton<T, type>::g_onceFlag;
template <typename T, SingletonType type>
typename Singleton<T, type>::Instance Singleton<T, type>::g_instance;

#endif
