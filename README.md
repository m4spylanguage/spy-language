# 🕵️ Spy Language

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-Spy%20%7C%20C%2B%2B-orange)]()
[![Build](https://img.shields.io/badge/build-cmake-brightgreen)]()

**High-level syntax. No headers. C-level speed. Spy compiles to C / C++ / LLVM IR — and it's self-hosting.**

> `spy-language` by [m4spylanguage](https://github.com/m4spylanguage) — GPL-3.0 — Made with ❤️ in India

---

## ⚡ Hello World

```spy
fn main():
    print("hello world")
```

```bash
./spy hello.spy              # compile & run
./spy hello.spy --c          # show generated C
./spy hello.spy --ast        # show AST
./spy hello.spy --tokens     # show tokens
```

---

## 💭 Why Spy? Why We Built This

**C is fast but painful. Python is beautiful but slow. We wanted both.**

We were tired of:
- Writing `.h` files just to declare what `.c` already defines
- Boilerplate, macros, `Makefile` hell for a simple idea
- Choosing between *joy* and *performance*

**So we built Spy — a language that feels like Python, runs like C.**

- No `#include`, no `.h`, no forward declarations — `import mathutils` just works
- Clean `fn`, `let`, `if`, `for`, `list()` — high-level, readable, fun
- Yet it emits clean C / LLVM you can ship anywhere — embedded, Android, desktop, server

> **Small today (15K lines) — massive vision. This is day 1. We open-sourced early so YOU can shape it.**

Spy is not another tutorial toy. The compiler is **self-hosted** — `codegen.spy` (1,723 lines) and `parser.spy` (943 lines) compile themselves. That's the proof it works.

---

## 🎯 What We Want — The Vision

**A language for humans, not compilers.**

1. **High-Level Forever** — No headers, no manual memory pain, no type torture. If Python can do it in one line, Spy should too — but fast.
2. **AI & Modern Ready** — First-class lists, dicts, pipes, match, closures, and future `ai` libs — so you can write real AI, apps, games, not just hello worlds.
3. **One Language, Everywhere** — Compile to C for speed, C++ for UI (`SpyUI`), LLVM for optimization, and soon to WASM / Android / iOS from same `.spy`.
4. **Spider OS Native** — Future frameworks built *in* Spy for Spider OS — apps, UI, services, all Spy-native.
5. **Open & Fun** — GPLv3, fully open, no corporate gatekeeping. Learn compiler design by reading code, not 500K.

**Goal:** Be the *easiest* language to start, and *fast enough* to never leave.

---

## 🙌 Why Contribute? (Why Even Bother With a Tiny Language?)

Because tiny is the **best time** to join.

- **Your name in history** — First 100 contributors to a language get remembered. Fix a bug today, it's in every future `spy --version`.
- **No bureaucracy** — No 3-month RFC. Open a PR, get merged, see it run.
- **Build your dream feature** — Want `async`, `generics`, `AI tensors`, `hot reload`? Build it — Spy is small enough to let you.
- **Keep it closed = useless; open = massive** — Keeping Spy closed helps nobody. Open means thousands can try, break, and make it legendary.

> If you ever wanted to say *"I helped build a programming language"* — this is your chance. Right now. Early.

See [CONTRIBUTING.md](CONTRIBUTING.md) — all levels welcome, even your first PR.

---

## 📦 Installation

### Prerequisites
- `clang` / `clang++` (C/C++ backend)
- `cmake >= 3.10`
- C++17

### Build
```bash
git clone https://github.com/m4spylanguage/spy-language.git
cd spy-language
cmake -B build && cmake --build build
# binary at: ./build/spy
# or use prebuilt: ./spy_compiler_linux
```

---

## 📖 Syntax — High Level, No Headers, No Nonsense

### Variables
```spy
let x = 42
let name = "spy"
let pi = 3.14
let flag = true
let nothing = None
```

### Functions
```spy
fn add(a: i32, b: i32) -> i32:
    return a + b

fn greet(name):
    print("hello " + name)

fn main():
    let result = add(3, 4)
    print(result)
    greet("world")
```

### Control Flow
```spy
fn main():
    let x = 42
    if x > 50:
        print("big")
    elif x > 10:
        print("medium")
    else:
        print("small")

    while x > 0:
        print(x)
        x = x - 1

    for i in range(0, 10):
        print(i)
```

### Lists & Indexing
```spy
fn main():
    let b = list(10, 20, 30)
    print(len(b))
    b.push(40)
    print(b[1])
    b[1] = 55
    for x in b:
        print(x)
```

### Imports (No .h needed! That's the point.)
```spy
import mathutils
from mathutils import square, cube

fn main():
    print(mathutils.square(5))
    print(cube(6))
```
> No header files. Ever. Just `import` a `.spy` file — the compiler reads it directly. Like Python, but compiled to C.

### Extern C Interop (when you need it)
```spy
extern fn printf(fmt: str) -> i32

fn main():
    printf("from C!\n")
```

### Match / Enums / Classes / Try / Lambdas supported
See `tests/` for 40+ examples: `match_test.spy`, `list_test.spy`, `closure_test.spy`, `parser.spy`, `codegen.spy`

---

## 🚀 Usage

```bash
spy <file.spy>                       # compile & run (C backend)
spy <file.spy> --target cpp          # C++ backend (SpyUI)
spy <file.spy> --target ir           # LLVM IR
spy <file.spy> --output myapp        # compile to binary
spy <file.spy> --c                   # emit C code only
spy <file.spy> --ast                 # dump AST
spy <file.spy> --tokens              # dump tokens
```

---

## 🗺️ Roadmap — Help Us Build It

- [x] Self-hosting (parser + codegen in Spy)
- [x] C / C++ / LLVM backends
- [x] SpyUI framework
- [ ] Package manager (`spy get`)
- [ ] LSP & VS Code extension
- [ ] WASM & Android targets
- [ ] AI stdlib (tensors, `ai` module)
- [ ] Hot reload & REPL
- [ ] Spider OS frameworks (UI, App, System APIs in pure Spy)
- [x] Spider OS boot animation — written entirely in Spy
- [ ] Self-compiled `spy` written *entirely* in Spy

Pick one. Own it. PR it.

---

## 📁 Project Structure

```
spy-language/
├── src/           # C++ compiler (Lexer, Parser, Codegen)
├── include/spy/   # Headers (AST, Token, Codegen)
├── spyui/         # UI framework (spyui.h/cpp)
├── tests/         # 40+ .spy examples
├── CMakeLists.txt
└── LICENSE (GPL-3.0)
```

---

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) — PRs welcome! Please read coding style, tests, and GPL-3.0 CLA notes.

## 📄 License

**GPL-3.0** — Copyright (C) 2026 Valuvajjala Vivek Vardhan Rao  
Free to use, modify, and distribute. Modified versions must stay GPL-3.0.

---
Made with ❤️ in India — M4 Spy Language — *"No headers. Just code."*
