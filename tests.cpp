// Tests for everything in cpp-archive. No framework needed.
//
// Build:  clang++ -std=c++20 -g -Wall -Wextra -pthread -fsanitize=address,undefined tests.cpp -o tests
// Run:    ./tests               run everything
//         ./tests vector2       run tests whose name contains "vector2"
//         ./tests --list        list test names

#include <atomic>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "utility/utility.hpp"
#include "stl/vector/vector.hpp"
#include "stl/vector/vector_with_allocator.hpp"
#include "stl/string/basic_string.hpp"
#include "smart_pointers/unique_ptr.hpp"
#include "smart_pointers/shared_ptr.hpp"
#include "smart_pointers/weak_ptr.hpp"
#include "concurrency/spsc_queue.hpp"

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

// Counts live instances so tests can catch leaks and double destruction.
struct Tracked {
    static inline int live = 0;
    int value;

    Tracked(int v = 0) : value(v) { ++live; }
    Tracked(const Tracked& o) : value(o.value) { ++live; }
    Tracked(Tracked&& o) noexcept : value(o.value) { o.value = -1; ++live; }
    Tracked& operator=(const Tracked&) = default;
    Tracked& operator=(Tracked&& o) noexcept { value = o.value; o.value = -1; return *this; }
    ~Tracked() { --live; }
};

struct Pair {
    int a;
    std::string b;
    Pair(int a, std::string b) : a(a), b(std::move(b)) {}
};

struct MoveOnly {
    int v;
    explicit MoveOnly(int v) : v(v) {}
    MoveOnly(MoveOnly&& o) noexcept : v(o.v) { o.v = -1; }
    MoveOnly(const MoveOnly&) = delete;
};

// ---------------------------------------------------------------- utility

TEST(utility_remove_reference) {
    static_assert(std::is_same_v<my_std::remove_reference_t<int>, int>);
    static_assert(std::is_same_v<my_std::remove_reference_t<int&>, int>);
    static_assert(std::is_same_v<my_std::remove_reference_t<int&&>, int>);
    static_assert(std::is_same_v<my_std::remove_reference_t<const int&>, const int>);
}

TEST(utility_is_lvalue_reference) {
    static_assert(my_std::is_lvalue_reference_v<int&>);
    static_assert(!my_std::is_lvalue_reference_v<int>);
    static_assert(!my_std::is_lvalue_reference_v<int&&>);
}

TEST(utility_move) {
    int x = 1;
    static_assert(std::is_same_v<decltype(my_std::move(x)), int&&>);
    static_assert(noexcept(my_std::move(x)));

    MoveOnly a(7);
    MoveOnly b(my_std::move(a));
    CHECK(b.v == 7);
    CHECK(a.v == -1);

    std::string s = "hello";
    std::string t(my_std::move(s));
    CHECK(t == "hello");
    CHECK(s.empty());
}

namespace {
enum class Cat { lvalue, rvalue };
Cat which(int&) { return Cat::lvalue; }
Cat which(int&&) { return Cat::rvalue; }
template <typename T>
Cat forwarder(T&& t) { return which(my_std::forward<T>(t)); }
}  // namespace

TEST(utility_forward) {
    int x = 0;
    CHECK(forwarder(x) == Cat::lvalue);
    CHECK(forwarder(1) == Cat::rvalue);
    CHECK(forwarder(my_std::move(x)) == Cat::rvalue);
}

// ---------------------------------------------------------------- vector / vector2
// Every VECTOR_TEST runs against both my_std::vector and my_std::vector2.

#define VECTOR_TEST(name)                                                  \
    template <typename V> static void vec_##name();                        \
    TEST(vector_##name)  { vec_##name<my_std::vector<Tracked>>(); }        \
    TEST(vector2_##name) { vec_##name<my_std::vector2<Tracked>>(); }       \
    template <typename V> static void vec_##name()

VECTOR_TEST(starts_empty) {
    V v;
    CHECK(v.empty());
    CHECK(v.size() == 0);
}

VECTOR_TEST(push_back_and_index) {
    {
        V v;
        for (int i = 0; i < 100; i++) v.push_back(Tracked(i));
        CHECK(v.size() == 100);
        CHECK(!v.empty());
        for (int i = 0; i < 100; i++) CHECK(v[i].value == i);
    }
    CHECK(Tracked::live == 0);
}

VECTOR_TEST(push_back_lvalue_copies) {
    V v;
    Tracked t(5);
    v.push_back(t);
    CHECK(t.value == 5);
    CHECK(v[0].value == 5);
}

VECTOR_TEST(push_back_rvalue_moves) {
    V v;
    Tracked t(5);
    v.push_back(my_std::move(t));
    CHECK(t.value == -1);
    CHECK(v[0].value == 5);
}

VECTOR_TEST(emplace_back) {
    V v;
    v.emplace_back(1);
    v.emplace_back(2);
    CHECK(v.size() == 2);
    CHECK(v[0].value == 1);
    CHECK(v[1].value == 2);
}

VECTOR_TEST(mutate_through_index) {
    V v;
    v.push_back(Tracked(1));
    v[0].value = 42;
    const V& cv = v;
    CHECK(cv[0].value == 42);
}

VECTOR_TEST(pop_back) {
    {
        V v;
        v.push_back(Tracked(1));
        v.push_back(Tracked(2));
        v.pop_back();
        CHECK(v.size() == 1);
        CHECK(v[0].value == 1);
        CHECK(Tracked::live == 1);
        v.pop_back();
        CHECK(v.empty());
    }
    CHECK(Tracked::live == 0);
}

VECTOR_TEST(pop_back_on_empty_throws) {
    V v;
    CHECK_THROWS(v.pop_back());
}

VECTOR_TEST(clear) {
    V v;
    for (int i = 0; i < 10; i++) v.push_back(Tracked(i));
    v.clear();
    CHECK(v.empty());
    CHECK(Tracked::live == 0);
    v.push_back(Tracked(3));  // still usable after clear
    CHECK(v.size() == 1);
    CHECK(v[0].value == 3);
}

VECTOR_TEST(reserve_keeps_elements) {
    V v;
    v.push_back(Tracked(1));
    v.push_back(Tracked(2));
    v.reserve(64);
    CHECK(v.size() == 2);
    CHECK(v[0].value == 1);
    CHECK(v[1].value == 2);
}

VECTOR_TEST(copy_constructor_is_deep) {
    V a;
    a.push_back(Tracked(1));
    a.push_back(Tracked(2));
    V b(a);
    b[0].value = 99;
    CHECK(a[0].value == 1);
    CHECK(b.size() == 2);
    CHECK(b[1].value == 2);
}

VECTOR_TEST(copy_assignment_is_deep) {
    V a, b;
    a.push_back(Tracked(1));
    b.push_back(Tracked(7));
    b.push_back(Tracked(8));
    b = a;
    CHECK(b.size() == 1);
    CHECK(b[0].value == 1);
    b[0].value = 50;
    CHECK(a[0].value == 1);
}

VECTOR_TEST(self_assignment) {
    V a;
    a.push_back(Tracked(1));
    V& ref = a;
    a = ref;
    CHECK(a.size() == 1);
    CHECK(a[0].value == 1);
}

VECTOR_TEST(move_constructor_steals) {
    V a;
    a.push_back(Tracked(1));
    a.push_back(Tracked(2));
    V b(my_std::move(a));
    CHECK(b.size() == 2);
    CHECK(a.size() == 0);
    CHECK(Tracked::live == 2);
}

VECTOR_TEST(move_assignment_steals) {
    V a, b;
    a.push_back(Tracked(1));
    b.push_back(Tracked(7));
    b.push_back(Tracked(8));
    b = my_std::move(a);
    CHECK(b.size() == 1);
    CHECK(b[0].value == 1);
    CHECK(a.size() == 0);
    CHECK(Tracked::live == 1);
}

VECTOR_TEST(copy_and_move_of_empty) {
    V a;
    V b(a);
    V c;
    c = a;
    V d(my_std::move(a));
    CHECK(b.empty());
    CHECK(c.empty());
    CHECK(d.empty());
}

VECTOR_TEST(moved_from_is_reusable) {
    V a;
    a.push_back(Tracked(1));
    V b(my_std::move(a));
    a.push_back(Tracked(2));
    CHECK(a.size() == 1);
    CHECK(a[0].value == 2);
}

VECTOR_TEST(push_back_own_element_across_growth) {
    V v;
    v.push_back(Tracked(1));  // size == capacity == 1, so the next push reallocates
    v.push_back(v[0]);
    CHECK(v.size() == 2);
    CHECK(v[1].value == 1);
}

TEST(vector_capacity_grows_by_doubling) {
    my_std::vector<int> v;
    CHECK(v.capacity() == 0);
    v.push_back(1);
    CHECK(v.capacity() == 1);
    v.push_back(2);
    CHECK(v.capacity() == 2);
    v.push_back(3);
    CHECK(v.capacity() == 4);
    v.reserve(100);
    CHECK(v.capacity() == 100);
    v.reserve(10);  // never shrinks
    CHECK(v.capacity() == 100);
}

TEST(vector_emplace_back_multiple_args) {
    my_std::vector<Pair> v;
    v.emplace_back(1, "one");
    v.emplace_back(2, std::string("two"));
    CHECK(v.size() == 2);
    CHECK(v[1].a == 2);
    CHECK(v[1].b == "two");
}

TEST(vector_move_only_elements) {
    my_std::vector<MoveOnly> v;
    v.emplace_back(1);
    v.push_back(MoveOnly(2));
    v.push_back(MoveOnly(3));  // forces reallocation, must move
    CHECK(v.size() == 3);
    CHECK(v[0].v == 1);
    CHECK(v[2].v == 3);
}

TEST(vector2_capacity_and_reserve) {
    my_std::vector2<int> v;
    CHECK(v.capacity() == 0);
    v.reserve(100);
    CHECK(v.capacity() == 100);
    CHECK(v.empty());
    for (int i = 0; i < 100; i++) v.push_back(i);
    CHECK(v.capacity() == 100);  // no reallocation while within reserved space
    v.push_back(100);
    CHECK(v.capacity() == 200);
    v.reserve(10);  // never shrinks
    CHECK(v.capacity() == 200);
    for (int i = 0; i <= 100; i++) CHECK(v[i] == i);
}

TEST(vector2_move_only_elements) {
    my_std::vector2<MoveOnly> v;
    v.emplace_back(1);
    v.push_back(MoveOnly(2));
    v.push_back(MoveOnly(3));
    CHECK(v.size() == 3);
    CHECK(v[0].v == 1);
    CHECK(v[2].v == 3);
}

TEST(vector_of_std_strings) {
    my_std::vector<std::string> v;
    for (int i = 0; i < 50; i++) v.push_back(std::string(20, 'a' + i % 26));
    CHECK(v.size() == 50);
    CHECK(v[3] == std::string(20, 'd'));
}

// ---------------------------------------------------------------- string

TEST(string_default_is_empty) {
    my_std::string s;
    CHECK(s.empty());
    CHECK(s.size() == 0);
    CHECK(s.length() == 0);
}

TEST(string_from_c_string) {
    char buf[] = "hello";
    my_std::string s(buf);
    CHECK(s.size() == 5);
    CHECK(s.length() == 5);
    CHECK(!s.empty());
    CHECK(s[0] == 'h');
    CHECK(s[4] == 'o');
    CHECK(std::strcmp(s.c_str(), "hello") == 0);
}

TEST(string_from_string_literal) {
    my_std::string s("hello");
    CHECK(s.size() == 5);
}

TEST(string_index_mutation) {
    char buf[] = "abc";
    my_std::string s(buf);
    s[1] = 'X';
    CHECK(std::strcmp(s.c_str(), "aXc") == 0);
}

TEST(string_copy_constructor_is_deep) {
    char buf[] = "hello";
    my_std::string a(buf);
    my_std::string b(a);
    b[0] = 'J';
    CHECK(std::strcmp(a.c_str(), "hello") == 0);
    CHECK(std::strcmp(b.c_str(), "Jello") == 0);
}

TEST(string_copy_assignment_is_deep) {
    char x[] = "hello", y[] = "hi";
    my_std::string a(x), b(y);
    b = a;
    CHECK(b.size() == 5);
    CHECK(std::strcmp(b.c_str(), "hello") == 0);
    b[0] = 'J';
    CHECK(std::strcmp(a.c_str(), "hello") == 0);
}

TEST(string_self_assignment) {
    char x[] = "hello";
    my_std::string a(x);
    my_std::string& ref = a;
    a = ref;
    CHECK(std::strcmp(a.c_str(), "hello") == 0);
}

TEST(string_move_constructor_steals) {
    char x[] = "hello";
    my_std::string a(x);
    my_std::string b(my_std::move(a));
    CHECK(std::strcmp(b.c_str(), "hello") == 0);
    CHECK(a.size() == 0);
}

TEST(string_move_assignment_steals) {
    char x[] = "hello", y[] = "hi";
    my_std::string a(x), b(y);
    b = my_std::move(a);
    CHECK(std::strcmp(b.c_str(), "hello") == 0);
    CHECK(a.size() == 0);
}

// A moved-from string must still be a valid, empty string.
TEST(string_moved_from_constructor_is_valid_empty) {
    char x[] = "hello";
    my_std::string a(x);
    my_std::string b(my_std::move(a));
    CHECK(a.empty());
    CHECK(a.c_str() != nullptr);
    CHECK(a.c_str() != nullptr && std::strcmp(a.c_str(), "") == 0);
    std::ostringstream os;
    os << a;
    CHECK(os.str().empty());
}

TEST(string_moved_from_assignment_is_valid_empty) {
    char x[] = "hello", y[] = "hi";
    my_std::string a(x), b(y);
    b = my_std::move(a);
    CHECK(a.empty());
    CHECK(a.c_str() != nullptr);
    CHECK(a.c_str() != nullptr && std::strcmp(a.c_str(), "") == 0);
}

TEST(string_moved_from_is_reusable) {
    char x[] = "hello";
    my_std::string a(x);
    my_std::string b(my_std::move(a));
    a.push_back('z');
    CHECK(a.size() == 1);
    CHECK(std::strcmp(a.c_str(), "z") == 0);
    CHECK(std::strcmp(b.c_str(), "hello") == 0);
}

TEST(string_copy_of_moved_from) {
    char x[] = "hello";
    my_std::string a(x);
    my_std::string b(my_std::move(a));
    my_std::string c(a);
    CHECK(c.empty());
    char y[] = "hi";
    my_std::string d(y);
    d = a;
    CHECK(d.empty());
}

TEST(string_push_back_from_literal) {
    char x[] = "ab";
    my_std::string s(x);
    s.push_back('c');
    char d = 'd';
    s.push_back(d);
    CHECK(s.size() == 4);
    CHECK(std::strcmp(s.c_str(), "abcd") == 0);
}

TEST(string_push_back_many) {
    char x[] = "";
    my_std::string s(x);
    std::string expected;
    for (int i = 0; i < 200; i++) {
        char c = 'a' + i % 26;
        s.push_back(c);
        expected.push_back(c);
    }
    CHECK(s.size() == 200);
    CHECK(expected == s.c_str());
}

TEST(string_push_back_on_default_constructed) {
    my_std::string s;
    s.push_back('a');
    s.push_back('b');
    CHECK(s.size() == 2);
    CHECK(std::strcmp(s.c_str(), "ab") == 0);
}

TEST(string_pop_back) {
    char x[] = "abc";
    my_std::string s(x);
    s.pop_back();
    CHECK(s.size() == 2);
    CHECK(std::strcmp(s.c_str(), "ab") == 0);
    s.pop_back();
    s.pop_back();
    CHECK(s.empty());
    CHECK(std::strcmp(s.c_str(), "") == 0);
}

TEST(string_pop_back_on_empty_throws) {
    my_std::string s;
    CHECK_THROWS(s.pop_back());
}

TEST(string_copy_of_default_constructed) {
    my_std::string a;
    my_std::string b(a);
    CHECK(b.empty());
}

TEST(string_copy_assign_from_default_constructed) {
    char x[] = "hi";
    my_std::string a, b(x);
    b = a;
    CHECK(b.empty());
}

TEST(string_stream_output) {
    char x[] = "hello";
    my_std::string s(x);
    std::ostringstream os;
    os << s;
    CHECK(os.str() == "hello");
}

// ---------------------------------------------------------------- unique_ptr

TEST(unique_ptr_default_is_null) {
    my_std::unique_ptr<int> p;
    CHECK(p.get() == nullptr);
    CHECK(!p);
}

TEST(unique_ptr_owns_and_deletes) {
    {
        my_std::unique_ptr<Tracked> p(new Tracked(5));
        CHECK(p);
        CHECK(p->value == 5);
        CHECK((*p).value == 5);
        CHECK(Tracked::live == 1);
    }
    CHECK(Tracked::live == 0);
}

TEST(unique_ptr_move_constructor) {
    my_std::unique_ptr<Tracked> a(new Tracked(1));
    Tracked* raw = a.get();
    my_std::unique_ptr<Tracked> b(my_std::move(a));
    CHECK(a.get() == nullptr);
    CHECK(b.get() == raw);
    CHECK(Tracked::live == 1);
}

TEST(unique_ptr_move_assignment_deletes_old) {
    my_std::unique_ptr<Tracked> a(new Tracked(1));
    my_std::unique_ptr<Tracked> b(new Tracked(2));
    b = my_std::move(a);
    CHECK(Tracked::live == 1);
    CHECK(b->value == 1);
    CHECK(!a);
}

TEST(unique_ptr_self_move_assignment) {
    my_std::unique_ptr<Tracked> a(new Tracked(1));
    my_std::unique_ptr<Tracked>& ref = a;
    a = my_std::move(ref);
    CHECK(a);
    CHECK(a->value == 1);
    CHECK(Tracked::live == 1);
}

TEST(unique_ptr_release) {
    my_std::unique_ptr<Tracked> a(new Tracked(1));
    Tracked* raw = a.release();
    CHECK(!a);
    CHECK(Tracked::live == 1);  // release must not delete
    delete raw;
    CHECK(Tracked::live == 0);
}

TEST(unique_ptr_reset) {
    my_std::unique_ptr<Tracked> a(new Tracked(1));
    a.reset(new Tracked(2));
    CHECK(Tracked::live == 1);
    CHECK(a->value == 2);
    a.reset();
    CHECK(Tracked::live == 0);
    CHECK(!a);
}

TEST(unique_ptr_is_move_only) {
    static_assert(!std::is_copy_constructible_v<my_std::unique_ptr<int>>);
    static_assert(!std::is_copy_assignable_v<my_std::unique_ptr<int>>);
    static_assert(std::is_nothrow_move_constructible_v<my_std::unique_ptr<int>>);
}

// ---------------------------------------------------------------- shared_ptr

TEST(shared_ptr_default_is_null) {
    my_std::shared_ptr<int> p;
    CHECK(p.get() == nullptr);
    CHECK(!p);
    CHECK(p.use_count() == 0);
}

TEST(shared_ptr_owns_and_deletes) {
    {
        my_std::shared_ptr<Tracked> p(new Tracked(5));
        CHECK(p);
        CHECK(p.use_count() == 1);
        CHECK(p->value == 5);
        CHECK((*p).value == 5);
        CHECK(Tracked::live == 1);
    }
    CHECK(Tracked::live == 0);
}

TEST(shared_ptr_copy_shares_ownership) {
    my_std::shared_ptr<Tracked> a(new Tracked(1));
    {
        my_std::shared_ptr<Tracked> b(a);
        CHECK(a.use_count() == 2);
        CHECK(b.use_count() == 2);
        CHECK(a.get() == b.get());
        CHECK(Tracked::live == 1);
    }
    CHECK(a.use_count() == 1);
    CHECK(Tracked::live == 1);
}

TEST(shared_ptr_move_constructor) {
    my_std::shared_ptr<Tracked> a(new Tracked(1));
    my_std::shared_ptr<Tracked> b(my_std::move(a));
    CHECK(!a);
    CHECK(a.use_count() == 0);
    CHECK(b.use_count() == 1);
    CHECK(Tracked::live == 1);
}

TEST(shared_ptr_copy_assignment) {
    my_std::shared_ptr<Tracked> a(new Tracked(1));
    my_std::shared_ptr<Tracked> b(new Tracked(2));
    b = a;
    CHECK(Tracked::live == 1);  // b's old object is gone
    CHECK(a.use_count() == 2);
    CHECK(b->value == 1);
}

TEST(shared_ptr_move_assignment) {
    my_std::shared_ptr<Tracked> a(new Tracked(1));
    my_std::shared_ptr<Tracked> b(new Tracked(2));
    b = my_std::move(a);
    CHECK(Tracked::live == 1);
    CHECK(!a);
    CHECK(b.use_count() == 1);
    CHECK(b->value == 1);
}

TEST(shared_ptr_self_assignment) {
    my_std::shared_ptr<Tracked> a(new Tracked(1));
    my_std::shared_ptr<Tracked>& ref = a;
    a = ref;
    CHECK(a.use_count() == 1);
    a = my_std::move(ref);
    CHECK(a.use_count() == 1);
    CHECK(Tracked::live == 1);
}

TEST(shared_ptr_assign_to_null_ptr) {
    my_std::shared_ptr<Tracked> a(new Tracked(1));
    my_std::shared_ptr<Tracked> empty;
    empty = a;
    CHECK(a.use_count() == 2);
    a = my_std::shared_ptr<Tracked>();
    CHECK(!a);
    CHECK(empty.use_count() == 1);
    CHECK(Tracked::live == 1);
}

TEST(shared_ptr_const_deref) {
    const my_std::shared_ptr<int> p(new int(9));
    CHECK(*p == 9);
}

TEST(make_shared_constructs_in_place) {
    {
        auto p = my_std::make_shared<Pair>(3, "three");
        CHECK(p.use_count() == 1);
        CHECK(p->a == 3);
        CHECK(p->b == "three");
    }
    {
        auto p = my_std::make_shared<Tracked>(8);
        auto q = p;
        CHECK(p.use_count() == 2);
        CHECK(Tracked::live == 1);
    }
    CHECK(Tracked::live == 0);
}

TEST(shared_ptr_concurrent_copies) {
    my_std::shared_ptr<Tracked> p(new Tracked(1));
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([p] {
            for (int i = 0; i < 20000; i++) {
                my_std::shared_ptr<Tracked> local(p);
                CHECK(local->value == 1);
            }
        });
    }
    p = my_std::shared_ptr<Tracked>();  // main drops its own reference while threads run
    for (auto& t : threads) t.join();
    CHECK(Tracked::live == 0);
}

// ---------------------------------------------------------------- weak_ptr

TEST(weak_ptr_default_is_expired) {
    my_std::weak_ptr<int> w;
    CHECK(w.expired());
    CHECK(w.use_count() == 0);
    CHECK(!w.lock());
}

TEST(weak_ptr_does_not_own) {
    my_std::shared_ptr<Tracked> s(new Tracked(1));
    my_std::weak_ptr<Tracked> w(s);
    CHECK(s.use_count() == 1);
    CHECK(w.use_count() == 1);
    CHECK(!w.expired());
}

TEST(weak_ptr_lock_while_alive) {
    my_std::shared_ptr<Tracked> s(new Tracked(4));
    my_std::weak_ptr<Tracked> w(s);
    {
        auto locked = w.lock();
        CHECK(locked);
        CHECK(locked->value == 4);
        CHECK(s.use_count() == 2);
    }
    CHECK(s.use_count() == 1);
}

TEST(weak_ptr_expires_when_last_shared_dies) {
    my_std::weak_ptr<Tracked> w;
    {
        my_std::shared_ptr<Tracked> s(new Tracked(1));
        w = s;
        CHECK(!w.expired());
    }
    CHECK(Tracked::live == 0);  // object gone even though weak_ptr remains
    CHECK(w.expired());
    CHECK(w.use_count() == 0);
    CHECK(!w.lock());
}

TEST(weak_ptr_outlives_make_shared_object) {
    my_std::weak_ptr<Tracked> w;
    {
        auto s = my_std::make_shared<Tracked>(1);
        w = s;
    }
    CHECK(Tracked::live == 0);
    CHECK(w.expired());
}

TEST(weak_ptr_copy_and_move) {
    my_std::shared_ptr<Tracked> s(new Tracked(1));
    my_std::weak_ptr<Tracked> a(s);
    my_std::weak_ptr<Tracked> b(a);
    my_std::weak_ptr<Tracked> c(my_std::move(a));
    CHECK(a.expired());
    CHECK(!b.expired());
    CHECK(!c.expired());
    CHECK(b.use_count() == 1);

    my_std::weak_ptr<Tracked> d;
    d = b;
    my_std::weak_ptr<Tracked> e;
    e = my_std::move(c);
    CHECK(!d.expired());
    CHECK(!e.expired());
    CHECK(c.expired());
}

TEST(weak_ptr_self_assignment) {
    my_std::shared_ptr<Tracked> s(new Tracked(1));
    my_std::weak_ptr<Tracked> w(s);
    my_std::weak_ptr<Tracked>& ref = w;
    w = ref;
    w = my_std::move(ref);
    CHECK(!w.expired());
}

TEST(weak_ptr_reassign_releases_old_block) {
    my_std::shared_ptr<Tracked> s1(new Tracked(1));
    my_std::shared_ptr<Tracked> s2(new Tracked(2));
    my_std::weak_ptr<Tracked> w(s1);
    w = s2;
    CHECK(w.lock()->value == 2);
    s1 = my_std::shared_ptr<Tracked>();
    CHECK(!w.expired());  // w now tracks s2
}

TEST(shared_ptr_from_weak_ptr) {
    my_std::shared_ptr<Tracked> s(new Tracked(6));
    my_std::weak_ptr<Tracked> w(s);
    my_std::shared_ptr<Tracked> t(w);
    CHECK(t);
    CHECK(t.use_count() == 2);
    CHECK(t->value == 6);
}

TEST(shared_ptr_from_expired_weak_ptr_is_null) {
    my_std::weak_ptr<Tracked> w;
    {
        my_std::shared_ptr<Tracked> s(new Tracked(1));
        w = s;
    }
    my_std::shared_ptr<Tracked> t(w);
    CHECK(!t);
    CHECK(t.use_count() == 0);
}

namespace {
struct Base {
    virtual ~Base() = default;
    int base_value = 1;
};
struct Derived : Base {
    static inline int live = 0;
    Derived() { ++live; }
    ~Derived() override { --live; }
};
}  // namespace

TEST(weak_ptr_converts_derived_to_base) {
    {
        my_std::shared_ptr<Derived> d(new Derived);
        my_std::weak_ptr<Base> w(d);
        CHECK(!w.expired());
        auto locked = w.lock();
        CHECK(locked->base_value == 1);
    }
    CHECK(Derived::live == 0);
}

// ---------------------------------------------------------------- spsc_queue

TEST(spsc_pop_from_empty) {
    my_std::spsc_queue<int, 4> q;
    CHECK(!q.try_pop().has_value());
}

TEST(spsc_push_pop_single) {
    my_std::spsc_queue<int, 4> q;
    CHECK(q.try_push(7));
    auto v = q.try_pop();
    CHECK(v.has_value());
    CHECK(*v == 7);
    CHECK(!q.try_pop().has_value());
}

TEST(spsc_is_fifo) {
    my_std::spsc_queue<int, 8> q;
    for (int i = 0; i < 8; i++) CHECK(q.try_push(i));
    for (int i = 0; i < 8; i++) {
        auto v = q.try_pop();
        CHECK(v.has_value());
        CHECK(*v == i);
    }
}

TEST(spsc_holds_exactly_N_items) {
    my_std::spsc_queue<int, 4> q;
    for (int i = 0; i < 4; i++) CHECK(q.try_push(i));
    CHECK(!q.try_push(99));  // full
    CHECK(*q.try_pop() == 0);
    CHECK(q.try_push(99));   // room again
    CHECK(!q.try_push(100));
}

TEST(spsc_capacity_one) {
    my_std::spsc_queue<int, 1> q;
    CHECK(q.try_push(1));
    CHECK(!q.try_push(2));
    CHECK(*q.try_pop() == 1);
    CHECK(q.try_push(3));
    CHECK(*q.try_pop() == 3);
}

TEST(spsc_wraps_around) {
    my_std::spsc_queue<int, 3> q;
    for (int round = 0; round < 50; round++) {
        for (int i = 0; i < 3; i++) CHECK(q.try_push(round * 10 + i));
        for (int i = 0; i < 3; i++) CHECK(*q.try_pop() == round * 10 + i);
    }
    CHECK(!q.try_pop().has_value());
}

TEST(spsc_push_lvalue_and_rvalue) {
    my_std::spsc_queue<std::string, 4> q;
    std::string s = "copy me";
    CHECK(q.try_push(s));
    CHECK(s == "copy me");  // lvalue overload leaves source alone
    CHECK(q.try_push(std::string("temp")));
    CHECK(q.try_push(my_std::move(s)));
    CHECK(*q.try_pop() == "copy me");
    CHECK(*q.try_pop() == "temp");
    CHECK(*q.try_pop() == "copy me");
}

TEST(spsc_producer_consumer_threads) {
    constexpr int kCount = 1'000'000;
    my_std::spsc_queue<int, 1024> q;
    bool in_order = true;

    std::thread producer([&] {
        for (int i = 0; i < kCount; i++) {
            while (!q.try_push(i)) std::this_thread::yield();
        }
    });
    std::thread consumer([&] {
        for (int expected = 0; expected < kCount; expected++) {
            std::optional<int> v;
            while (!(v = q.try_pop())) std::this_thread::yield();
            if (*v != expected) { in_order = false; return; }
        }
    });
    producer.join();
    consumer.join();

    CHECK(in_order);
    CHECK(!q.try_pop().has_value());
}

// ---------------------------------------------------------------- runner

int main(int argc, char** argv) {
    std::string filter = argc > 1 ? argv[1] : "";
    if (filter == "--list") {
        for (auto& t : registry()) std::cout << t.name << "\n";
        return 0;
    }

    int ran = 0, failed = 0;
    for (auto& t : registry()) {
        if (!filter.empty() && std::string(t.name).find(filter) == std::string::npos) continue;

        int before = g_failures;
        Tracked::live = 0;
        Derived::live = 0;
        t.fn();
        if (Tracked::live != 0 || Derived::live != 0) {
            ++g_failures;
            std::cout << "    FAIL leaked or double-destroyed objects (Tracked live=" << Tracked::live
                      << ", Derived live=" << Derived::live << ")\n";
        }

        ++ran;
        bool ok = g_failures == before;
        if (!ok) ++failed;
        std::cout << (ok ? "[ ok ] " : "[FAIL] ") << t.name << "\n";
    }

    std::cout << "\n" << (ran - failed) << "/" << ran << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
