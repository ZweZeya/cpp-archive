#include "type_erasure.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// ---------- shapes ----------

struct Circle {
    double r;
    double area() const { return r * r * 3.14; }
};

struct Square {
    double s;
    double area() const { return s * s; }
};

// Counts live objects, copies and moves so tests can check that Shape
// copies/moves the erased object the right number of times and destroys
// everything it creates exactly once. Setting throw_on_copy makes the
// next copy throw, to test exception safety.
template <std::size_t Pad>
struct Counted {
    static inline int live = 0;
    static inline int copies = 0;
    static inline int moves = 0;
    static inline bool throw_on_copy = false;

    double v;
    std::array<char, Pad> pad{};

    explicit Counted(double v) : v(v) { ++live; }
    Counted(const Counted& o) : v(o.v), pad(o.pad) {
        if (throw_on_copy)
            throw std::runtime_error("copy failed");
        ++live;
        ++copies;
    }
    Counted(Counted&& o) noexcept : v(o.v), pad(o.pad) { ++live; ++moves; }
    Counted& operator=(const Counted&) = default;
    ~Counted() { --live; }

    double area() const { return v; }

    static void reset() { live = 0; copies = 0; moves = 0; throw_on_copy = false; }
};

using Small = Counted<1>;   // goes in the inline buffer
using Big   = Counted<64>;  // goes on the heap

static_assert(sizeof(Small) <= 32, "Small should take the in-buffer path");
static_assert(sizeof(Big)   >  32, "Big should take the heap path");

// Owns heap memory, so a shallow copy shows up as a double free under ASan.
struct Named {
    std::string name;
    double area() const { return static_cast<double>(name.size()); }
};

// Stores a pointer to itself. Its copy/move constructors re-point it, so it is
// only valid if it was really constructed where it lives. Relocating it with
// memcpy leaves `self` pointing at the old address.
struct SelfRef {
    SelfRef* self;
    double v;

    explicit SelfRef(double v) : self(this), v(v) {}
    SelfRef(const SelfRef& o) : self(this), v(o.v) {}
    SelfRef(SelfRef&& o) noexcept : self(this), v(o.v) {}

    double area() const { return self == this ? v : -1.0; }
};

static_assert(sizeof(SelfRef) <= 32, "SelfRef should take the in-buffer path");

// ---------- tiny test harness ----------

// Under ASan, also catch pointers into a function's stack frame that are used
// after the function returns (off by default on macOS).
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
extern "C" const char* __asan_default_options() {
    return "detect_stack_use_after_return=1";
}
#  endif
#endif

static int failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ++failures;                                                     \
            std::cout << "  FAIL " << __LINE__ << ": " #cond "\n";          \
        }                                                                   \
    } while (0)

// ---------- copy construction ----------

template <typename C>
void copy_construct() {
    C::reset();
    {
        Shape a(C{2.0});
        C::copies = 0;              // ignore anything the constructor did

        Shape b = a;

        CHECK(C::copies == 1);      // the erased object was copied exactly once
        CHECK(C::live == 2);        // a and b each own their own object
        CHECK(a.area() == 2.0);
        CHECK(b.area() == 2.0);
    }
    CHECK(C::live == 0);            // both destroyed, nothing destroyed twice
}

// The copy must not point into the original's buffer or share its heap object.
template <typename C>
void copy_outlives_original() {
    C::reset();
    {
        auto* a = new Shape(C{3.0});
        Shape b = *a;
        delete a;                   // ASan flags b if it still refers to a's storage

        CHECK(b.area() == 3.0);
        CHECK(C::live == 1);
    }
    CHECK(C::live == 0);
}

void copy_owning_member() {
    std::string long_name(100, 'x');    // long enough to defeat SSO
    auto* a = new Shape(Named{long_name});
    Shape b = *a;
    delete a;
    CHECK(b.area() == 100.0);
}

// ---------- copy assignment ----------

template <typename From, typename To>
void copy_assign() {
    From::reset();
    To::reset();
    {
        Shape a(To{1.0});
        Shape b(From{5.0});
        From::copies = 0;

        a = b;

        CHECK(a.area() == 5.0);
        CHECK(b.area() == 5.0);
        CHECK(From::copies == 1);
        if constexpr (std::is_same_v<From, To>) {
            CHECK(From::live == 2);     // a's old object gone, a has b's copy
        } else {
            CHECK(To::live == 0);       // a's old object was destroyed
            CHECK(From::live == 2);
        }
    }
    CHECK(From::live == 0);
    CHECK(To::live == 0);
}

template <typename C>
void self_assign() {
    C::reset();
    {
        Shape a(C{7.0});
        Shape& ref = a;             // via a reference to dodge -Wself-assign
        a = ref;

        CHECK(a.area() == 7.0);
        CHECK(C::live == 1);
    }
    CHECK(C::live == 0);
}

// Objects in the buffer must be constructed in place, never memcpy'd there.
void self_ref_survives_copy_and_move() {
    Shape a(SelfRef{1.0});
    CHECK(a.area() == 1.0);

    Shape b = a;                        // copy construct
    CHECK(b.area() == 1.0);

    Shape c(SelfRef{2.0});
    c = a;                              // copy assign
    CHECK(c.area() == 1.0);

    Shape d = std::move(b);             // move construct
    CHECK(d.area() == 1.0);

    Shape e(SelfRef{3.0});
    e = std::move(d);                   // move assign
    CHECK(e.area() == 1.0);
}

// ---------- exception safety ----------

// A failed copy must not leak or leave anything half-built.
template <typename C>
void copy_construct_throws() {
    C::reset();
    {
        Shape a(C{2.0});
        C::throw_on_copy = true;

        bool threw = false;
        try {
            Shape b = a;
        } catch (const std::runtime_error&) {
            threw = true;
        }
        C::throw_on_copy = false;

        CHECK(threw);
        CHECK(a.area() == 2.0);
        CHECK(C::live == 1);
    }
    CHECK(C::live == 0);
}

// Strong guarantee: if copying b's object throws, a is left exactly as it was.
// Without it, a's destructor later destroys an object that is already dead.
template <typename From, typename To>
void copy_assign_throws() {
    From::reset();
    To::reset();
    {
        Shape a(To{1.0});
        Shape b(From{5.0});
        From::throw_on_copy = true;

        bool threw = false;
        try {
            a = b;
        } catch (const std::runtime_error&) {
            threw = true;
        }
        From::throw_on_copy = false;

        CHECK(threw);
        CHECK(static_cast<bool>(a));    // a was not emptied...
        if (a)
            CHECK(a.area() == 1.0);     // ...and still holds its old object
        CHECK(b.area() == 5.0);
        if constexpr (std::is_same_v<From, To>) {
            CHECK(From::live == 2);
        } else {
            CHECK(To::live == 1);
            CHECK(From::live == 1);
        }
    }
    CHECK(From::live == 0);
    CHECK(To::live == 0);
}

// ---------- move construction ----------

template <typename C>
void move_construct() {
    C::reset();
    {
        Shape a(C{4.0});
        C::copies = 0;
        C::moves = 0;

        Shape b = std::move(a);

        CHECK(b.area() == 4.0);
        CHECK(C::copies == 0);          // a move must never copy the object
        if constexpr (std::is_same_v<C, Big>) {
            CHECK(C::moves == 0);       // heap object: steal the pointer, don't touch T
            CHECK(C::live == 1);
        } else {
            CHECK(C::moves == 1);       // buffer object: move T into b's own buffer
        }
    }
    CHECK(C::live == 0);                // moved-from a was destroyed safely
}

// The new Shape must not point into the old one's buffer.
template <typename C>
void move_outlives_original() {
    C::reset();
    {
        auto* a = new Shape(C{3.0});
        Shape b = std::move(*a);
        delete a;                       // ASan flags b if it still refers to a's storage

        CHECK(b.area() == 3.0);
        CHECK(C::live == 1);
    }
    CHECK(C::live == 0);
}

// A moved-from Shape must still be safe to assign to and to destroy.
template <typename C>
void moved_from_is_reusable() {
    C::reset();
    {
        Shape a(C{1.0});
        Shape b = std::move(a);

        a = b;                          // copy-assign into moved-from
        CHECK(a.area() == 1.0);

        a = Shape(C{6.0});              // move-assign into a live object
        CHECK(a.area() == 6.0);
    }
    CHECK(C::live == 0);
}

// ---------- move assignment ----------

template <typename From, typename To>
void move_assign() {
    From::reset();
    To::reset();
    {
        Shape a(To{1.0});
        Shape b(From{5.0});
        From::copies = 0;

        a = std::move(b);

        CHECK(a.area() == 5.0);
        CHECK(From::copies == 0);       // a move must never copy the object
        if constexpr (!std::is_same_v<From, To>) {
            CHECK(To::live == 0);       // a's old object was destroyed
        }
    }
    CHECK(From::live == 0);             // moved-from b destroyed safely
    CHECK(To::live == 0);
}

// a = std::move(a) may leave a in any valid state, but must not
// double-destroy, leak, or leave a unusable.
template <typename C>
void self_move_assign() {
    C::reset();
    {
        Shape a(C{7.0});
        Shape& ref = a;                 // via a reference to dodge -Wself-move
        a = std::move(ref);

        a = Shape(C{8.0});
        CHECK(a.area() == 8.0);
    }
    CHECK(C::live == 0);
}

// ---------- empty (moved-from) state ----------

// After a move the source is empty, and the destination holds the object.
template <typename C>
void moved_from_is_empty() {
    C::reset();
    {
        Shape a(C{1.0});
        CHECK(static_cast<bool>(a));

        Shape b = std::move(a);
        CHECK(!a);
        CHECK(static_cast<bool>(b));

        Shape c(C{2.0});
        c = std::move(b);
        CHECK(!b);
        CHECK(static_cast<bool>(c));
    }
    CHECK(C::live == 0);
}

template <typename C>
void empty_area_throws() {
    C::reset();
    {
        Shape a(C{1.0});
        Shape b = std::move(a);

        bool threw = false;
        try {
            (void)a.area();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        CHECK(threw);
    }
    CHECK(C::live == 0);
}

// Copying or moving from an empty Shape gives an empty Shape, not a crash.
template <typename C>
void copy_and_move_from_empty() {
    C::reset();
    {
        Shape a(C{1.0});
        Shape b = std::move(a);         // a is now empty

        Shape c = a;                    // copy-construct from empty
        CHECK(!c);

        Shape d = std::move(a);         // move-construct from empty
        CHECK(!d);
        CHECK(!a);

        CHECK(C::live == 1);            // only b's object exists
    }
    CHECK(C::live == 0);
}

// Assigning an empty Shape into a full one destroys the old object.
template <typename C>
void assign_from_empty() {
    C::reset();
    {
        Shape a(C{1.0});
        Shape b = std::move(a);         // a is now empty, b holds 1.0

        Shape x(C{2.0});
        x = a;                          // copy-assign from empty
        CHECK(!x);
        CHECK(C::live == 1);            // x's old object destroyed

        Shape y(C{3.0});
        y = std::move(a);               // move-assign from empty
        CHECK(!y);
        CHECK(C::live == 1);            // y's old object destroyed
    }
    CHECK(C::live == 0);
}

// ---------- noexcept and containers ----------

void move_is_noexcept() {
    CHECK(std::is_nothrow_move_constructible_v<Shape>);
    CHECK(std::is_nothrow_move_assignable_v<Shape>);
}

// std::vector only moves elements on reallocation if the move constructor is
// noexcept; otherwise it copies them to keep its own strong guarantee.
template <typename C>
void vector_growth_moves() {
    C::reset();
    {
        std::vector<Shape> v;
        v.reserve(1);
        v.emplace_back(C{0.0});
        C::copies = 0;

        for (int i = 1; i <= 8; ++i)
            v.emplace_back(C{static_cast<double>(i)});

        CHECK(C::copies == 0);          // reallocations moved, never copied
        for (int i = 0; i <= 8; ++i)
            CHECK(v[i].area() == static_cast<double>(i));
    }
    CHECK(C::live == 0);
}

// ---------- runner ----------

struct Test {
    const char* name;
    void (*fn)();
};

static const Test tests[] = {
    {"copy_construct_small",         copy_construct<Small>},
    {"copy_construct_big",           copy_construct<Big>},
    {"copy_outlives_original_small", copy_outlives_original<Small>},
    {"copy_outlives_original_big",   copy_outlives_original<Big>},
    {"copy_owning_member",           copy_owning_member},
    {"copy_assign_small_to_small",   copy_assign<Small, Small>},
    {"copy_assign_big_to_big",       copy_assign<Big, Big>},
    {"copy_assign_big_to_small",     copy_assign<Big, Small>},
    {"copy_assign_small_to_big",     copy_assign<Small, Big>},
    {"self_assign_small",            self_assign<Small>},
    {"self_assign_big",              self_assign<Big>},
    {"self_ref_survives_copy_and_move", self_ref_survives_copy_and_move},

    {"copy_construct_throws_small",       copy_construct_throws<Small>},
    {"copy_construct_throws_big",         copy_construct_throws<Big>},
    {"copy_assign_throws_small_to_small", copy_assign_throws<Small, Small>},
    {"copy_assign_throws_big_to_big",     copy_assign_throws<Big, Big>},
    {"copy_assign_throws_big_to_small",   copy_assign_throws<Big, Small>},
    {"copy_assign_throws_small_to_big",   copy_assign_throws<Small, Big>},

    {"move_construct_small",          move_construct<Small>},
    {"move_construct_big",            move_construct<Big>},
    {"move_outlives_original_small",  move_outlives_original<Small>},
    {"move_outlives_original_big",    move_outlives_original<Big>},
    {"moved_from_is_reusable_small",  moved_from_is_reusable<Small>},
    {"moved_from_is_reusable_big",    moved_from_is_reusable<Big>},
    {"move_assign_small_to_small",    move_assign<Small, Small>},
    {"move_assign_big_to_big",        move_assign<Big, Big>},
    {"move_assign_big_to_small",      move_assign<Big, Small>},
    {"move_assign_small_to_big",      move_assign<Small, Big>},
    {"self_move_assign_small",        self_move_assign<Small>},
    {"self_move_assign_big",          self_move_assign<Big>},
    {"moved_from_is_empty_small",       moved_from_is_empty<Small>},
    {"moved_from_is_empty_big",         moved_from_is_empty<Big>},
    {"empty_area_throws",               empty_area_throws<Small>},
    {"copy_and_move_from_empty_small",  copy_and_move_from_empty<Small>},
    {"copy_and_move_from_empty_big",    copy_and_move_from_empty<Big>},
    {"assign_from_empty_small",         assign_from_empty<Small>},
    {"assign_from_empty_big",           assign_from_empty<Big>},
    {"move_is_noexcept",              move_is_noexcept},
    {"vector_growth_moves_small",     vector_growth_moves<Small>},
    {"vector_growth_moves_big",       vector_growth_moves<Big>},
};

// Run all tests, or only the one named on the command line:
//   ./a                      run everything
//   ./a copy_construct_big   run a single test
int main(int argc, char** argv) {
    for (const auto& t : tests) {
        if (argc > 1 && std::strcmp(argv[1], t.name) != 0)
            continue;
        std::cout << t.name << '\n';
        t.fn();
    }

    std::cout << (failures == 0 ? "all passed\n" : "some checks failed\n");
    return failures == 0 ? 0 : 1;
}
