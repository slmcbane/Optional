#ifndef SLM_CHECK_REQUIRE_HPP
#define SLM_CHECK_REQUIRE_HPP

#include <format>
#include <iostream>
#include <source_location>

namespace slm {

namespace assertions {

#define INLINE inline __attribute__((always_inline))
#define NOT_INLINE __attribute__((noinline))

inline NOT_INLINE void
simple_fail(const char* condition_text, const std::source_location& loc) noexcept
{
  std::format_to(std::ostreambuf_iterator(std::cerr),
                 "Assertion error in {:s}:{:d}:{:s}: {:s}\n",
                 loc.file_name(),
                 loc.line(),
                 loc.function_name(),
                 condition_text);
  std::abort();
}

template<class... Args>
NOT_INLINE void
fail_with_message(const char* condition_text,
                  const std::source_location& loc,
                  const char* msg_format,
                  Args&&... args) noexcept
{
  auto msg = std::vformat(msg_format, std::make_format_args(args...));
  std::format_to(std::ostreambuf_iterator(std::cerr),
                 "Assertion error in {:s}:{:d}:{:s}: {:s}; message: {:s}\n",
                 loc.file_name(),
                 loc.line(),
                 loc.function_name(),
                 condition_text,
                 msg);
  std::abort();
}

template<class... Args>
INLINE constexpr void
check(bool condition,
      const char* condition_text,
      const std::source_location& loc,
      Args&&... args)
{
  if (std::is_constant_evaluated() && !condition) {
    *(volatile int*)nullptr = 0;
  } else if (!condition) [[unlikely]] {
    if constexpr (sizeof...(Args) == 0) {
      simple_fail(condition_text, loc);
    } else {
      fail_with_message(condition_text, loc, std::forward<Args>(args)...);
    }
  }
}

} // namespace assert

#define REQUIRE(condition, ...)                                                                \
  assertions::check(                                                                           \
    condition, #condition, std::source_location::current() __VA_OPT__(, ) __VA_ARGS__);

#ifndef NDEBUG
#define CHECK(condition, ...) REQUIRE(condition __VA_OPT__(, ) __VA_ARGS__)
#else
#define CHECK(condition, ...)
#endif

} // namespace slm

#endif // SLM_CHECK_REQUIRE_HPP
