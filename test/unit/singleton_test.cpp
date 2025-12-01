#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../include/singleton.hpp"
#include "../../include/singleton_test.hpp"

namespace testing
{
namespace
{

/////////////
// Test Cases

// Scenario: Test the basic behaviour of a simple singleton initialization with default ctor.

struct SimpleSingleton : Singleton<SimpleSingleton>
{
    static constexpr const char* INITIAL_VALUE = "InitialValue";

    const char* m_value = INITIAL_VALUE;

    const char* GetValue() const { return m_value; }
};
constexpr const char* SimpleSingleton::INITIAL_VALUE;

TEST(BasicSingletonTest, DefaultCtor)
{
    auto& inst = SimpleSingleton::Get();

    EXPECT_EQ(inst.m_value, SimpleSingleton::INITIAL_VALUE);
    EXPECT_EQ(inst.GetValue(), SimpleSingleton::INITIAL_VALUE);
}

// Scenario: Test the basic behaviour of a singleton with a non-default ctor, and reset it after
// use.

struct CustomCtorSingleton : Singleton<CustomCtorSingleton>
{
    static constexpr double INITIAL_VALUE = 123456789.01234;

    const char* m_value1 = nullptr;
    int m_value2 = 0;
    double m_value3 = INITIAL_VALUE;

    const char* GetValue1() const { return m_value1; }
    int GetValue2() const { return m_value2; }
    double GetValue3() const { return m_value3; }
protected:
    CustomCtorSingleton(const char* arg1, int arg2)
        : m_value1(arg1), m_value2(arg2)
    { }
    friend BaseType;
};
constexpr double CustomCtorSingleton::INITIAL_VALUE;

TEST(BasicSingletonTest, CustomCtorAndReset)
{
    // First instantiation with test arguments
    auto& inst1 = CustomCtorSingleton::Get("TestValue", 123456789);

    EXPECT_STREQ(inst1.m_value1, "TestValue");
    EXPECT_STREQ(inst1.GetValue1(), "TestValue");
    EXPECT_EQ(inst1.m_value2, 123456789);
    EXPECT_EQ(inst1.GetValue2(), 123456789);
    EXPECT_EQ(inst1.m_value3, CustomCtorSingleton::INITIAL_VALUE);
    EXPECT_EQ(inst1.GetValue3(), CustomCtorSingleton::INITIAL_VALUE);

    // Re-instantiation with different test arguments
    auto& inst2 = SingletonTestApi<CustomCtorSingleton>::Reset("DifferentValue", 0123);

    EXPECT_STREQ(inst2.m_value1, "DifferentValue");
    EXPECT_STREQ(inst2.GetValue1(), "DifferentValue");
    EXPECT_EQ(inst2.m_value2, 0123);
    EXPECT_EQ(inst2.GetValue2(), 0123);
    EXPECT_EQ(inst2.m_value3, CustomCtorSingleton::INITIAL_VALUE);
    EXPECT_EQ(inst2.GetValue3(), CustomCtorSingleton::INITIAL_VALUE);
}

// Scenario: Singleton with constructor that has copy-only and move-only parameters.

struct CopyOnlyType
{
    CopyOnlyType() = default;
    CopyOnlyType(const CopyOnlyType&) = default;
    CopyOnlyType& operator =(const CopyOnlyType&) = default;
};

struct MoveOnlyType
{
    MoveOnlyType() = default;
    MoveOnlyType(const MoveOnlyType&) = delete;
    MoveOnlyType(MoveOnlyType&&) = default;
    MoveOnlyType& operator =(const MoveOnlyType&) = delete;
    MoveOnlyType& operator =(MoveOnlyType&&) = default;
};

struct SpecialParamCtorSingleton : Singleton<SpecialParamCtorSingleton>
{
protected:
    SpecialParamCtorSingleton(CopyOnlyType arg1, MoveOnlyType&& arg2) { }
    friend BaseType;
};

TEST(BasicSingletonTest, SpecialParamCtorAndReset)
{
    // First instantiation with test arguments
    auto& inst1 = SpecialParamCtorSingleton::Get(CopyOnlyType(), MoveOnlyType());

    // Re-instantiation with different test arguments
    auto& inst2 = SingletonTestApi<SpecialParamCtorSingleton>::Reset(CopyOnlyType(), MoveOnlyType());
}

// Scenario: The `TryGet()` function returns `nullptr` for an uninitialized singleton.

template <int testCaseNum>
struct EmptySingleton : Singleton<EmptySingleton<testCaseNum>>
{
};

TEST(BasicSingletonTest, UninitializedTryGet)
{
    auto inst = EmptySingleton<1>::TryGet();

    EXPECT_EQ(inst, nullptr);
}

// Scenario: The `TryGet()` function returns a pointer to the instance of an initialized singleton.

TEST(BasicSingletonTest, InitializedTryGet)
{
    // Initialize the singleton
    auto& inst1 = EmptySingleton<2>::Get();

    auto inst2 = EmptySingleton<2>::TryGet();

    EXPECT_EQ(&inst1, inst2);
}

// Scenario: The Reset function does not change the address of the object.

using EmptySingleton2 = EmptySingleton<2>;

TEST(BasicSingletonTest, ResetKeepsStableAddress)
{
    auto inst1 = &EmptySingleton2::Get();
    auto inst2 = &SingletonTestApi<EmptySingleton2>::Reset();
    auto inst3 = &EmptySingleton2::Get();

    EXPECT_EQ(inst1, inst2);
    EXPECT_EQ(inst2, inst3);
}

// Scenario group: Test the injection of a mocked instance into a singleton.

template <int testCaseNum>
struct MockableSingleton : Singleton<MockableSingleton<testCaseNum>>
{
    virtual bool Overridden() { return false; }
};

template <int testCaseNum>
struct MockMockableSingleton : MockableSingleton<testCaseNum>
{
    MOCK_METHOD(bool, Overridden, (), (override));
};

// Scenario: Test injecting a mocked instance into an already initialized singleton.

using MockableSingleton1 = MockableSingleton<1>;

TEST(BasicSingletonTest, InjectMockAfterInstantiation)
{
    // Initialize the singleton
    EXPECT_FALSE(MockableSingleton1::Get().Overridden());

    StrictMock<MockMockableSingleton<1>> mock;
    EXPECT_CALL(mock, Overridden).Times(1).WillOnce(Return(true));

    // Inject the mock to replace the original singleton (and destroy it)
    SingletonTestApi<MockableSingleton1>::Inject(&mock);

    EXPECT_TRUE(MockableSingleton1::Get().Overridden());
}

// Scenario: Test injecting a mocked instance into an uninitialized singleton.

using MockableSingleton2 = MockableSingleton<2>;

TEST(BasicSingletonTest, InjectMockBeforeInstantiation)
{
    StrictMock<MockMockableSingleton<2>> mock;
    EXPECT_CALL(mock, Overridden).Times(1).WillOnce(Return(true));

    SingletonTestApi<MockableSingleton2>::Inject(&mock);

    EXPECT_TRUE(MockableSingleton2::Get().Overridden());
}

// Scenario group: Singleton-allocated objects are created and destroyed on Reset and Inject.

// A prototype that can capture construction and destruction events.
struct CtorVerifier
{
    virtual void Constructed(void*) {}
    virtual void Destructed(void*) {}
};

// A singleton type that takes a mock instance and calls its `Constructed` and `Destructed`
// functions on construction and destruction.
template <CtorVerifier& (*GetMock)()>
struct CtorMockedSingleton : Singleton<CtorMockedSingleton<GetMock>>
{
    enum FLAGS : unsigned
    {
        CALL_MOCKS = 1,
    };

    CtorMockedSingleton(unsigned flags = CALL_MOCKS)
        : m_flags(flags)
    {
        if (m_flags & CALL_MOCKS)
            GetMock().Constructed(this);
    }

    ~CtorMockedSingleton()
    {
        if (m_flags & CALL_MOCKS)
            GetMock().Destructed(this);
    }

    unsigned m_flags;
};

// A test fixture that sets up a mock while running the test, but replaces it with a fake when the
// test is not running. Singletons survive until the end of the program, so a destruction happens
// outside of the test's scope. This should be swallowed by the fake.
template <int testCaseNum>
struct CtorSingletonTest : Test
{
    CtorSingletonTest()
    {
        g_pMock = &m_mock;
    }
    ~CtorSingletonTest()
    {
        g_pMock = nullptr;
    }
protected:
    struct CtorVerifierMock : CtorVerifier
    {
        MOCK_METHOD(void, Constructed, (void* ptr), (override));
        MOCK_METHOD(void, Destructed, (void* ptr), (override));
    } m_mock;

    static CtorVerifierMock* g_pMock;
    static CtorVerifier& GetMock() {
        static CtorVerifier fake;
        return g_pMock ? *g_pMock : fake;
    }
public:
    using SingletonType = CtorMockedSingleton<&GetMock>;
};
template <int testCaseNum>
typename CtorSingletonTest<testCaseNum>::CtorVerifierMock* CtorSingletonTest<testCaseNum>::g_pMock = nullptr;

// Scenario: A singleton instance is constructed on Get(), but not destructed during the test.

using CtorSingletonTest1 = CtorSingletonTest<1>;

TEST_F(CtorSingletonTest1, ConstructionOnGet)
{
    EXPECT_CALL(*g_pMock, Constructed).Times(1);
    EXPECT_CALL(*g_pMock, Destructed).Times(0);

    CtorMockedSingleton<&GetMock>::Get();
}

// Scenario: An initialized singleton is reconstructed as new when Reset()-ing.

using CtorSingletonTest2 = CtorSingletonTest<2>;
using CtorSingletonType2 = CtorSingletonTest2::SingletonType;

TEST_F(CtorSingletonTest2, DestructionOnReset)
{
    void* pSingletonInstance = nullptr;
    Sequence seq;
    EXPECT_CALL(*g_pMock, Constructed(_))
        .Times(1)
        .InSequence(seq)
        .WillOnce(SaveArg<0>(&pSingletonInstance));
    EXPECT_CALL(*g_pMock, Destructed(Eq(ByRef(pSingletonInstance))))
        .Times(1)
        .InSequence(seq);
    EXPECT_CALL(*g_pMock, Constructed(Eq(ByRef(pSingletonInstance))))
        .Times(1)
        .InSequence(seq);

    // Construct the first instance
    auto& instance1 = CtorMockedSingleton<&GetMock>::Get();

    EXPECT_NE(pSingletonInstance, nullptr);
    EXPECT_EQ(pSingletonInstance, &instance1);

    // Destroy the first instance and create the second one (same address)
    auto& instance2 = SingletonTestApi<CtorSingletonType2>::Reset();

    EXPECT_EQ(pSingletonInstance, &instance2);
}

// Scenario: An initialized singleton is destroyed when injecting a mocked instance.

using CtorSingletonTest3 = CtorSingletonTest<3>;
using CtorSingletonType3 = CtorSingletonTest3::SingletonType;

TEST_F(CtorSingletonTest3, DestructionOnInject)
{
    struct MockOfSingleton : CtorSingletonType3
    {
        MockOfSingleton() : CtorSingletonType3(0) {}
    } mock;

    void* pSingletonInstance = nullptr;
    Sequence seq;
    EXPECT_CALL(*g_pMock, Constructed(_))
        .Times(1)
        .InSequence(seq)
        .WillOnce(SaveArg<0>(&pSingletonInstance));
    EXPECT_CALL(*g_pMock, Destructed(Eq(ByRef(pSingletonInstance))))
        .Times(1)
        .InSequence(seq);

    auto& instance1 = CtorMockedSingleton<&GetMock>::Get();

    EXPECT_NE(pSingletonInstance, nullptr);
    EXPECT_EQ(pSingletonInstance, &instance1);

    // Destroy the first instance, use local mock
    SingletonTestApi<CtorSingletonType3>::Inject(&mock);

    auto& instance2 = CtorMockedSingleton<&GetMock>::Get();

    EXPECT_EQ(&instance2, &mock);
    EXPECT_NE(pSingletonInstance, &instance2);
}

// Scenario: When injecting a nullptr mocked instance, the following Get reconstructs the instance.

using CtorSingletonTest4 = CtorSingletonTest<4>;
using CtorSingletonType4 = CtorSingletonTest4::SingletonType;

TEST_F(CtorSingletonTest4, ReconstructionAfterNullInject)
{
    void* pSingletonInstance = nullptr;
    void* pNewSingletonInstance = nullptr;
    Sequence seq;
    EXPECT_CALL(*g_pMock, Constructed(_))
        .Times(1)
        .InSequence(seq)
        .WillOnce(SaveArg<0>(&pSingletonInstance));
    EXPECT_CALL(*g_pMock, Destructed(Eq(ByRef(pSingletonInstance))))
        .Times(1)
        .InSequence(seq);
    EXPECT_CALL(*g_pMock, Constructed(_))
        .Times(1)
        .InSequence(seq)
        .WillOnce(SaveArg<0>(&pNewSingletonInstance));

    auto& instance1 = CtorMockedSingleton<&GetMock>::Get();

    EXPECT_NE(pSingletonInstance, nullptr);
    EXPECT_EQ(pSingletonInstance, &instance1);

    // Destroy the first instance
    SingletonTestApi<CtorSingletonType4>::Inject(nullptr);

    auto& instance2 = CtorMockedSingleton<&GetMock>::Get();

    EXPECT_EQ(pSingletonInstance, pNewSingletonInstance);
    EXPECT_EQ(pNewSingletonInstance, &instance2);
}

// Scenario group: Injected instances are not destroyed on Reset and Inject.

// Scenario: A singleton that has an injected instance does not destroy it on Inject.

using EmptySingleton3 = EmptySingleton<3>;

TEST(BasicSingletonTest, InjectedInstanceNotDestroyedOnInjection)
{
    static bool g_destroyed = false;

    struct MockOfSingleton : EmptySingleton3
    {
        ~MockOfSingleton() {
            g_destroyed = true;
        }
    };

    MockOfSingleton mock1, mock2;

    SingletonTestApi<EmptySingleton3>::Inject(&mock1);
    auto& instance1 = EmptySingleton3::Get();
    
    EXPECT_EQ(&instance1, &mock1);

    SingletonTestApi<EmptySingleton3>::Inject(&mock2);
    auto& instance2 = EmptySingleton3::Get();

    EXPECT_EQ(&instance2, &mock2);

    EXPECT_FALSE(g_destroyed);
}

// Scenario: A singleton that has an injected instance does not destroy it on Reset.

using EmptySingleton4 = EmptySingleton<4>;

TEST(BasicSingletonTest, InjectedInstanceNotDestroyedOnReset)
{
    static bool g_destroyed = false;

    struct MockOfSingleton : EmptySingleton4
    {
        ~MockOfSingleton() {
            g_destroyed = true;
        }
    };

    MockOfSingleton mock1;

    SingletonTestApi<EmptySingleton4>::Inject(&mock1);

    SingletonTestApi<EmptySingleton4>::Inject(&mock1);
    auto& instance1 = EmptySingleton4::Get();

    EXPECT_EQ(&instance1, &mock1);

    auto& instance2 = SingletonTestApi<EmptySingleton4>::Reset();

    auto& instance3 = EmptySingleton4::Get();

    EXPECT_NE(&instance1, &instance2);
    EXPECT_EQ(&instance2, &instance3);

    EXPECT_FALSE(g_destroyed);
}

} } // namespace testing
