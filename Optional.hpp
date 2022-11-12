#ifndef SEANS_OPTIONAL_HPP
#define SEANS_OPTIONAL_HPP

#include <cassert>
#include <concepts>
#include <initializer_list>
#include <memory>
#include <tuple>
#include <type_traits>

#ifdef ALTERNATE_ASSERT_MACRO
#define SLM_ASSERT ALTERNATE_ASSERT_MACRO
#else
#define SLM_ASSERT assert
#endif // ALTERNATE_ASSERT_MACRO

namespace slm
{

template <class T>
struct Optional;

namespace Optional_detail
{

using std::is_reference_v;

static_assert(is_reference_v<int &> && is_reference_v<const int &> && !is_reference_v<int>);

template <class T>
concept ReferenceType = is_reference_v<T>;

template <class T>
concept NotReferenceType = !is_reference_v<T>;

template <class T>
inline constexpr bool is_tuple_v = false;

template <class... Ts>
inline constexpr bool is_tuple_v<std::tuple<Ts...>> = true;

template <class S, class T>
inline constexpr bool is_tuple_v<std::pair<S, T>> = true;

template <class T>
concept TupleLike = is_tuple_v<T>;

static_assert(
    is_tuple_v<std::tuple<int, char, long>> && is_tuple_v<std::tuple<int &>> &&
    is_tuple_v<std::pair<int, int>> && !is_tuple_v<int>);

template <class T>
concept TriviallyCopyable = std::conjunction_v<
    std::is_trivially_copy_assignable<T>, std::is_trivially_copy_constructible<T>,
    std::is_trivially_destructible<T>>;

template <class T>
concept TriviallyMovable = std::conjunction_v<
    std::is_trivially_move_assignable<T>, std::is_trivially_move_constructible<T>,
    std::is_trivially_destructible<T>>;

// clang-format off
template <class T, class U>
concept ConvertibleFromOptional = std::disjunction_v<
    std::is_constructible<T, Optional<U>&>,
    std::is_constructible<T, const Optional<U> &>,
    std::is_constructible<T, Optional<U> &&>,
    std::is_constructible<T, const Optional<U> &&>,
    std::is_convertible<Optional<U> &, T>,
    std::is_convertible<const Optional<U> &, T>,
    std::is_convertible<Optional<U> &&, T>,
    std::is_convertible < const Optional<U> &&, T> >;

template <class T, class U>
    concept ConvertibleOrAssignableFrom = ConvertibleFromOptional<T, U> or
    std::disjunction_v<
        std::is_assignable<T&, Optional<U>&>,
        std::is_assignable<T&, const Optional<U>&>,
        std::is_assignable<T&, Optional<U>&&>,
        std::is_assignable<T, const Optional<U>&&>>;
// clang-format on

} // namespace Optional_detail

struct nullopt_t
{
};

constexpr nullopt_t nullopt;

template <class T>
struct Optional
{
    /********************************************************************************
     * Constructors
     *******************************************************************************/
    constexpr Optional() : m_engaged{false} {}
    constexpr Optional(nullopt_t) : m_engaged{false} {}

    // Copy constructors. First for trivially copy constructible, second for non-trivial
    constexpr Optional(const Optional &other) requires std::is_trivially_copy_constructible_v<T>
    = default;

    // clang-format off
    constexpr Optional(const Optional &other)
        noexcept( std::is_nothrow_constructible_v<T, const T &>)
        requires(!std::is_trivially_copy_constructible_v<T>)
        : m_engaged{other.m_engaged}
    {
        if (m_engaged)
            std::construct_at(&m_payload.x, *other);
    }

    // Move constructors. First for trivally move constructible, second for non-trivial.
    constexpr Optional(Optional &&other) requires std::is_trivially_move_constructible_v<T>
    = default;

    constexpr Optional(Optional &&other)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        requires( !std::is_trivially_move_constructible_v<T>)
        : m_engaged{other.m_engaged}
    {    
        if (m_engaged)
            std::construct_at(&m_payload.x, std::move(*other));
    }

    // Construct from an optional of different type.
    template <class U>
    requires std::is_constructible_v<T, const U&>
        and (!Optional_detail::ConvertibleFromOptional<T, U>)
    explicit(!std::is_convertible_v<const U&, T>)
    constexpr Optional(const Optional<U> &other)
    noexcept(std::is_nothrow_constructible_v<T, const U&>)
        : m_engaged{other.m_engaged}
    {
        if (m_engaged)
            std::construct_at(&m_payload.x, *other);
    }

    template <class U>
    requires std::is_constructible_v<T, U&&>
        and (!Optional_detail::ConvertibleFromOptional<T, U>)
    explicit(!std::is_convertible_v<U&&, T>)
    constexpr Optional(Optional<U> &&other)
    noexcept(std::is_nothrow_constructible_v<T, U&&>)
        : m_engaged{other.m_engaged}
    {
        if (m_engaged)
            std::construct_at(&m_payload.x, std::move(*other));
    }

    // Forwarding constructors.
    template <class... Args>
    requires std::is_constructible_v<T, Args&&...>
    constexpr Optional(std::in_place_t, Args &&... args)
    noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
        : m_engaged{true}
    {
        std::construct_at(&m_payload.x, std::forward<Args>(args)...);
    }

    template <class U, class... Args>
    requires std::is_constructible_v<T, std::initializer_list<U>&, Args &&...>
    constexpr Optional(std::in_place_t, std::initializer_list<U> ilist, Args &&... args)
    noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U> &, Args &&...>)
        : m_engaged{true}
    {
        std::construct_at(&m_payload.x, ilist, std::forward<Args>(args)...);
    }

    // Construct from a different type convertible to T
    template <class U = T, class V = std::remove_cvref_t<U>>
    requires std::is_constructible_v<T, U&&> 
        and (!std::is_same_v<V, std::in_place_t>)
        and (!std::is_same_v<V, Optional<T>>)
    explicit(!std::is_convertible_v<U &&, T>)
    constexpr Optional(U &&u)
    noexcept(std::is_nothrow_constructible_v<T, U&&>) : m_engaged{true}
    {
        std::construct_at(&m_payload.x, std::forward<U>(u));
    }
    // clang-format on

    /********************************************************************************
     * Assignment operators
     *******************************************************************************/

    constexpr Optional &operator=(const Optional &) requires Optional_detail::TriviallyCopyable<T>
    = default;

    constexpr Optional &operator=(const Optional &other) requires(!Optional_detail::TriviallyCopyable<T>)
    {
        if (m_engaged)
            if (other.m_engaged)
                m_payload.x = *other;
            else
                unchecked_reset();
        else if (other.m_engaged)
            unchecked_construct(*other);
        return *this;
    }

    constexpr Optional &operator=(Optional &&) requires Optional_detail::TriviallyMovable<T>
    = default;

    constexpr Optional &operator=(Optional &&other) requires(!Optional_detail::TriviallyMovable<T>)
    {
        if (m_engaged)
            if (other.m_engaged)
                m_payload.x = std::move(*other);
            else
                unchecked_reset();
        else if (other.m_engaged)
            unchecked_construct(std::move(*other));
        return *this;
    }

    template <class U = T>
    constexpr Optional &operator=(U &&value)
    {
        if (m_engaged)
            m_payload.x = static_cast<U &&>(value);
        else
            unchecked_construct(static_cast<U &&>(value));
    }

    template <class U>
    constexpr Optional &
    operator=(const Optional<U> &other) requires(!Optional_detail::ConvertibleOrAssignableFrom<T, U>)
    {
        if (m_engaged)
            if (other.m_engaged)
                m_payload.x = *other;
            else
                unchecked_reset();
        else if (other.m_engaged)
            unchecked_construct(*other);
        return *this;
    }

    template <class U>
    constexpr Optional &
    operator=(Optional<U> &&other) requires(!Optional_detail::ConvertibleOrAssignableFrom<T, U>)
    {
        if (m_engaged)
            if (other.m_engaged)
                m_payload.x = static_cast<U &&>(*other);
            else
                unchecked_reset();
        else if (other.m_engaged)
            unchecked_construct(static_cast<U &&>(*other));
        return *this;
    }

  private:
    union Payload
    {
        T x;
    } m_payload;

    bool m_engaged;

    template <class U>
    void unchecked_construct(U &&v)
    {
        std::construct_at(&m_payload.x, static_cast<U &&>(v));
        m_engaged = true;
    }

    void unchecked_reset()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
            m_payload.x.~T();
        m_engaged = false;
    }
};

} // namespace slm

#undef SLM_ASSERT

#endif // SEANS_OPTIONAL_HPP
