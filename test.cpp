#include "Optional.hpp"
#include <type_traits>

using namespace slm;
using namespace std;

/*********************************************************************************
 * Compile-time tests - type traits, exception specifications, and constexpr-ness
 *********************************************************************************/

static_assert(is_trivially_copy_constructible_v<Optional<int>>);

static_assert(conjunction_v<is_trivially_copy_constructible<Optional<int>>,
                            is_trivially_copy_assignable<Optional<int>>,
                            is_trivially_move_constructible<Optional<int>>,
                            is_trivially_move_assignable<Optional<int>>,
                            is_trivially_destructible<Optional<int>>>);

// clang at least pre-16.0.0 is incorrect for these traits.
#ifndef __clang__
static_assert(is_trivially_copyable_v<Optional<int>>);
#endif

struct TriviallyMovableButNotCopyAssignable
{
  TriviallyMovableButNotCopyAssignable(const TriviallyMovableButNotCopyAssignable&) = default;
  TriviallyMovableButNotCopyAssignable(TriviallyMovableButNotCopyAssignable&&) = default;
  TriviallyMovableButNotCopyAssignable& operator=(TriviallyMovableButNotCopyAssignable&&) =
    default;
  TriviallyMovableButNotCopyAssignable& operator=(const TriviallyMovableButNotCopyAssignable&)
  {
    return *this;
  }
};
static_assert(
  is_trivially_move_constructible_v<Optional<TriviallyMovableButNotCopyAssignable>>);
static_assert(is_trivially_move_assignable_v<Optional<TriviallyMovableButNotCopyAssignable>>);
static_assert(is_trivially_destructible_v<Optional<TriviallyMovableButNotCopyAssignable>>);
static_assert(
  is_trivially_copy_constructible_v<Optional<TriviallyMovableButNotCopyAssignable>>);
static_assert(!is_trivially_copy_assignable_v<Optional<TriviallyMovableButNotCopyAssignable>>);

#ifndef __clang__
static_assert(!is_trivially_copyable_v<Optional<TriviallyMovableButNotCopyAssignable>>);
#endif

struct TriviallyCopyableButNotMoveAssignable
{
  TriviallyCopyableButNotMoveAssignable(const TriviallyCopyableButNotMoveAssignable&) = default;
  TriviallyCopyableButNotMoveAssignable(TriviallyCopyableButNotMoveAssignable&&) = default;
  TriviallyCopyableButNotMoveAssignable& operator=(TriviallyCopyableButNotMoveAssignable&&) =
    delete;
  TriviallyCopyableButNotMoveAssignable& operator=(
    const TriviallyCopyableButNotMoveAssignable&) = default;
};
static_assert(!is_move_assignable_v<TriviallyCopyableButNotMoveAssignable>);
static_assert(
  is_trivially_move_constructible_v<Optional<TriviallyCopyableButNotMoveAssignable>>);
static_assert(
  is_trivially_copy_constructible_v<Optional<TriviallyCopyableButNotMoveAssignable>>);
static_assert(is_trivially_destructible_v<Optional<TriviallyCopyableButNotMoveAssignable>>);
static_assert(is_trivially_copy_assignable_v<Optional<TriviallyCopyableButNotMoveAssignable>>);

#ifndef __clang__
static_assert(is_trivially_copyable_v<TriviallyCopyableButNotMoveAssignable>);
// I think this should be true but it does not pass.
// The non-trivial move constructor (that won't compile because
// TriviallyCopyableButNotMoveAssignable has its move assignment operator deleted) prevents this
// trait from being true.
// static_assert(is_trivially_copyable_v<Optional<TriviallyCopyableButNotMoveAssignable>>);
#endif

struct TriviallyCopyAndMoveConstructButNotAssign
{
  typedef TriviallyCopyAndMoveConstructButNotAssign This;
  TriviallyCopyAndMoveConstructButNotAssign(const This&) = default;
  TriviallyCopyAndMoveConstructButNotAssign(This&&) = default;
  This& operator=(const This&);
  This& operator=(This&&);
};

static_assert(is_trivially_copy_constructible_v<TriviallyCopyAndMoveConstructButNotAssign>);
static_assert(is_trivially_move_constructible_v<TriviallyCopyAndMoveConstructButNotAssign>);
static_assert(!is_trivially_copy_assignable_v<TriviallyCopyAndMoveConstructButNotAssign>);
static_assert(!is_trivially_move_assignable_v<TriviallyCopyAndMoveConstructButNotAssign>);
static_assert(!is_trivially_copyable_v<TriviallyCopyAndMoveConstructButNotAssign>);

/*
 * constexpr tests
 */

void
constexpr_test()
{
  // Construction from Some
  constexpr Optional<int> optional = Some(10);
  static_assert(optional.has_value());
  static_assert((bool)optional);
  static_assert(*optional == 10);
  // Copy assign
  constexpr Optional<int> b = optional;
  static_assert(*b == 10);
  static_assert(b.has_value());

  // value_or
  static_assert(optional.value_or(1) == 10);
  // and_then
  static_assert(
    optional.and_then([](const auto& arg) { return Optional(Some(arg * 2)); }).value() == 20);
  static_assert(*optional == 10);

  // transform, compare to heterogeneous type
  static_assert(optional.transform([](int arg) -> double { return arg * 2; }) == 20);
  // or_else, compare to Optional
  static_assert(optional.or_else([]() -> Optional<int> { return Some(2); }) == optional);
  // compare to nullopt
  static_assert(optional != None);

  // Ordering tests
  static_assert(b < 20);
  static_assert(b <= Optional(Some(20.0)));
  static_assert(b > 9.8);
  static_assert(b == Optional(Some(10)));
  static_assert(b < b.transform([](auto arg) { return arg + 1; }));

  constexpr auto lambda1 = []() {
    Optional<int> opt = None;
    int x = 2;
    Optional opt2 = SomeRef(x);
    *opt2 += 1;
    opt = Some(*opt2);
    return opt;
  };

  constexpr auto c = lambda1();
  static_assert(c == 3);
  static_assert(4 > c);
  static_assert(None < c);
  static_assert(c >= None);

  constexpr auto lambda2 = []() -> Optional<int> {
    int a = 1;
    int b = 2;
    Optional opt = SomeRef(b);
    *opt *= 2;
    opt = a;
    *opt *= 2;
    return Some(a + b);
  };
  static_assert(lambda2() == 6);
}

struct Int
{
  inline static size_t constructed = 0;
  inline static size_t destroyed = 0;

  int value;
  enum Flags : int
  {
    DEFAULT_CONSTRUCTED = 0x1,
    COPY_CONSTRUCTED = 0x2,
    MOVE_CONSTRUCTED = 0x4,
    VALUE_CONSTRUCTED = 0x8,
    COPY_ASSIGNED = 0x10,
    MOVE_ASSIGNED = 0x20,
    COPIED_FROM = 0x40,
    MOVED_FROM = 0x80,
    VALUE_ASSIGNED = 0x100
  };

  mutable int flags = DEFAULT_CONSTRUCTED;

  Int() { constructed++; }
  Int(int x)
    : value{ x }
    , flags{ VALUE_CONSTRUCTED }
  {
    constructed++;
  }
  Int(const Int& other)
    : value{ other.value }
    , flags{ COPY_CONSTRUCTED }
  {
    constructed++;
    other.flags |= COPIED_FROM;
  }
  Int(Int&& other)
    : value{ other.value }
    , flags{ MOVE_CONSTRUCTED }
  {
    constructed++;
    other.flags |= MOVED_FROM;
  }
  Int& operator=(const Int& other)
  {
    flags |= COPY_ASSIGNED;
    value = other.value;
    other.flags |= COPIED_FROM;
    return *this;
  }
  Int& operator=(Int&& other)
  {
    flags |= MOVE_ASSIGNED;
    value = other.value;
    other.flags |= MOVED_FROM;
    return *this;
  }
  ~Int() { destroyed++; }
  Int& operator=(int x)
  {
    flags |= VALUE_ASSIGNED;
    value = x;
    return *this;
  }

  bool operator==(const Int& other) const { return value == other.value; }
  auto operator<=>(const Int& other) const { return value <=> other.value; }

  friend Int operator+(const Int& a, const Int& b) { return a.value + b.value; }
};

constexpr auto const_detector = [](auto&& arg) -> Optional<std::string_view> {
  return Some(is_const_v<std::remove_reference_t<decltype(arg)>> ? "const" : "not const");
};

int
main()
{
  constexpr_test();

  {
    Optional<Int> a = Some(1);
    static_assert(!std::is_trivially_copy_constructible_v<Optional<Int>>);
    REQUIRE(a.value() == 1);
    REQUIRE(a == 1);
    REQUIRE(*a == 1);
    REQUIRE(a->flags & Int::VALUE_CONSTRUCTED);

    Optional<Int> b = Some(*a);
    REQUIRE(b.value() == 1);
    REQUIRE(*b == 1);
    REQUIRE(b->flags & Int::COPY_CONSTRUCTED);

    Optional<Int> c = Some(std::move(b).value());
    REQUIRE(c == 1);
    REQUIRE(c->flags & Int::MOVE_CONSTRUCTED);
    REQUIRE(b->flags & Int::MOVED_FROM);

    Optional<Int> d = c;
    REQUIRE(d < 2);
    REQUIRE(d > None);
    REQUIRE(d <= 3);
    REQUIRE(d <= 1);
    REQUIRE(d >= 1);
    REQUIRE(None < d);
    REQUIRE(d->flags & Int::COPY_CONSTRUCTED);

    auto e = d.transform([](Int x) { return x + 1; });
    static_assert(std::is_same_v<decltype(e), Optional<Int>>);
    // Important: the result in 'transform' should be materialized directly into the optional.
    REQUIRE(e->flags & Int::VALUE_CONSTRUCTED);
    REQUIRE(e == 2);

    Optional<Int> f = std::move(d);
    REQUIRE(f == 1);
    REQUIRE(d->flags & Int::MOVED_FROM);
  }

  {
    Optional<Int> empty;
    Optional<Int> a = Some(1);
    auto b = (a = empty).and_then([](const Int& x) -> Optional<Int> { return Some(x + 1); });
    REQUIRE(b == None);
    REQUIRE(b >= None);
    REQUIRE(b <= None);
    REQUIRE(a == b);

    Optional<Int> c = None;
    REQUIRE(c == b);
    Optional<Int> d(c);
    REQUIRE(c == None);
    Optional<Int> e(std::move(d));
    REQUIRE(e == None);
  }

  {
    Optional<Int> x(std::in_place, 5);
    REQUIRE(x.has_value());
    REQUIRE(*x == 5);
    auto y = x.and_then([](auto v) -> Optional<Int> { return Some(v + 1); });
    REQUIRE(y == 6);
    REQUIRE(y->flags & Int::MOVE_CONSTRUCTED);
    y = None;
    REQUIRE(y == None);
    REQUIRE(y != x);
    y = x;
    REQUIRE(y == 5);
    REQUIRE(y->flags == Int::COPY_CONSTRUCTED);
    auto z = (x = None).or_else([]() -> Optional<Int> { return Some(10); });
    REQUIRE(z == 10);
    REQUIRE(z->flags == Int::VALUE_CONSTRUCTED);
    y = z;
    REQUIRE(y == 10);
    REQUIRE(y->flags & Int::COPY_CONSTRUCTED);
    REQUIRE(y == z);
    REQUIRE(y != x);
    REQUIRE(y != Optional<Int>(std::in_place, 9));
    REQUIRE(!x);

    x = std::move(y);
    REQUIRE(y.has_value());
    REQUIRE(y->flags & Int::MOVED_FROM);
    REQUIRE(x == 10);
    z = std::move(x);
    REQUIRE(x->flags & Int::MOVED_FROM);
    REQUIRE(z->flags & Int::MOVE_ASSIGNED);
    REQUIRE(z == 10);
    x = y = None;
    x = std::move(z);
    REQUIRE(x->flags & Int::MOVE_CONSTRUCTED);

    REQUIRE(x.and_then(const_detector) == "not const");
    const Optional<Int> x2(x);
    REQUIRE(x2.and_then(const_detector) == "const");
    REQUIRE(x2->flags & Int::COPY_CONSTRUCTED);
    REQUIRE(std::move(x).and_then(const_detector) == "not const");
    REQUIRE(std::move(x2).and_then(const_detector) == "const");

    x = y = None;
    REQUIRE(!(x || y));
    REQUIRE(x == y);
    REQUIRE(x >= y);
    REQUIRE(x <= y);
    REQUIRE(z > x);
    REQUIRE(z <= z);
    REQUIRE(z >= z);
    REQUIRE(z == z);
    z = std::move(x);
    REQUIRE(z == None);
  }

  {
    Optional<Int> x = Some(2);
    REQUIRE(x->flags & Int::VALUE_CONSTRUCTED);
    x = Some(3);
    REQUIRE(x == 3);
    REQUIRE(x->flags & Int::VALUE_ASSIGNED);
    x = None;
    x = Some(4);
    REQUIRE(x->flags & Int::VALUE_CONSTRUCTED);
    REQUIRE(*x == 4);
    Int y = 5;
    x = Some(y);
    REQUIRE(x == 5);
    REQUIRE(x->flags & Int::COPY_ASSIGNED);
    x = None;
    x = Some(std::move(y));
    REQUIRE(x == 5);
    REQUIRE(x->flags & Int::MOVE_CONSTRUCTED);
    REQUIRE(y.flags & Int::MOVED_FROM);
    Optional<Int> z;
    REQUIRE(!z);

    z = Some(*x);
    REQUIRE(z == 5);
    REQUIRE(z->flags & Int::COPY_CONSTRUCTED);
    z = None;

    REQUIRE(Int(*(z.or_else([]() -> Optional<Int> { return Some(5); }))).flags &
            Int::MOVE_CONSTRUCTED);

    z = Some(1);
    REQUIRE(Int(*(z.transform([](auto&& arg) -> Int { return arg + 1; }))).flags &
            Int::MOVE_CONSTRUCTED);

    constexpr auto helper = []() -> const Optional<Int> { return Some(5); };
    REQUIRE(Int(*helper()).flags & Int::COPY_CONSTRUCTED);

    REQUIRE(const_detector(*helper()) == "const");
    REQUIRE(const_detector(helper().value()) == "const");
    REQUIRE(const_detector(Optional(Some(*helper())).value()) == "not const");
  }

  {
    Optional<Int> x(Some(1));
    REQUIRE(x->flags & Int::VALUE_CONSTRUCTED);
    auto moving_fn = [&](auto&&) -> decltype(auto) { return std::move(x); };
    auto transformed = x.transform(moving_fn);
    static_assert(std::is_same_v<decltype(transformed), Optional<Optional<Int>>>);
    REQUIRE((*transformed)->flags & Int::MOVE_CONSTRUCTED);
    auto transformed2 =
      transformed.transform([](Optional<Int>& opt) -> decltype(auto) { return *opt; });
    static_assert(std::is_same_v<decltype(transformed2), Optional<Int&>>);
    REQUIRE(transformed2->flags & Int::MOVE_CONSTRUCTED);
  }

  {
    const Optional<Int> x(Some(1));
    auto transformer = [](auto&& n) -> decltype(auto) { return n + 1; };
    static_assert(std::is_same_v<std::invoke_result_t<decltype(transformer), const Int&>, Int>);
    auto transformed = x.transform(transformer);
    static_assert(std::is_same_v<decltype(transformed), Optional<Int>>);
    REQUIRE(transformed->flags & Int::VALUE_CONSTRUCTED);

    auto return_rvalue = [](auto&& n) -> decltype(auto) { return std::move(n); };
    static_assert(
      std::is_same_v<std::invoke_result_t<decltype(return_rvalue), const Int&>, const Int&&>);
    auto transformed2 = x.transform(return_rvalue);
    static_assert(std::is_same_v<decltype(transformed2), Optional<Int>>);
    REQUIRE(transformed2->flags & Int::COPY_CONSTRUCTED);
    REQUIRE(x == transformed2);
    auto transformed3 = x.transform(std::identity{});
    static_assert(std::is_same_v<decltype(transformed3), Optional<const Int&>>);
    REQUIRE(transformed3.operator->() == x.operator->());

    transformed = std::move(x).transform(std::identity{});
    REQUIRE(!(x->flags & Int::MOVED_FROM));
    REQUIRE(transformed == x);
    transformed3 = std::move(x).transform([](auto& arg) -> decltype(auto) { return arg; });
    REQUIRE(transformed3 == x);
    transformed2 = std::move(x).transform(transformer);
    REQUIRE(*transformed2 == 2);
    REQUIRE(transformed2->flags & Int::MOVE_ASSIGNED);
  }

  {
    Optional<Int&> x = None;
    REQUIRE(!x.has_value());
    Int y = 77;
    Int z = 88;
    REQUIRE(x.value_or(z) == z);
    x = SomeRef(y);
    REQUIRE(x.has_value() && x.value() == 77);
    REQUIRE(x.value_or(z) == y);
    REQUIRE(&x.value_or(z) == &y);

    x = SomeRef(z);
    REQUIRE(&x.value() == &z);
    REQUIRE(x == z);
    REQUIRE(x != y);
    REQUIRE(y < x);
    REQUIRE(None < x);
    *x = 99;
    REQUIRE(z == 99);

    x = None;
    REQUIRE(!x.has_value());
    REQUIRE(z == 99 && y == 77);

    {
      auto result = x.and_then([](const Int& z) -> Optional<Int> { return Some(z + 1); });
      REQUIRE(!result);
      x = Optional<Int&>(SomeRef(z));
      auto result2 = x.and_then([](Int& z) -> Optional<Int&> {
        z = z + 1;
        return SomeRef(z);
      });
      REQUIRE(z == 100);
      REQUIRE(x == z);
      REQUIRE(&(*x) == &(*result2));

      // Referencing a variable in outer scope should work.
      auto result3 = x.and_then([&](const Int&) -> Optional<const Int&> { return SomeRef(y); });
      REQUIRE(result3 == y);
      // Trying to reference a temporary should error.
      /* result3 =
        x.and_then([&](const Int& z) -> Optional<const Int&> { return SomeRef(z + 1); }); */
      result3 = None;
      REQUIRE(result3.value_or(*x) == x);
      result3 = x;
      REQUIRE(result3.transform([](const Int& x) { return x; })->flags & Int::COPY_CONSTRUCTED);
      REQUIRE(Optional<const Int&>().transform(std::identity{}) == None);
      REQUIRE(x.transform([](Int& z) -> decltype(auto) { return (z = z + 1); }) == x);
      REQUIRE(x == 101);
      REQUIRE(z == 101);
      Optional<Int> a = x.transform([&z](Int& i) -> Int&& {
        z = z + i;
        return std::move(z);
      });
      REQUIRE(a->flags & Int::MOVE_CONSTRUCTED);
      REQUIRE(a == 202);
      REQUIRE(z.flags & Int::MOVED_FROM);

      REQUIRE(x->flags & Int::MOVED_FROM);
      x = SomeRef(*a);
      REQUIRE(&(*x) == &(*a));

      REQUIRE(x.or_else([]() { return Optional<Int&>(); }) == x);
      result3 = None;
      result3 = result3.or_else([&a]() -> Optional<Int&> { return SomeRef(*a); });
      REQUIRE(result3 == a);
      REQUIRE(&(*result3) == &(*a));
    }

    Optional<const Int&> x2 = x;
    REQUIRE(&(*x2) == &(*x));
  }

  REQUIRE(Int::constructed == Int::destroyed,
          "Constructed: {:d}; destroyed: {:d}",
          Int::constructed,
          Int::destroyed);
}
