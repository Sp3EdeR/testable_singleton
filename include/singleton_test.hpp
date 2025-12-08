#ifndef TESTABLE_SINGLETON_TEST_INCLUDED_H
#define TESTABLE_SINGLETON_TEST_INCLUDED_H

/** @file
  * @brief Provides testing functions for Singleton<T>.
  * 
  * @warning DO NOT USE THIS FILE IN PRODUCTION CODE!  
  * This interface bypasses the singleton's encapsulation and exposes functions intended for
  * unit testing only.
  *
  * See `singleton_test.cpp` for an example of using this interface.
  */

#include "singleton.hpp"

namespace testing
{
    /// Provides access to Singleton's testing functions.
    /** @warning DO NOT USE THIS IN PRODUCTION CODE!  
      * This interface bypasses the singleton's encapsulation and exposes functions intended for
      * unit testing only.
      * 
      * Invoke the `Inject()` method to inject a mock implementation into the singleton. It is also
      * possible to reconstruct the Singleton with different constructor arguments by using the
      * `Reconstruct()` method.
      * 
      * Usage:
      * 
      * ```cpp
      * testing::SingletonTestApi<MySingletonType>::Reconstruct(constructorArg1, constructorArg2);
      * testing::SingletonTestApi<MySingletonType>::Inject(&myMockInstance);
      * ```
      *
      * @remark This class's methods are NOT thread safe! They should only be used in test code
      *         sections while implementation code is not running on a different thread.
      */
    template <typename T>
    struct SingletonTestApi
    {
        /// (Re)constructs the internal singleton instance.
        /** If an existing singleton instance was already constructed, it is destroyed. If an
          * external instance was injected, it is overridden with the newly constructed internal
          * instance.
          * @remark This function is not thread safe. It is intended for tests, not production
          *         code. Do not call this function twice, or any of the same singleton's getter
          *         functions at the same time.
          */
        template <typename ...Args>
        static T& Reconstruct(Args&&... args)
        {
            return SingletonType::Reset(std::forward<Args>(args)...);
        }

        /// This is an alias for `Reconstruct()`.
        /** It is recommended to use `Reconstruct()`, because it is more descriptive.
          */
        template <typename ...Args>
        static T& Reset(Args&&... args)
        {
            return SingletonType::Reset(std::forward<Args>(args)...);
        }

        /// Injects an external instance into the singleton.
        /** The injected class instance is returned on the upcoming calls to the `Singleton<T>::Get()`
          * function.
          *
          * If an existing singleton instance was already constructed, it is destroyed.
          *
          * If `object` is `nullptr`, it resets the singleton to uninitialized state, and the
          *         next invocation of `Get()` reconstructs the instance.
          * @param object The object is taken without ownership and must be deleted by the caller.
          * @remark This function is not thread safe. It is intended for tests, not production
          *         code. Do not call this function twice, or any of the same singleton's getter
          *         functions at the same time.
          */
        static void Inject(T* object)
        {
            SingletonType::Inject(object);
        }

        /// Resets the singleton to uninitialized state.
        /** This is an alias for `Inject()` with a `nullptr` argument.
          * @remark This function is not thread safe. It is intended for tests, not production
          *         code. Do not call this function twice, or any of the same singleton's getter
          *         functions at the same time.
          */
        static void Clear()
        {
            SingletonType::Inject(nullptr);
        }
    private:
        // The singleton's type for which this class exposes testing functions.
        using SingletonType = Singleton<T>;

        SingletonTestApi() = delete;
    };
}

#endif
