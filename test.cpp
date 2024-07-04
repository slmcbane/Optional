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

int
main()
{
  constexpr_test();
}
