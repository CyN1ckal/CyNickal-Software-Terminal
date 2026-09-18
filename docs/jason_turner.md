# Jason Turner’s C++ practices

A working synthesis of Jason Turner’s published C++ guidance for this project. Transfer the **habits**, not a product clone: tools first, constexpr by default, Rule of Zero, initialize everything, const where it still moves.

This is not a dump of the Leanpub book. The paid chapter bodies were not inspected; rules below come from the public table of contents, the GitHub coding-standards repo, `cmake_template`, C++ Weekly, and talks. Later weekly episodes sometimes tighten older GitHub examples — prefer the later advice when they conflict. The GitHub repo is collaborative; Turner curates it, but some wording may be community PRs. The preface says you are not supposed to agree with it 100%.

---

## 1. Sources

| Source | What it is |
|---|---|
| Leanpub *C++ Best Practices* | Paid, numbered “45ish Simple Rules with Specific Action Items,” last updated 2022-07-12. Short items plus exercises. [leanpub.com/cppbestpractices](https://leanpub.com/cppbestpractices/) |
| `cpp-best-practices/cppbestpractices` | Free CC BY-NC 4.0 coding-standards text, meant to be forked. Sibling to the book, not a verbatim dump. [github.com/cpp-best-practices/cppbestpractices](https://github.com/cpp-best-practices/cppbestpractices) |
| `cmake_template` | Executable version of the tooling: warnings, clang-tidy, sanitizers, CI. [github.com/cpp-best-practices/cmake_template](https://github.com/cpp-best-practices/cmake_template) |
| C++ Weekly + talks | constexpr/consteval, const, CMake, clang-tidy. Later episodes can supersede older GitHub examples. [youtube.com/@cppweekly](https://www.youtube.com/@cppweekly) |

A later Leanpub title, *C++23 Best Practices*, is a major update of the original book. It is not treated as the source of this file.

GitHub chapter map: Use the Tools Available, Style, Safety, Maintainability, Portability, Threadability, Performance, Correctness, Enable Scripting.

---

## 2. Philosophy

From the book TOC:

- Slow down.
- C++ is not magic.
- C++ is not object-oriented.
- Learn another language.
- Do not invoke undefined behavior.

C++20 items in the book: concepts, spaceship operator, `consteval` / `constinit`, designated initializers.

---

## 3. Tools first

Establish source control, automated builds, CI, tests, compiler warnings, static analysis, and sanitizers **early**. Strict tooling belongs on the top-level project, not on dependents.

### Compiler warnings

**GCC / Clang.** `-Wall -Wextra` is not enough. Minimum:

```
-Wall -Wextra -Wshadow -Wconversion -Wpedantic -Werror
```

Also: `-Wnon-virtual-dtor`, `-Wold-style-cast`, `-Wcast-align`, `-Wunused`, `-Woverloaded-virtual`, `-Wnull-dereference`, `-Wdouble-promotion`, `-Wformat=2`, `-Wimplicit-fallthrough`.

GCC extras: `-Wmisleading-indentation`, `-Wduplicated-cond`, `-Wduplicated-branches`, `-Wlogical-op`, `-Wuseless-cast`, `-Wsuggest-override`.

**MSVC.** `/W4` plus extra `/w14xxx` promotions and `/permissive-`. Do not use `/Wall` for normal builds — the MSVC standard library is not `/Wall` clean. Treat warnings as errors (`/WX`) at top level; turn that off when the project is a dependency or in packaging-maintainer mode.

### clang-tidy

Enable all (`*`), then disable project-specific families:

```
-abseil-* -altera-* -android-* -fuchsia-* -google-* -llvm* -zircon-*
```

Also disable `-modernize-use-trailing-return-type` and some readability/misc checks. Keep `bugprone` and `cert`.

### Analyzers

`cmake_template` turns on clang-tidy and cppcheck at top level; include-what-you-use is optional. Other named analyzers: Clang’s analyzer, MSVC `/analyze`, Coverity Scan, PVS-Studio, Sonar, GCC’s analyzer.

The GitHub tools chapter says cppcheck `--enable=all`. The template actually uses `--enable=style,performance,warning,portability` with suppressions — the shipped default is narrower.

### CMake / CI (as encoded in `cmake_template`)

- C++23, `CMAKE_CXX_EXTENSIONS` OFF.
- INTERFACE libraries `myproject_warnings` / `myproject_options`.
- CPM dependencies with `SYSTEM YES`.
- ASan + UBSan on by default where they link.
- IPO / LTO on.
- Always export `compile_commands.json`.
- Default build type RelWithDebInfo.
- Strict tooling only when `PROJECT_IS_TOP_LEVEL`.

CI is a GitHub Actions matrix of at least two OSes and two compilers (ubuntu / macOS / Windows, llvm-19.1.1 and gcc-14, MSVC on Windows), Debug and Release, packaging-maintainer ON/OFF, Ninja Multi-Config, ctest plus coverage to Codecov, scheduled CodeQL, `fail-fast: false`. Public GitHub projects: enable Actions, coverage, and Coverity Scan.

---

## 4. constexpr / consteval

Mark as `constexpr` everything that is reasonable. Treat it as moving work from runtime to compile time (0–100%), not as template metaprogramming. Libraries should be constexpr-enabled so the first *M* known steps of a problem can run at compile time. Test constexpr functions **both** at compile time and at runtime.

| Situation | Default |
|---|---|
| Function-scope compile-time data | `static constexpr` (plain `constexpr` objects are stack values unless `static`) |
| Header / global compile-time data | `inline constexpr` so translation units merge the data |
| Force compile-time evaluation | `consteval` — especially literal-taking APIs, no-argument free functions, default constructors |

C++20 `constexpr` `vector` / `string` cannot persist heap allocations into runtime (the “two-step”). Compile-time data of unknown size must be copied into a statically sized object. C++26 `define_static_string` / `define_static_array` is the library fix he presents.

Caveats: constexpr is not truly zero runtime cost (loader relocations of pointers / `string_view`s); consteval is hard to debug or fuzz; arguments must be constant expressions; compilers do not catch all undefined behavior during constant evaluation.

Existence proofs he cites: 2017 compile-time JSON, and a later fully constexpr ARM emulator whose compile-time tests succeed if they compile.

---

## 5. RAII, special members, loops, globals

**Rule of Zero** by default: do not user-declare destructor, copy/move constructor, or copy/move assignment unless the class has novel ownership.

If you must manage a resource (typically because you need a destructor), **Rule of 5**: define, `=default`, or `=delete` all five. Declaring a destructor, copy constructor, or assignment operator suppresses the move constructor.

Prefer RAII over raw `new` / `delete`:

- Stack objects.
- `unique_ptr` / `make_unique`.
- Factories that return `unique_ptr`.
- `shared_ptr` / `make_shared` only when sharing is needed.

Book TOC includes “No More new!” and prefer stack over heap.

Prefer algorithms and ranged-for over raw loops. Never `std::bind` (use a lambda). Avoid `std::function`. Avoid global data, statics, and singletons (threadability).

---

## 6. Initialization and const

Always initialize:

- Member initializer lists.
- In-class initializers — prefer `{}` over `=`.
- Initialize at declaration.
- Smallest possible scope: declare as late as possible, and only when the object can be initialized.

Forgotten member initialization is undefined behavior. C++20 designated initializers are an extra option.

`constexpr` everything known at compile time. `const` everything else that can be, including parameters and locals.

**Later C++ Weekly tightens const.** Do **not** put `const` on:

- Member data.
- Non-reference return types.
- Values you need to move from (`const` suppresses moves).

Stop using `const_cast`. This conflicts with an older GitHub Style example that marks a never-reassigned member `const`. Prefer the later weekly recommendation.

---

## 7. What this file does not cover

- Paid Leanpub chapter bodies (public TOC only, plus a secondary review).
- *C++23 Best Practices*.
- ACCU 2025 slides (transcript only).
- CppCon 2018 slide PDF (abstract / transcript only).
- Every C++ Weekly episode on constexpr / consteval.
