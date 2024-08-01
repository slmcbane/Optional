# Optional
Optionally, Optional replaces std::optional

For my own use, I wanted a nullable type that supports:
 * Optional references - I hate writing optional<reference_wrapper<T>>. I find the reasons for not having this in the standard unconvincing.
 * Support for C++23 monadic operations without requiring C++23 compiler support
 * No exceptions

In this project I also wanted to experiment with a couple of things:
 * Cleaner implementation using C++20 concepts.
 * Requiring explicit syntax to create an Optional - using my class you must do something like Optional<int> x = Some(1).

I also aim to provide the same guarantees as far as trivial copyability, etc. as in the standard library, and satisfy all
constexpr requirements from the C++20 standard. You should not be giving up any efficieny by using this over std::optional.
 
# License
Copyright 2024 by Sean McBane and released under the terms of the MIT License (see LICENSE for full statement).
