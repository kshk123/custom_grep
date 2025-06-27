# CustomGrep

A C++ utility that recursively searches all files under a directory for a given query.
Supports optional case-insensitive or regex matching, and parallelizes work across threads.

---

## Table of Contents

1. [Design Overview](#design-overview)
2. [Build & Test](#build--test)
3. [Usage](#usage)
4. [Examples](#examples)
5. [License](#license)

---

## Design Overview

1. **File Collection (serial)**
   - Uses `std::filesystem::recursive_directory_iterator`
   - Gathers all regular files into a `std::vector<path>`

2. **Per-File Search (serial per file)**
   - **Substring mode**
     - Case-sensitive:
       ```cpp
       line.find(query);
       ```
     - Case-insensitive: lowercase `query` and `line` via:
       ```cpp
       std::transform(s.begin(), s.end(), s.begin(), ::tolower);
       ```
   - **Regex mode**
     ```cpp
     std::regex re(query, ECMAScript | (ignoreCase ? icase : 0));
     std::regex_search(line, re);
     ```
   - Handle CRLF: strip trailing `'\r'` after each `std::getline`

3. **Parallel Search**
   - Determine **N** = `std::thread::hardware_concurrency()`. If this returns
     `0`, use **N = 1** thread instead.
   - Split file list into **N** contiguous chunks
   - Spawn **N** threads, each:
     1. Runs per-file search on its slice
     2. Accumulates `Match` objects into a thread-local `vector<Match>`
   - Join all threads and merge results—no mutex needed since each thread has its own vector

---

## Build & Test

> Requires **CMake 3.15+** and a **C++20**-capable compiler.

### 1. Build (default)

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 2. Build & Run Tests

```bash
mkdir build
cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest --output-on-failure
```

---

## Usage

```text
Usage: ./grep_exec <query> <directory> [OPTIONS]

Options:
  --ignore-case    Perform case-insensitive matching
  --regex          Treat <query> as a regular expression
```

---

## Examples

```bash
# 1. Case-sensitive substring (default)
./grep_exec "foo" /path/to/dir

# 2. Case-insensitive substring
./grep_exec "foo" /path/to/dir --ignore-case

# 3. Regex, case-sensitive
./grep_exec '^foo[0-9]+' /path/to/dir --regex

# 4. Regex, case-insensitive
./grep_exec '^foo[0-9]+' /path/to/dir --ignore-case --regex
```

---

## License

Distributed under the MIT License. See [LICENSE](./LICENSE) for details.
