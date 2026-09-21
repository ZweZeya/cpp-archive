# cpp-archive

Personal, header-only implementations of core C++ building blocks, written to learn how they work under the hood. Everything lives in the `my_std` namespace.

> Learning project, not production code. Use the standard library for real work.

## Contents

| Path | What's inside |
|------|---------------|
| [stl/vector/](stl/vector) | `vector`, plus `vector2` with a custom allocator |
| [stl/string/](stl/string) | `basic_string` |
| [smart_pointers/](smart_pointers) | `unique_ptr`, `shared_ptr`, `weak_ptr` |
| [utility/](utility/utility.hpp) | `remove_reference`, `move`, `forward` (move semantics helpers) |
| [concurrency/](concurrency/spsc_queue.hpp) | Lock-free single-producer/single-consumer ring buffer |

## Usage

Just include the header you need:

```cpp
#include "stl/vector/vector.hpp"

my_std::vector<int> v;
```

Requires C++20 (`shared_ptr` uses concepts).
