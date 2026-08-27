# 🕵️ Spy Language

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-Spy%20%7C%20C%2B%2B-orange)]()
[![Build](https://img.shields.io/badge/build-cmake-brightgreen)]()
[![Lines](https://img.shields.io/badge/lines-15k%2B-blue)]()

**Fast, self-hosting programming language that compiles to C / C++ / LLVM IR.**

No header files. Python-like syntax. C-level performance. ~15K lines, 40+ tests.

> `spy-language` by [m4spylanguage](https://github.com/m4spylanguage) — GPL-3.0

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

## 📖 Syntax Guide

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

### Imports (No .h needed!)
```spy
import mathutils
from mathutils import square, cube

fn main():
    print(mathutils.square(5))
    print(cube(6))
```

### Extern C Interop
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

Core: `src/Codegen.cpp` (3,832) | `tests/codegen.spy` (1,723 self-hosted) | `tests/parser.spy` (943)

---

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) — PRs welcome! Please read coding style, tests, and GPL-3.0 CLA notes.

## 📄 License

**GPL-3.0** — Copyright (C) 2026 Valuvajjala Vivek Vardhan Rao  
Free to use, modify, and distribute. Modified versions must stay GPL-3.0.

---
Made with ❤️ in India — M4 Spy Language
