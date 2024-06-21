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

  T& unwrap() const noexcept { return m_ref; }

private:
  T& m_ref;
};

template<class T>
Some(T&& x) -> Some<T&&>;

template<class T>
Some(T& x) -> Some<T&>;

template<class T>
Some(const T& x) -> Some<const T&>;

namespace optional_detail {
template<class T>
inline constexpr bool is_some = false;

template<class T>
inline constexpr bool is_some<Some<T>> = true;
} // namespace optional_detail

struct None
{};

inline constexpr None none;

template<class T>
  requires(!std::is_same_v<T, None> && !std::is_reference_v<T> && !optional_detail::is_some<T>)
class Optional;

namespace optional_detail {

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

template<class F, class T>
constexpr inline bool returns_non_array_object =
  !std::disjunction_v<std::is_reference<std::invoke_result_t<F, T>>,
                      std::is_array<std::invoke_result_t<F, T>>>;

} // namespace optional_detail

template<class T>
  requires(!std::is_same_v<T, None> && !std::is_reference_v<T> && !optional_detail::is_some<T>)
class Optional
{
  constexpr Optional() noexcept = default;
  constexpr Optional(None) noexcept {}

  constexpr Optional(const Optional&)
    requires std::is_trivially_copy_constructible_v<T>
  = default;

  constexpr Optional(const Optional&)
    requires(!std::is_copy_constructible_v<T>)
  = delete;

  constexpr Optional(const Optional& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires(std::is_copy_constructible_v<T> && !std::is_trivially_copy_constructible_v<T>)
  {
    if (other.m_engaged) {
      std::construct_at(&m_payload, other.m_payload);
      m_engaged = true;
    }
  }

  constexpr Optional(Optional&&)
    requires std::is_trivially_move_constructible_v<T>
  = default;

  constexpr Optional(Optional&&)
    requires(!std::is_move_constructible_v<T>)
  = delete;

  constexpr Optional(Optional&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires(std::is_move_constructible_v<T> && !std::is_trivially_move_constructible_v<T>)
  {
    if (other.m_engaged) {
      std::construct_at(&m_payload, std::move(other.m_payload));
      m_engaged = true;
    }
  }

  template<class... Args>
  constexpr explicit Optional(std::in_place_t, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<T, Args&&...>)
    requires std::is_constructible_v<T, Args&&...>
    : m_payload(std::forward<Args>(args)...)
    , m_engaged{ true }
  {
  }

  template<class U>
  constexpr Optional(Some<const U&> x) noexcept(std::is_nothrow_constructible_v<T, const U&>)
    requires std::is_constructible_v<T, const U&>
    : m_payload(x.unwrap())
    , m_engaged{ true }
  {
  }

  template<class U>
  constexpr Optional(Some<U&&> x) noexcept(std::is_nothrow_constructible_v<T, U&&>)
    requires std::is_constructible_v<T, U&&>
    : m_payload(x.unwrap())
    , m_engaged{ true }
  {
  }

  constexpr ~Optional()
    requires std::is_trivially_destructible_v<T>
  = default;

  constexpr ~Optional() noexcept(std::is_nothrow_destructible_v<T>)
    requires(!std::is_trivially_destructible_v<T>)
  {
    if (m_engaged) {
      m_payload.~T();
    }
  }

  constexpr Optional& operator=(None) noexcept
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

  constexpr Optional& operator=(const Optional& other)
    requires(!(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>))
  = delete;

  constexpr Optional& operator=(const Optional& other) noexcept(
    std::is_nothrow_copy_constructible_v<T>&& std::is_nothrow_copy_assignable_v<T>&&
      std::is_nothrow_destructible_v<T>)
    requires(!optional_detail::trivially_copy_assignable<T>)
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

  constexpr Optional& operator=(Optional&&)
    requires(!(std::is_move_assignable_v<T> && std::is_move_constructible_v<T>))
  = delete;

  constexpr Optional& operator=(Optional&& other) noexcept(
    std::is_nothrow_move_assignable_v<T>&& std::is_nothrow_move_constructible_v<T>&&
      std::is_nothrow_destructible_v<T>)
    requires(!optional_detail::trivially_move_assignable<T>)
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
  constexpr Optional& operator=(Some<const U&> x) noexcept(
    std::is_nothrow_constructible_v<T, const U&>&& std::is_nothrow_assignable_v<T, const U&>)
    requires(std::is_assignable_v<T, const U&> && std::is_constructible_v<T, const U&>)
  {
    if (m_engaged) {
      m_payload = x.unwrap();
    } else {
      std::construct_at(&m_payload, x.unwrap());
      m_engaged = true;
    }
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

  constexpr const T& value() const& noexcept
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return m_payload;
  }

  constexpr T& value() & noexcept
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return m_payload;
  }

  constexpr T&& value() && noexcept
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return static_cast<T&&>(m_payload);
  }

  constexpr const T&& value() const&& noexcept
  {
    REQUIRE(m_engaged, "dereferencing disengaged Optional");
    return static_cast<const T&&>(m_payload);
  }

  template<class U>
  constexpr T value_or(U&& default_value) const& noexcept(
    std::conjunction_v<std::is_nothrow_copy_constructible<T>,
                       std::is_nothrow_constructible<T, U&&>>)
    requires std::constructible_from<T, U&&> && std::constructible_from<T, const T&>
  {
    return m_engaged ? m_payload : static_cast<T>(std::forward<U>(default_value));
  }

  template<class U>
  constexpr T value_or(U&& default_value) && noexcept(
    std::conjunction_v<std::is_nothrow_move_constructible<T>,
                       std::is_nothrow_constructible<T, U&&>>)
    requires std::constructible_from<T, U&&> && std::constructible_from<T, T&&>
  {
    return m_engaged ? std::move(m_payload) : static_cast<T>(std::forward<U>(default_value));
  }

  template<class F>
  constexpr auto and_then(F&& f) & noexcept(std::is_nothrow_invocable_v<F, T&>)
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, T&>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), m_payload, m_engaged);
  }

  template<class F>
  constexpr auto and_then(F&& f) const& noexcept(std::is_nothrow_invocable_v<F, const T&>)
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, const T&>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), m_payload, m_engaged);
  }

  template<class F>
  constexpr auto and_then(F&& f) && noexcept(std::is_nothrow_invocable_v<F, T&&>)
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, T>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), std::move(m_payload), m_engaged);
  }

  template<class F>
  constexpr auto and_then(F&& f) const&& noexcept(std::is_nothrow_invocable_v<F, const T&&>)
    requires optional_detail::is_optional<optional_detail::bare_result_type<F, const T>>
  {
    return optional_detail::maybe_invoke(std::forward<F>(f), std::move(m_payload), m_engaged);
  }

  template<class F>
  constexpr auto transform(F&& f) & noexcept(std::is_nothrow_invocable_v<F, T&>)
    requires(!std::is_rvalue_reference_v<std::invoke_result_t<F, T&>>)
  {
    using result_type = std::invoke_result_t<F, T&>;
    if constexpr (std::is_reference_v<result_type>) {
      return Optional(SomeRef(std::invoke(std::forward<F>(f), m_payload)));
    } else {
      return Optional<result_type>(std::forward<F>(f), m_payload);
    }
  }

  template<class F>
  constexpr auto transform(F&& f) const& noexcept(std::is_nothrow_invocable_v<F, const T&>)
    requires(!std::is_rvalue_reference_v<std::invoke_result_t<F, const T&>>)
  {
    using result_type = std::invoke_result_t<F, const T&>;
    if constexpr (std::is_reference_v<result_type>) {
      return Optional(SomeRef(std::invoke(std::forward<F>(f), m_payload)));
    } else {
      return Optional<result_type>(std::forward<F>(f), m_payload);
    }
  }

  template<class F>
  constexpr auto transform(F&& f) && noexcept(std::is_nothrow_invocable_v<F, T&&>)
    requires(!std::is_rvalue_reference_v<std::invoke_result_t<F, T &&>>)
  {
    using result_type = std::invoke_result_t<F, T&&>;
    if constexpr (std::is_reference_v<result_type>) {
      return Optional(SomeRef(std::invoke(std::forward<F>(f), std::move(m_payload))));
    } else {
      return Optional<result_type>(std::forward<F>(f), std::move(m_payload));
    }
  }

  template<class F>
  constexpr auto transform(F&& f) const&& noexcept(std::is_nothrow_invocable_v<F, const T&&>)
    requires(!std::is_rvalue_reference_v<std::invoke_result_t<F, const T &&>>)
  {
    using result_type = std::invoke_result_t<F, const T&&>;
    if constexpr (std::is_reference_v<result_type>) {
      return Optional(SomeRef(std::invoke(std::forward<F>(f), std::move(m_payload))));
    } else {
      return Optional<result_type>(std::forward<F>(f), std::move(m_payload));
    }
  }

  template<class F>
  constexpr Optional or_else(F&& f) const& noexcept(
    std::conjunction_v<std::is_nothrow_copy_constructible<T>, std::is_nothrow_invocable<F>>)
    requires std::conjunction_v<
      std::is_same<optional_detail::bare_result_type<F, const T&>, Optional<T>>,
      std::is_copy_constructible<T>,
      std::is_invocable<F>>
  {
    return m_engaged ? *this : std::invoke(std::forward<F>(f));
  }

  template<class F>
  constexpr Optional or_else(F&& f) && noexcept(
    std::conjunction_v<std::is_nothrow_move_constructible<T>, std::is_nothrow_invocable<F>>)
    requires std::conjunction_v<
      std::is_same<optional_detail::bare_result_type<F, T&&>, Optional<T>>,
      std::is_move_constructible<T>,
      std::is_invocable<F>>
  {
    return m_engaged ? std::move(*this) : std::invoke(std::forward<F>(f));
  }

  constexpr void swap(Optional& other) noexcept(
    std::conjunction_v<std::is_nothrow_move_constructible<T>, std::is_nothrow_swappable<T>>)
    requires std::conjunction_v<std::is_move_constructible<T>, std::is_swappable<T>>
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
  constexpr T& emplace(Args&&... args) noexcept(
    std::conjunction_v<std::is_nothrow_destructible<T>,
                       std::is_nothrow_constructible<T, Args&&...>>)
    requires std::constructible_from<T, Args&&...>
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

  template<class F, class U>
  constexpr explicit Optional(F&& f, U&& u) noexcept(std::is_nothrow_invocable_v<F, U&&>)
    requires std::is_invocable_r_v<T, F, U>
    : m_payload(std::invoke(std::forward<F>(f), std::forward<U>(u)))
    , m_engaged{ true }
  {
  }
};

/******************************************************************************************
 * Comparison operators
 *****************************************************************************************/

template<class T, std::equality_comparable_with<T> U>
inline constexpr bool
operator==(const Optional<T>& a, const Optional<U>& b) noexcept
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
operator<=>(const Optional<T>& a, const Optional<U>& b) noexcept
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
operator==(const Optional<T>& a, const None&) noexcept
{
  return !a;
}

template<class T>
inline constexpr std::strong_ordering
operator<=>(const Optional<T>& a, const None&) noexcept
{
  return a ? std::strong_ordering::greater : std::strong_ordering::equivalent;
}

template<class T, std::equality_comparable_with<T> U>
inline constexpr bool
operator==(const Optional<T>& a, const U& b) noexcept
{
  return a && *a == b;
}

template<class T, std::three_way_comparable_with<T> U>
inline constexpr std::compare_three_way_result_t<T, U>
operator<=>(const Optional<T>& a, const U& b) noexcept
{
  if (a) {
    return *a <=> b;
  }
  return std::compare_three_way_result_t<T, U>::less;
}

} // namespace slm

#endif // SLM_OPTIONAL_HPP
