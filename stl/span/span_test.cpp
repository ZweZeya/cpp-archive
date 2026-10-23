// Tests for my_std::Span. No framework needed (same style as tests.cpp).
//
// Build:  clang++ -std=c++20 -g -Wall -Wextra -fsanitize=address,undefined stl/span/span_test.cpp -o span_test
// Run:    ./span_test
//
// Blocks marked "#if 0 // TODO" cover features span.hpp doesn't have yet.
// Flip one to "#if 1" when you implement that feature; it should compile and pass.

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <numeric>
#include <ranges>
#include <type_traits>
#include <vector>

#include "span.hpp"

// ---------------------------------------------------------------- mini framework

namespace {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

int g_failures = 0;

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

}  // namespace

#define TEST(name)                                  \
    static void name();                             \
    static Registrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cout << "    FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr)                                                       \
    do {                                                                         \
        bool threw_ = false;                                                     \
        try { expr; } catch (...) { threw_ = true; }                             \
        CHECK(threw_);                                                           \
    } while (0)

using my_std::Span;
using my_std::dynamic_extent;

struct Base { int v; };
struct Derived : Base { int extra; };

// ---------------------------------------------------------------- compile-time properties

// Fixed extent stores only a pointer; dynamic extent stores pointer + size.
static_assert(sizeof(Span<int, 3>) == sizeof(int*));
static_assert(sizeof(Span<int>) == sizeof(int*) + sizeof(std::size_t));

static_assert(std::is_same_v<Span<const int>::element_type, const int>);
static_assert(std::is_same_v<Span<const int>::value_type, int>);

// A span is a cheap view: copying it must be trivial.
static_assert(std::is_trivially_copyable_v<Span<int>>);
static_assert(std::is_trivially_copyable_v<Span<int, 3>>);

// Raw-pointer iterators make it a contiguous range for free.
static_assert(std::ranges::contiguous_range<Span<int>>);
static_assert(std::ranges::sized_range<Span<int>>);

// Extent must match for fixed-extent spans.
static_assert(std::is_constructible_v<Span<int, 3>, int (&)[3]>);
static_assert(!std::is_constructible_v<Span<int, 4>, int (&)[3]>);
static_assert(std::is_constructible_v<Span<int, 3>, std::array<int, 3>&>);
static_assert(!std::is_constructible_v<Span<int, 4>, std::array<int, 3>&>);

// Constness can be added, never dropped.
static_assert(std::is_constructible_v<Span<const int>, std::array<int, 3>&>);
static_assert(std::is_constructible_v<Span<const int>, const std::array<int, 3>&>);
static_assert(!std::is_constructible_v<Span<int>, const std::array<int, 3>&>);
static_assert(!std::is_constructible_v<Span<int>, const int (&)[3]>);

// Viewing Derived[] as Base[] would break pointer arithmetic, so it must be rejected.
static_assert(!std::is_constructible_v<Span<Base>, std::array<Derived, 3>&>);

// Pointer + count is implicit for dynamic extent, explicit for fixed extent.
// Copy-list-init ({p, n} passed to a by-value parameter) only works with a non-explicit constructor.
template <class S>
void accepts(S) {}
template <class S>
concept implicit_from_ptr_count = requires(int* p, std::size_t n) { accepts<S>({p, n}); };
static_assert(implicit_from_ptr_count<Span<int>>);
static_assert(!implicit_from_ptr_count<Span<int, 3>>);
static_assert(std::is_constructible_v<Span<int, 3>, int*, std::size_t>);

// first<N>() / last<N>() return a fixed extent; first(n) / last(n) return dynamic.
static_assert(std::is_same_v<decltype(std::declval<Span<int>>().first<2>()), Span<int, 2>>);
static_assert(std::is_same_v<decltype(std::declval<Span<int>>().last<2>()), Span<int, 2>>);
static_assert(std::is_same_v<decltype(std::declval<Span<int, 3>>().first(2)), Span<int>>);
static_assert(std::is_same_v<decltype(std::declval<Span<int, 3>>().last(2)), Span<int>>);

// Taking more than a fixed extent holds must not compile.
template <class S, std::size_t N>
concept can_first = requires(S s) { s.template first<N>(); };
static_assert(can_first<Span<int, 3>, 3>);
static_assert(!can_first<Span<int, 3>, 4>);

// Works in constant expressions.
constexpr int sum_of_constexpr_array() {
    std::array<int, 4> arr{1, 2, 3, 4};
    Span<int> s(arr);
    int sum = 0;
    for (int v : s) sum += v;
    return sum + s.front() + s.back() + s[1];
}
static_assert(sum_of_constexpr_array() == 10 + 1 + 4 + 2);

// ---------------------------------------------------------------- construction

TEST(span_from_c_array) {
    int c[3]{1, 2, 3};
    Span<int> s(c);
    CHECK(s.data() == c);
    CHECK(s.size() == 3);

    Span<int, 3> f(c);
    CHECK(f.data() == c);
    CHECK(f.size() == 3);
}

TEST(span_from_std_array) {
    std::array<int, 4> arr{1, 2, 3, 4};
    Span<int> s(arr);
    CHECK(s.data() == arr.data());
    CHECK(s.size() == 4);

    Span<int, 4> f(arr);
    CHECK(f.size() == 4);
}

TEST(span_from_const_std_array) {
    const std::array<int, 2> arr{7, 8};
    Span<const int> s(arr);
    CHECK(s.data() == arr.data());
    CHECK(s[0] == 7);
    CHECK(s[1] == 8);
}

TEST(span_from_pointer_and_count) {
    int c[5]{0, 1, 2, 3, 4};
    Span<int> s(c + 1, 3);
    CHECK(s.data() == c + 1);
    CHECK(s.size() == 3);
    CHECK(s[0] == 1);

    Span<int, 3> f(c + 2, 3);
    CHECK(f[0] == 2);
}

TEST(span_empty_from_nullptr) {
    Span<int> s(nullptr, 0);
    CHECK(s.empty());
    CHECK(s.size() == 0);
    CHECK(s.size_bytes() == 0);
    CHECK(s.begin() == s.end());
}

// ---------------------------------------------------------------- observers

TEST(span_size_and_size_bytes) {
    double d[4]{};
    Span<double> s(d);
    CHECK(s.size() == 4);
    CHECK(s.size_bytes() == 4 * sizeof(double));
    CHECK(!s.empty());
}

// ---------------------------------------------------------------- element access

TEST(span_index_and_front_back) {
    int c[3]{10, 20, 30};
    Span<int> s(c);
    CHECK(s[0] == 10);
    CHECK(s[2] == 30);
    CHECK(s.front() == 10);
    CHECK(s.back() == 30);
    CHECK(&s.front() == c);
    CHECK(&s.back() == c + 2);
}

TEST(span_writes_through_to_underlying_data) {
    int c[3]{1, 2, 3};
    Span<int> s(c);
    s[1] = 42;
    s.front() = 7;
    s.back() = 9;
    CHECK(c[0] == 7);
    CHECK(c[1] == 42);
    CHECK(c[2] == 9);
}

TEST(span_const_span_still_mutates_elements) {
    // Shallow constness: a const span is a const view handle, not a view of const data.
    int c[2]{1, 2};
    const Span<int> s(c);
    s[0] = 100;
    CHECK(c[0] == 100);
}

TEST(span_at_in_range) {
    int c[3]{1, 2, 3};
    Span<int> s(c);
    CHECK(s.at(0) == 1);
    CHECK(s.at(2) == 3);
}

TEST(span_at_out_of_range_throws) {
    int c[3]{1, 2, 3};
    Span<int> s(c);
    CHECK_THROWS(s.at(3));  // index == size is the classic off-by-one
    CHECK_THROWS(s.at(100));
    CHECK_THROWS(s.at(static_cast<std::size_t>(-1)));

    Span<int> e(nullptr, 0);
    CHECK_THROWS(e.at(0));
}

// ---------------------------------------------------------------- iterators

TEST(span_range_for) {
    int c[4]{1, 2, 3, 4};
    Span<int> s(c);
    int sum = 0;
    for (int v : s) sum += v;
    CHECK(sum == 10);
}

TEST(span_begin_end_distance) {
    int c[4]{};
    Span<int> s(c);
    CHECK(s.begin() == c);
    CHECK(s.end() == c + 4);
    CHECK(std::distance(s.begin(), s.end()) == 4);
}

TEST(span_works_with_algorithms) {
    int c[5]{5, 3, 1, 4, 2};
    Span<int> s(c);
    std::sort(s.begin(), s.end());
    CHECK(std::is_sorted(c, c + 5));
    CHECK(std::accumulate(s.begin(), s.end(), 0) == 15);
    CHECK(std::ranges::find(s, 4) == s.begin() + 3);
}

// ---------------------------------------------------------------- subviews

TEST(span_first_and_last_template) {
    int c[5]{0, 1, 2, 3, 4};
    Span<int> s(c);

    auto f = s.first<2>();
    CHECK(f.data() == c);
    CHECK(f.size() == 2);

    auto l = s.last<2>();
    CHECK(l.data() == c + 3);
    CHECK(l[0] == 3);
    CHECK(l[1] == 4);
}

TEST(span_first_and_last_runtime) {
    int c[5]{0, 1, 2, 3, 4};
    Span<int, 5> s(c);

    auto f = s.first(3);
    CHECK(f.data() == c);
    CHECK(f.size() == 3);

    auto l = s.last(3);
    CHECK(l.data() == c + 2);
    CHECK(l.size() == 3);
}

TEST(span_first_and_last_edge_counts) {
    int c[3]{1, 2, 3};
    Span<int> s(c);
    CHECK(s.first(0).empty());
    CHECK(s.last(0).empty());
    CHECK(s.first(3).size() == 3);
    CHECK(s.last(3).data() == c);
}

// ---------------------------------------------------------------- not implemented yet

#if 0  // TODO: member types and static extent
static_assert(std::is_same_v<Span<int>::size_type, std::size_t>);
static_assert(Span<int, 3>::extent == 3);
static_assert(Span<int>::extent == dynamic_extent);
#endif

#if 0  // TODO: default constructor (only for dynamic extent or extent 0)
static_assert(std::is_default_constructible_v<Span<int>>);
static_assert(std::is_default_constructible_v<Span<int, 0>>);
static_assert(!std::is_default_constructible_v<Span<int, 3>>);

TEST(span_default_constructed_is_empty) {
    Span<int> s;
    CHECK(s.data() == nullptr);
    CHECK(s.empty());
}
#endif

#if 1  // converting constructor between spans
// Adding const is fine; dropping it is not.
static_assert(std::is_convertible_v<Span<int>, Span<const int>>);
static_assert(!std::is_constructible_v<Span<int>, Span<const int>>);
// Fixed -> dynamic is implicit.
static_assert(std::is_convertible_v<Span<int, 3>, Span<int>>);
// Fixed -> fixed with matching extent is implicit (sizes are known to agree).
static_assert(std::is_convertible_v<Span<int, 3>, Span<const int, 3>>);
static_assert(std::is_convertible_v<Span<int, 3>, Span<int, 3>>);
// Dynamic -> fixed is allowed, but only explicitly.
static_assert(std::is_constructible_v<Span<int, 3>, Span<int>>);
static_assert(!std::is_convertible_v<Span<int>, Span<int, 3>>);
// Mismatched fixed extents never convert.
static_assert(!std::is_constructible_v<Span<int, 4>, Span<int, 3>>);
// Derived -> Base must still be rejected.
static_assert(!std::is_constructible_v<Span<Base>, Span<Derived>>);

static std::size_t takes_const_span(Span<const int> s) { return s.size(); }

TEST(span_converts_to_const_and_dynamic) {
    int c[4]{1, 2, 3, 4};
    Span<int, 4> f(c);
    CHECK(takes_const_span(f) == 4);
    CHECK(takes_const_span(f.first<2>()) == 2);

    Span<int> d = f;
    CHECK(d.data() == c);
    CHECK(d.size() == 4);
}
#endif

#if 0  // TODO: subspan
// Fixed extent + template Offset/Count: result extent is Count,
// or (Extent - Offset) when Count is dynamic_extent and Extent is fixed.
static_assert(std::is_same_v<decltype(std::declval<Span<int, 5>>().subspan<1, 2>()), Span<int, 2>>);
static_assert(std::is_same_v<decltype(std::declval<Span<int, 5>>().subspan<1>()), Span<int, 4>>);
static_assert(std::is_same_v<decltype(std::declval<Span<int>>().subspan<1>()), Span<int>>);
static_assert(std::is_same_v<decltype(std::declval<Span<int, 5>>().subspan(1, 2)), Span<int>>);

TEST(span_subspan) {
    int c[5]{0, 1, 2, 3, 4};
    Span<int> s(c);

    auto a = s.subspan<1, 3>();
    CHECK(a.data() == c + 1);
    CHECK(a.size() == 3);

    auto b = s.subspan(2);  // count defaults to "the rest"
    CHECK(b.data() == c + 2);
    CHECK(b.size() == 3);

    auto e = s.subspan(5);  // offset == size is valid and gives an empty span
    CHECK(e.empty());
}
#endif

#if 0  // TODO: deduction guides (CTAD)
TEST(span_ctad) {
    int c[3]{};
    std::array<int, 4> arr{};
    const std::array<int, 2> carr{};

    my_std::Span a(c);
    my_std::Span b(arr);
    my_std::Span d(carr);
    my_std::Span p(c, 2);

    static_assert(std::is_same_v<decltype(a), Span<int, 3>>);
    static_assert(std::is_same_v<decltype(b), Span<int, 4>>);
    static_assert(std::is_same_v<decltype(d), Span<const int, 2>>);
    static_assert(std::is_same_v<decltype(p), Span<int>>);
}
#endif

#if 1  // reverse iterators
TEST(span_reverse_iteration) {
    int c[3]{1, 2, 3};
    Span<int> s(c);
    std::vector<int> out(s.rbegin(), s.rend());
    CHECK((out == std::vector<int>{3, 2, 1}));
}
#endif

// ---------------------------------------------------------------- runner

int main() {
    int ran = 0, failed = 0;
    for (auto& t : registry()) {
        int before = g_failures;
        t.fn();
        ++ran;
        bool ok = g_failures == before;
        if (!ok) ++failed;
        std::cout << (ok ? "[ ok ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << "\n" << (ran - failed) << "/" << ran << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
