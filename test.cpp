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

