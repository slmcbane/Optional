#ifndef SLM_OPTIONAL_HPP
#define SLM_OPTIONAL_HPP

#include <compare>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "slm_assert.hpp"

namespace slm {

/*
 * Some is a proxy reference type to be used only for initializing an Optional.
 * It always forms a reference to the value passed to it and uses other than
 * immediate initialization of an Optional are undefined.
 */
template<class T>
class Some;

template<class T>
class Some<const T&>
{
public:
  constexpr Some(const T& x) noexcept
    : m_ref{ x }
  {
  }
  constexpr const T& unwrap() const noexcept { return m_ref; }

private:
  const T& m_ref;
};

template<class T>
class Some<T&&>
{
public:
  constexpr Some(T&& x) noexcept
    : m_ref{ x }
  {
  }
  constexpr T&& unwrap() const noexcept { return static_cast<T&&>(m_ref); }

private:
  T& m_ref;
};

/*
 * SomeRef is similar, but denotes that an optional should contain a reference rather
 * than the object itself. SomeRef always wraps an lvalue reference.
 */
template<class T>
class SomeRef
{
public:
  constexpr SomeRef(T& r) noexcept
    : m_ref{ r }
  {
  }

  SomeRef(T&&) = delete;

  constexpr T& unwrap() const noexcept { return m_ref; }

private:
  T& m_ref;
};

template<class T>
Some(T&& x) -> Some<T&&>;

/*
 * For non-const lvalue reference we construct a Some<const T&>, because the appropriate
 * constructor for Optional is the one that invokes copy-construction on T.
 */
template<class T>
Some(T& x) -> Some<const T&>;

template<class T>
Some(const T& x) -> Some<const T&>;

/*
 * Equivalent to std::nullopt_t and std::nullopt. I prefer to spell this as 'None'.
 */
struct NoneType
{};

inline constexpr NoneType None;

namespace optional_detail {

template<class T>
struct is_some : std::false_type
{};

template<class T>
struct is_some<Some<T>> : std::true_type
{};

/*
 * gcc errors saying requires clauses are different on the different declarations of Optional
 * unless we wrap the conditions in a concept. Interestingly, clang accepts it without the
 * concept.
 */
template<class T>
concept AllowedOptional =
  std::negation_v<std::disjunction<std::is_same<T, NoneType>, is_some<T>>>;

} // namespace optional_detail

template<optional_detail::AllowedOptional T>
class Optional;

namespace optional_detail {

/*
 * These concepts are useful for defaulting copy/move assignment operators that should be
 * trivial.
 */
template<class T>
concept trivially_copy_assignable =
  std::is_trivially_copy_assignable_v<T> && std::is_trivially_copy_constructible_v<T> &&
  std::is_trivially_destructible_v<T>;

template<class T>
concept trivially_move_assignable =
  std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
  std::is_trivially_destructible_v<T>;

template<class T>
constexpr inline bool is_optional = false;

template<class T>
constexpr inline bool is_optional<Optional<T>> = true;

/*
 * This concept is needed as a constraint on the comparison operators for non-optional type in
 * order to avoid a circular concept evaluation.
 */
template<class T>
concept NotOptional = !is_optional<std::remove_cvref_t<T>>;

template<class F, class T>
using bare_result_type = std::remove_cvref_t<std::invoke_result_t<F, T>>;

template<class F, class T>
INLINE constexpr bare_result_type<F, T>
maybe_invoke(F&& f, T&& arg, bool cond)
{
  if (cond) {
    return std::invoke(std::forward<F>(f), std::forward<T>(arg));
  } else {
    return bare_result_type<F, T>{};
  }
}

} // namespace optional_detail

template<optional_detail::AllowedOptional T>
class Optional
{
public:
  constexpr Optional() noexcept = default;
  constexpr Optional(NoneType) noexcept {}

  /*
   * Note the use of requires to make copy/move construction/assignment trivial if T is
   * trivially copy/move constructible/assignable.
   */
  constexpr Optional(const Optional&)
    requires std::is_trivially_copy_constructible_v<T>
  = default;

  constexpr Optional(const Optional& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
  {
    if (other.m_engaged) {
      std::construct_at(&m_payload, other.m_payload);
      m_engaged = true;
    }
  }

  constexpr Optional(Optional&&)
    requires std::is_trivially_move_constructible_v<T>
  = default;

  constexpr Optional(Optional&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
  {
    if (other.m_engaged) {
      std::construct_at(&m_payload, std::move(other.m_payload));
      m_engaged = true;
    }
  }

  template<class... Args>
  constexpr explicit Optional(std::in_place_t, Args&&... args)
    : m_payload(std::forward<Args>(args)...)
    , m_engaged{ true }
  {
  }

  template<class U>
  constexpr Optional(Some<const U&> x)
    : m_payload(x.unwrap())
    , m_engaged{ true }
  {
  }

  template<class U>
  constexpr Optional(Some<U&&> x)
    : m_payload(x.unwrap())
    , m_engaged{ true }
  {
  }

  constexpr ~Optional()
    requires std::is_trivially_destructible_v<T>
  = default;

  constexpr ~Optional() noexcept(std::is_nothrow_destructible_v<T>)
  {
    if (m_engaged) {
      m_payload.~T();
    }
  }

  constexpr Optional& operator=(NoneType) noexcept(std::is_nothrow_destructible_v<T>)
  {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (m_engaged) {
        m_payload.~T();
      }
    }
    m_engaged = false;
  }

  constexpr Optional& operator=(const Optional&)
    requires optional_detail::trivially_copy_assignable<T>
  = default;

  constexpr Optional& operator=(const Optional& other) noexcept(
    std::conjunction_v<std::is_nothrow_copy_constructible<T>,
                       std::is_nothrow_copy_assignable<T>,
                       std::is_nothrow_destructible<T>>)
  {
    if (m_engaged && other.m_engaged) {
      m_payload = other.m_payload;
    } else if (m_engaged) {
      m_payload.~T();
      m_engaged = false;
    } else if (other.m_engaged) {
      std::construct_at(&m_payload, other.m_payload);
      m_engaged = true;
    }
    return *this;
  }

  constexpr Optional& operator=(Optional&&)
    requires optional_detail::trivially_move_assignable<T>
  = default;

  constexpr Optional& operator=(Optional&& other) noexcept(
    std::conjunction_v<std::is_nothrow_move_assignable<T>,
                       std::is_nothrow_move_constructible<T>,
                       std::is_nothrow_destructible<T>>)
  {
    if (m_engaged && other.m_engaged) {
      m_payload = static_cast<T&&>(other.m_payload);
    } else if (m_engaged) {
      m_payload.~T();
      m_engaged = false;
    } else if (other.m_engaged) {
      std::construct_at(&m_payload, static_cast<T&&>(other.m_payload));
      m_engaged = true;
    }
    return *this;
  }

  template<class U>
  constexpr Optional& operator=(Some<const U&> x)
  {
    if (m_engaged) {
      m_payload = x.unwrap();
    } else {
      std::construct_at(&m_payload, x.unwrap());
      m_engaged = true;
    }
    return *this;
  }

  template<class U>
  constexpr Optional& operator=(Some<U&&> x)
  {
    if (m_engaged) {
      m_payload = x.unwrap();
    } else {
      std::construct_at(&m_payload, x.unwrap());
      m_engaged = true;
    }
    return *this;
  }

  constexpr const T* operator->() const noexcept
  {
    CHECK(m_engaged, "dereferencing disengaged Optional");
    return &m_payload;
  }

  constexpr T* operator->() noexcept
  {
    CHECK(m_engaged, "dereferencing disengaged Optional");
    return &m_payload;
  }

  constexpr const T& operator*() const& noexcept
  {
    CHECK(m_engaged, "dereferencing disengaged Optional");
    return m_payload;
  }

  constexpr T& operator*() & noexcept
  {
    CHECK(m_engaged, "dereferencing disengaged Optional");
    return m_payload;
  }

  constexpr const T&& operator*() const&& noexcept
  {
    CHECK(m_engaged, "dereferencing disengaged Optional");
    return static_cast<const T&&>(m_payload);
  }

  constexpr T&& operator*() && noexcept
  {
    CHECK(m_engaged, "dereferencing disengaged Optional");
    return static_cast<T&&>(m_payload);
  }

  constexpr explicit operator bool() const noexcept { return m_engaged; }

  constexpr bool has_value() const noexcept { return m_engaged; }

  constexpr const T& value() const&
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return m_payload;
  }

  constexpr T& value() &
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return m_payload;
  }

  constexpr T&& value() &&
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return static_cast<T&&>(m_payload);
  }

  constexpr const T&& value() const&&
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return static_cast<const T&&>(m_payload);
  }

  template<class U>
  constexpr T value_or(U&& default_value) const&
  {
    return m_engaged ? m_payload : static_cast<T>(std::forward<U>(default_value));
  }

  template<class U>
  constexpr T value_or(U&& default_value) &&
  {
    return m_engaged ? std::move(m_payload) : static_cast<T>(std::forward<U>(default_value));
  }

  template<class F>
  constexpr auto and_then(F&& f) &
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, T&>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), m_payload, m_engaged);
  }

  template<class F>
  constexpr auto and_then(F&& f) const&
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, const T&>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), m_payload, m_engaged);
  }

  template<class F>
  constexpr auto and_then(F&& f) &&
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, T>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), std::move(m_payload), m_engaged);
  }

  template<class F>
  constexpr auto and_then(F&& f) const&&
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, const T>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), std::move(m_payload), m_engaged);
  }

  // TODO: Make sure to test this with the passed callable returning an r-value reference.
  template<class F>
  constexpr auto transform(F&& f) &
  {
    using result_type = std::invoke_result_t<F, T&>;
    if constexpr (std::is_rvalue_reference_v<result_type>) {
      return m_engaged
               ? Optional<std::remove_cvref_t<result_type>>(std::forward<F>(f), m_payload)
               : None;
    } else if constexpr (std::is_reference_v<result_type>) {
      return m_engaged ? Optional(SomeRef(std::invoke(std::forward<F>(f), m_payload))) : None;
    } else {
      return m_engaged ? Optional<result_type>(std::forward<F>(f), m_payload) : None;
    }
  }

  template<class F>
  constexpr auto transform(F&& f) const&
  {
    using result_type = std::invoke_result_t<F, const T&>;
    if constexpr (std::is_rvalue_reference_v<result_type>) {
      return m_engaged
               ? Optional<std::remove_cvref_t<result_type>>(std::forward<F>(f), m_payload)
               : None;
    } else if constexpr (std::is_reference_v<result_type>) {
      return m_engaged ? Optional(SomeRef(std::invoke(std::forward<F>(f), m_payload))) : None;
    } else {
      return m_engaged ? Optional<result_type>(std::forward<F>(f), m_payload) : None;
    }
  }

  template<class F>
  constexpr auto transform(F&& f) &&
  {
    using result_type = std::invoke_result_t<F, T&&>;
    if constexpr (std::is_rvalue_reference_v<result_type>) {
      return m_engaged ? Optional<std::remove_cvref_t<result_type>>(std::forward<F>(f),
                                                                    std::move(m_payload))
                       : None;
    } else if constexpr (std::is_reference_v<result_type>) {
      return m_engaged
               ? Optional(SomeRef(std::invoke(std::forward<F>(f), std::move(m_payload))))
               : None;
    } else {
      return m_engaged ? Optional<result_type>(std::forward<F>(f), std::move(m_payload)) : None;
    }
  }

  template<class F>
  constexpr auto transform(F&& f) const&&
  {
    using result_type = std::invoke_result_t<F, const T&&>;
    if constexpr (std::is_rvalue_reference_v<result_type>) {
      return m_engaged ? Optional<std::remove_cvref_t<result_type>>(std::forward<F>(f),
                                                                    std::move(m_payload))
                       : None;
    } else if constexpr (std::is_reference_v<result_type>) {
      return m_engaged
               ? Optional(SomeRef(std::invoke(std::forward<F>(f), std::move(m_payload))))
               : None;
    } else {
      return m_engaged ? Optional<result_type>(std::forward<F>(f), std::move(m_payload)) : None;
    }
  }

  template<class F>
  constexpr Optional or_else(F&& f) const&
    requires std::conjunction_v<
      std::is_same<std::remove_cvref_t<std::invoke_result_t<F>>, Optional<T>>,
      std::is_copy_constructible<T>,
      std::is_invocable<F>>
  {
    return m_engaged ? *this : std::invoke(std::forward<F>(f));
  }

  template<class F>
  constexpr Optional or_else(F&& f) &&
    requires std::conjunction_v<
      std::is_same<std::remove_cvref_t<std::invoke_result_t<F>>, Optional<T>>,
      std::is_move_constructible<T>,
      std::is_invocable<F>>
  {
    return m_engaged ? std::move(*this) : std::invoke(std::forward<F>(f));
  }

  constexpr void swap(Optional& other) noexcept(
    std::conjunction_v<std::is_nothrow_move_constructible<T>,
                       std::is_nothrow_swappable<T>,
                       std::is_nothrow_destructible<T>>)
  {
    using std::swap;
    if (m_engaged) {
      if (other.m_engaged) {
        swap(m_payload, other.m_payload);
      } else {
        std::construct_at(&other.m_payload, std::move(m_payload));
        other.m_engaged = true;
        m_payload.~T();
        m_engaged = false;
      }
    } else if (other.m_engaged) {
      std::construct_at(&m_payload, std::move(other.m_payload));
      m_engaged = true;
      other.m_payload.~T();
      other.m_engaged = false;
    }
  }

  friend void swap(Optional& a, Optional& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }

  constexpr void reset() noexcept
  {
    if (m_engaged) {
      m_payload.~T();
      m_engaged = false;
    }
  }

  template<class... Args>
  constexpr T& emplace(Args&&... args)
  {
    reset();
    std::construct_at(&m_payload, std::forward<Args>(args)...);
    m_engaged = true;
    return m_payload;
  }

private:
  union
  {
    T m_payload;
  };
  bool m_engaged{ false };

  template<optional_detail::AllowedOptional U>
  friend class Optional;

  template<class F, class U>
  constexpr explicit Optional(F&& f, U&& u)
    : m_payload(std::invoke(std::forward<F>(f), std::forward<U>(u)))
    , m_engaged{ true }
  {
  }
};

template<optional_detail::AllowedOptional T>
class Optional<T&>
{
public:
  constexpr Optional() = default;
  constexpr Optional(const Optional&) = default;
  constexpr Optional(Optional&&) = default;
  constexpr Optional(NoneType) noexcept {}

  constexpr Optional(SomeRef<T> ref) noexcept
    : m_ptr{ &ref.unwrap() }
  {
  }

  constexpr Optional& operator=(NoneType) noexcept { m_ptr = nullptr; }
  constexpr Optional& operator=(const Optional& other) = default;
  constexpr Optional& operator=(Optional&& other) = default;
  constexpr Optional& operator=(SomeRef<T> ref) noexcept
  {
    m_ptr = &ref.unwrap();
    return *this;
  }

  constexpr T& operator*() const noexcept
  {
    CHECK(m_ptr, "Disengaged optional access");
    return *m_ptr;
  }

  constexpr T* operator->() const noexcept
  {
    CHECK(m_ptr, "Disengaged optional access");
    return m_ptr;
  }

  constexpr explicit operator bool() const noexcept { return m_ptr != nullptr; }

  constexpr bool has_value() const noexcept { return (bool)*this; }

  constexpr T& value() const
  {
    REQUIRE(m_ptr, "Disengaged optional access");
    return *m_ptr;
  }

  constexpr T& value_or(T& default_value) const noexcept
  {
    return m_ptr ? *m_ptr : default_value;
  }

  template<class F>
  constexpr auto and_then(F&& f) const
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, T&>>
  {
    return m_ptr ? std::invoke(std::forward<F>(f), *m_ptr)
                 : optional_detail::bare_result_type<F, T&>();
  }

  template<class F>
  constexpr auto transform(F&& f) const
  {
    using result_type = std::invoke_result_t<F, T&>;
    if constexpr (std::is_rvalue_reference_v<result_type>) {
      return m_ptr ? Optional<std::remove_cvref_t<result_type>>(std::forward<F>(f), *m_ptr)
                   : None;
    } else if constexpr (std::is_reference_v<result_type>) {
      return m_ptr ? Optional(SomeRef(std::invoke(std::forward<F>(f), *m_ptr))) : None;
    } else {
      return m_ptr ? Optional<result_type>(std::forward<F>(f), *m_ptr) : None;
    }
  }

  template<class F>
  constexpr Optional or_else(F&& f) const
    requires std::conjunction_v<
      std::is_same<optional_detail::bare_result_type<F, T&>, Optional<T&>>,
      std::is_invocable<F>>
  {
    return m_ptr ? *this : std::invoke(std::forward<F>(f));
  }

  constexpr void swap(Optional& other) noexcept { std::swap(m_ptr, other.m_ptr); }
  friend void swap(Optional& a, Optional& b) noexcept { a.swap(b); }

  constexpr void reset() noexcept { m_ptr = nullptr; }

private:
  T* m_ptr{ nullptr };
};

/******************************************************************************************
 * Deduction guides
 *****************************************************************************************/
template<class T>
Optional(Some<const T&>) -> Optional<T>;

template<class T>
Optional(Some<T&&>) -> Optional<T>;

template<class T>
Optional(SomeRef<T>) -> Optional<T&>;

template<class T>
Optional(SomeRef<const T>) -> Optional<const T&>;

/******************************************************************************************
 * Comparison operators
 *****************************************************************************************/

template<class T, std::equality_comparable_with<T> U>
inline constexpr bool
operator==(const Optional<T>& a, const Optional<U>& b)
{
  if (a) {
    if (b) {
      return *a == *b;
    }
    return false;
  } else if (b) {
    return false;
  }
  return true;
}

template<class T, std::three_way_comparable_with<T> U>
inline constexpr std::compare_three_way_result_t<T, U>
operator<=>(const Optional<T>& a, const Optional<U>& b)
{
  if (a) {
    if (b) {
      return *a <=> *b;
    }
    return std::compare_three_way_result_t<T, U>::greater;
  } else if (b) {
    return std::compare_three_way_result_t<T, U>::less;
  }
  return std::compare_three_way_result_t<T, U>::equivalent;
}

template<class T>
inline constexpr bool
operator==(const Optional<T>& a, const NoneType&)
{
  return !a;
}

template<class T>
inline constexpr std::strong_ordering
operator<=>(const Optional<T>& a, const NoneType&)
{
  return a ? std::strong_ordering::greater : std::strong_ordering::equivalent;
}

/*
 * Without the NotOptional constraint you get a circular evaluation of constraints
 * from recursion in the standard lib.
 */
template<class T, optional_detail::NotOptional U>
  requires std::equality_comparable_with<T, U>
inline constexpr bool
operator==(const Optional<T>& a, const U& b)
{
  return a && *a == b;
}

template<class T, optional_detail::NotOptional U>
  requires std::three_way_comparable_with<T, U>
inline constexpr std::compare_three_way_result_t<T, U>
operator<=>(const Optional<T>& a, const U& b)
{
  if (a) {
    return *a <=> b;
  }
  return std::compare_three_way_result_t<T, U>::less;
}

} // namespace slm

#endif // SLM_OPTIONAL_HPP
