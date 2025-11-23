#ifndef TESTABLE_SINGLETON_INCLUDED_H
#define TESTABLE_SINGLETON_INCLUDED_H

#include <atomic>
#include <mutex>
#include <type_traits>
#include <utility>

/// This file implements the singleton pattern, but makes unit testing easy.

// Types of singletons:
enum class SingletonType : int32_t {
    // default case, 99% of the time this is enough
    STATIC,
    // Some notes:
    // Static instances don't always play nice with process termination:
    // Problem with atexit handlers:
    // - When a static instance is created (e.g. meyers singleton at first use) after
    //   the atexit handler is registered, then by the time the atexit handler runs,
    //   this static instance has already been destroyed.
    // - This is not a problem if the static instance is created before the atexit handler
    //   is registered, but when your library is used by some third party or customer processes,
    //   you cannot rely on this.
    // So a singleton type created with this LOAD_TIME parameter provides an alternative:
    // - The instance is created at first use as usual.
    // - However, its destruction is deferred until "unload time", when your library is unloaded,
    //   or if linked statically during process termination, but after static destruction phase.
    LOAD_TIME
};

// fw declare some dependencies
namespace Detail {
template <typename T, SingletonType instanceType> struct SingletonInstance;
}

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
template <typename T, SingletonType type = SingletonType::STATIC> struct Singleton {
    using BaseType = Singleton<T, type>;

    /// Returns the instance of the class.
    /** @remark The constructor arguments are only used if the instance is not constructed yet.
     */
    template <typename... Args> static T& Get(Args&&... args);

    ///  Returns the instance of the class without construction.
    /** @return The instance of the singleton or a `nullptr` if unconstructed.
     * @remark This can be useful for singletons that have custom constructor arguments. Code can
     *         get an already initialied instance, if it has been initialized already, without
     *         having to specify the arguments.
     */
    static T* TryGet();

protected:
    Singleton() noexcept = default;

private:
    using Instance = Detail::SingletonInstance<T, type>;
    // just here to catch future regressions
    static_assert(type != SingletonType::LOAD_TIME || std::is_trivially_destructible_v<Instance>,
        "Load-time singletons must be trivially destructible");

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static Instance g_instance;

    /// Holds a flag used for `std::call_once()`.
    struct OnceFlag final {
        OnceFlag() noexcept;
        OnceFlag(const OnceFlag&) = delete;
        ~OnceFlag();
        OnceFlag& operator=(const OnceFlag&) = delete;
        /// Resets the OnceFlag to initial state (allowing another call)
        void Reset();
        /// Returns the internal std::once_flag. May be uninitialized.
        operator std::once_flag&();

    private:
        union U {
            std::once_flag asOnceFlag;
            U() { }
            ~U() { }
        } buffer;
    };

    static OnceFlag g_onceFlag;

    /// (Re)constructs the internal singleton instance.
    /** If an existing singleton instance was already constructed, it is destroyed. If an external
     * instance was injected, it is overridden with the newly constructed instance.
     *
     * @remark This function is not thread safe. It is intended for tests, not production code.
     */
    template <typename... Args> static T& Reset(Args&&... args);

    /// Injects an external instance into the singleton.
    /** If an external instance is injected, the `Get()` function
     * @param object The object is taken without ownership and must be deleted by the caller.
     * @remark If `object` is `nullptr`, it resets the singleton to uninitialized state, and the
     *         next invocation of `Get()` reconstructs the instance.
     */
    static void Inject(T* object);
};

// Specialiaze some details of the singleton class in "private" Detail namespace
namespace Detail {

// Static lifetime singleton specialization (default)
template <typename T> struct SingletonInstance<T, SingletonType::STATIC> {
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
    template <typename... Args> void Emplace(Args&&... args)
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
        static union U {
            T asT;
            U() { }
            ~U() { }
        } buffer;
        return buffer.asT;
    }
};

template <typename T>
T* const SingletonInstance<T, SingletonType::STATIC>::LOCAL_INSTANCE_ID = reinterpret_cast<T*>(0x1);

// Load time singleton impl:
// Some details:
// The instance is created at first use as usual, but not as a static variable
// but on the heap. Its deleter function is saved to a global registry, in reverse order of creation.
// With the help of GCC's attribute((destructor)) (Clang compatible), the destroy functions
// are called at "unload" time.

using DeleteLoadTimeSingletonFunc = void (*)();

struct LoadTimeSingletonEntry {
    DeleteLoadTimeSingletonFunc m_deleteFunc;
    LoadTimeSingletonEntry* m_next;
};
// note to maintainer: all types used by load time singletons must be trivially destructible,
// otherwise they will get destroyed during static destruction phase before "unload" time.
static_assert(std::is_trivially_destructible_v<LoadTimeSingletonEntry>);

// global stack for load time singletons dtors
inline std::atomic<LoadTimeSingletonEntry*>& LoadTimeSingletonEntryStack()
{
    static_assert(std::is_trivially_destructible_v<std::atomic<LoadTimeSingletonEntry*>>);
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
// attribute destructor to make sure its run after static destruction phase
inline void __attribute__((destructor)) DestroyLoadTimeSingletons()
{
    // memory safety is no concern here
    for (auto entry = LoadTimeSingletonEntryStack().load(); entry != nullptr; entry = entry->m_next) {
        entry->m_deleteFunc();
    }
}

// Load-time singleton specialization
// API is compatible with static singleton specialization
// however it has no custom dtor (as that would violate trivial destructibility)
template <typename T> struct SingletonInstance<T, SingletonType::LOAD_TIME> {
    operator T*() { return m_pExtern == LOCAL_INSTANCE_ID ? g_pInternal : m_pExtern; }
    template <typename... Args> void Emplace(Args&&... args)
    {
        Reset();
        CreateInternalInstance(std::forward<Args>(args)...);
        RegisterLoadTimeSingleton(&g_entry);
        m_pExtern = LOCAL_INSTANCE_ID;
    }
    void SetExtern(T* ptr)
    {
        Reset();
        m_pExtern = ptr;
    }

private:
    // Swapped the dtor on static instance for a Reset function
    void Reset()
    {
        if (m_pExtern == LOCAL_INSTANCE_ID)
            DestroyInternalInstance();
    }
    // Manage internal instance on heap:
    template <typename... Args> static void CreateInternalInstance(Args&&... args)
    {
        g_pInternal = new T(std::forward<Args>(args)...);
    }
    static void DestroyInternalInstance() { delete std::exchange(g_pInternal, nullptr); }

    static T* const LOCAL_INSTANCE_ID;
    T* m_pExtern = nullptr;
    static T* g_pInternal;
    static LoadTimeSingletonEntry g_entry;
    // we know scalar types are trivially destructible, but add check for intent
    static_assert(std::is_trivially_destructible_v<T*>);
};

template <typename T>
T* const SingletonInstance<T, SingletonType::LOAD_TIME>::LOCAL_INSTANCE_ID = reinterpret_cast<T*>(0x1);
template <typename T> T* SingletonInstance<T, SingletonType::LOAD_TIME>::g_pInternal = nullptr;
template <typename T>
LoadTimeSingletonEntry SingletonInstance<T, SingletonType::LOAD_TIME>::g_entry
    = { SingletonInstance<T, SingletonType::LOAD_TIME>::DestroyInternalInstance, nullptr };

} // namespace Detail

// Implementation of Singleton class

template <typename T, SingletonType type> template <typename... Args> T& Singleton<T, type>::Get(Args&&... args)
{
    std::call_once(
        g_onceFlag, [](Args&&... args) { g_instance.Emplace(std::forward<Args>(args)...); },
        std::forward<Args>(args)...);
    return *static_cast<T*>(g_instance);
}
template <typename T, SingletonType type> T* Singleton<T, type>::TryGet()
{
    return g_instance;
}
template <typename T, SingletonType type> template <typename... Args> T& Singleton<T, type>::Reset(Args&&... args)
{
    g_onceFlag.Reset();
    return Get(std::forward<Args>(args)...);
}
template <typename T, SingletonType type> void Singleton<T, type>::Inject(T* object)
{
    if (object)
        std::call_once(g_onceFlag, []() {});
    else
        g_onceFlag.Reset();
    g_instance.SetExtern(object);
}
template <typename T, SingletonType type> Singleton<T, type>::OnceFlag::OnceFlag() noexcept
{
    new (&buffer.asOnceFlag) std::once_flag();
}
template <typename T, SingletonType type> Singleton<T, type>::OnceFlag::~OnceFlag()
{
    buffer.asOnceFlag.~once_flag();
}
template <typename T, SingletonType type> void Singleton<T, type>::OnceFlag::Reset()
{
    this->~OnceFlag();
    new (this) OnceFlag();
}
template <typename T, SingletonType type> Singleton<T, type>::OnceFlag::operator std::once_flag&()
{
    return buffer.asOnceFlag;
}

template <typename T, SingletonType type> typename Singleton<T, type>::OnceFlag Singleton<T, type>::g_onceFlag;
template <typename T, SingletonType type> typename Singleton<T, type>::Instance Singleton<T, type>::g_instance;

#endif // TESTABLE_SINGLETON_INCLUDED_H
