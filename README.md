# Optional
Optionally, Optional replaces std::optional

For my hobby projects, I wanted a nullable type that supports:
 * Optional references - I am convinced by the reasoning presented [in this article](https://thephd.dev/to-bind-and-loose-a-reference-optional). I want an Optional<T&> that rebinds on assignment. I hate writing optional<reference_wrapper<T>>.
 * Chaining via and_then. This will be in C++23 but I want it now (10/2022).
 * No exceptions

Besides the items above my goals are:
 * One to one correspondence with std::optional (except of course for reference types, and exceptions)
 * Code generation as efficient as std::optional
 * Fulfill all constexpr requirements in the C++20 standard
 * Have correct rebinding semantics for optionals containing std::pair or std::tuple with reference members
 
# License
Copyright 2022 by Sean McBane and released under the terms of the MIT License (see LICENSE for full statement).
