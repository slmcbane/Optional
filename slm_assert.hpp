#ifndef SLM_CHECK_REQUIRE_HPP
#define SLM_CHECK_REQUIRE_HPP

#include <format>
#include <iostream>

namespace slm {

namespace assertions {

#define UNLIKELY(x) __builtin_expect(x, 0)
#define INLINE inline __attribute__((always_inline))
#define NOT_INLINE __attribute__((noinline))

inline NOT_INLINE void
simple_fail(const char* condition_text, const char* func, const char* file, int line)
{
  std::vformat_to(std::ostreambuf_iterator(std::cerr),
                  "Assertion error in {:s}:{:d}:{:s}: {:s}\n",
                  std::make_format_args(file, line, func, condition_text));
  std::abort();
}

template<class... Args>
NOT_INLINE void
fail_with_message(const char* condition_text,
                  const char* func,
                  const char* file,
                  int line,
                  const char* msg_format,
                  Args&&... args)
{
  auto msg = std::vformat(msg_format, std::make_format_args(args...));
  std::vformat_to(std::ostreambuf_iterator(std::cerr),
                  "Assertion error in {:s}:{:d}:{:s}: {:s}; message: {:s}\n",
                  std::make_format_args(file, line, func, condition_text, msg));
  std::abort();
}

template<class... Args>
INLINE void
check(bool condition,
      const char* condition_text,
      const char* func,
      const char* file,
      int line,
      Args&&... args)
{
  if (UNLIKELY(!condition)) {
    if constexpr (sizeof...(Args) == 0) {
      simple_fail(condition_text, func, file, line);
    } else {
      fail_with_message(condition_text, func, file, line, std::forward<Args>(args)...);
    }
  }
}

} // namespace assert

#define REQUIRE(condition, ...)                                                                \
  do {                                                                                         \
    if constexpr (!std::is_constant_evaluated())                                               \
      assertions::check(                                                                       \
        condition, #condition, __func__, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);       \
  } while (0);

#ifndef NDEBUG
#define CHECK(condition, ...) REQUIRE(condition __VA_OPT__(, ) __VA_ARGS__)
#else
#define CHECK(condition, ...)
#endif

} // namespace slm

#endif // SLM_CHECK_REQUIRE_HPP
