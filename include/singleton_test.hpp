#ifndef TESTABLE_SINGLETON_TEST_INCLUDED_H
#define TESTABLE_SINGLETON_TEST_INCLUDED_H

/** @file
  * @brief Provides testing functions for Singleton<T>.
  * 
  * This file should only be included in unit tests. The unit test code should be added to the
  * `::testing` namespace, from which the provided interfaces are directly usable.
  */

#include "singleton.hpp"

namespace testing
{
    /// Provides access to Singleton's testing functions.
    /** @warning DO NOT USE THIS IN PRODUCTION CODE
      * This interface bypasses the singleton's encapsulation and exposes functions intended for unit
      * testing only.
      * 
      * Usage:
      * 
      * ```cpp
      * testing::SingletonTestApi<MySingletonType>::Reset(constructorArg1, constructorArg2);
      * testing::SingletonTestApi<MySingletonType>::Inject(&myMockInstance);
      * ```
      */
    template <typename T>
    struct SingletonTestApi
    {
        /// (Re)constructs the internal singleton instance.
        /** If an existing singleton instance was already constructed, it is destroyed. If an external
          * instance was injected, it is overridden with the newly constructed instance.
          *
          * @remark This function is not thread safe. It is intended for tests, not production code.
          */
        template <typename ...Args>
        static T& Reconstruct(Args&&... args)
        {
            return SingletonType::Reset(std::forward<Args>(args)...);
        }

        /// @overload
        template <typename ...Args>
        static T& Reset(Args&&... args)
        {
            return SingletonType::Reset(std::forward<Args>(args)...);
        }

        /// Injects an external instance into the singleton.
        /** If an external instance is injected, the `Get()` function
          * @param object The object is taken without ownership and must be deleted by the caller.
          * @remark If `object` is `nullptr`, it resets the singleton to uninitialized state, and the
          *         next invocation of `Get()` reconstructs the instance.
          */
        static void Inject(T* object)
        {
            SingletonType::Inject(object);
        }

		/// Resets the singleton to uninitialized state.
        /** @remark This is an alias for `Inject(nullptr)`.
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
