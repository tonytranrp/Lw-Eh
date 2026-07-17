#include "test_harness.hpp"
#include <lweh/delegate.hpp>

// Correctness coverage for lweh::delegate<Sig> (research.md Part A §A1).
// signal<Event,N> isn't implemented yet, so this is the lowest-level
// coverage possible right now — everything above delegate depends on it.

namespace {

int add_one(int x) {
    return x + 1;
}

struct counter {
    int value = 0;
    void bump(int by) {
        value += by;
    }
    int get() const {
        return value;
    }
};

struct base {
    virtual ~base() = default;
    virtual int who() const {
        return 1;
    }
};

struct derived : base {
    int who() const override {
        return 2;
    }
};

struct inherits_method {
    int tag() const {
        return 42;
    }
};

struct child_of_inherits_method : inherits_method {};

} // namespace

int main() {
    // Free function: bind + invoke.
    {
        lweh::delegate<int(int)> d;
        d.bind<&add_one>();
        LWEH_EXPECT_EQ(d(41), 42);
    }

    // Member function: bind + invoke mutates the bound instance's own state.
    {
        counter c;
        lweh::delegate<void(int)> d;
        d.bind<&counter::bump>(&c);
        d(5);
        d(5);
        LWEH_EXPECT_EQ(c.value, 10);
    }

    // Two delegate instances bound to different targets don't interfere.
    {
        counter a;
        counter b;
        lweh::delegate<void(int)> da;
        lweh::delegate<void(int)> db;
        da.bind<&counter::bump>(&a);
        db.bind<&counter::bump>(&b);
        da(3);
        db(100);
        LWEH_EXPECT_EQ(a.value, 3);
        LWEH_EXPECT_EQ(b.value, 100);
    }

    // Virtual dispatch: bind through base::who, invoke on a derived
    // instance via a base* — must call derived::who, not base::who.
    {
        derived obj;
        base* as_base = &obj;
        lweh::delegate<int()> d;
        d.bind<&base::who>(as_base);
        LWEH_EXPECT_EQ(d(), 2);
    }

    // Inherited (non-virtual) method: MemFn declared in a base class, T
    // deduced as the derived instance's own type.
    {
        child_of_inherits_method obj;
        lweh::delegate<int()> d;
        d.bind<&inherits_method::tag>(&obj);
        LWEH_EXPECT_EQ(d(), 42);
    }

    // Default-constructed delegate is falsy; a bound one is truthy.
    {
        lweh::delegate<void()> unbound;
        LWEH_EXPECT(!static_cast<bool>(unbound));

        counter c;
        lweh::delegate<void(int)> bound;
        bound.bind<&counter::bump>(&c);
        LWEH_EXPECT(static_cast<bool>(bound));
    }

    // Multi-argument, non-void return signature (not just the single-arg/
    // void cases above) to exercise the variadic Args... path generally.
    {
        struct math {
            int scale(int x, int y) const {
                return x * y + factor;
            }
            int factor = 7;
        } m;
        lweh::delegate<int(int, int)> d;
        d.bind<&math::scale>(&m);
        LWEH_EXPECT_EQ(d(3, 4), 19);
    }

    return lweh_test::finish();
}
