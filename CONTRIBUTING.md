# Contributing to Spy Language

Thanks for your interest! 🕵️

## Quick Start

1. Fork `m4spylanguage/spy-language` & clone
2. Build: `cmake -B build && cmake --build build`
3. Test: `./build/spy tests/hello.spy`
4. Create branch: `git checkout -b feat/my-feature`
5. Commit & PR to `main`

## What to Contribute

- Bug fixes (Lexer/Parser/Codegen)
- New `.spy` features / stdlib
- Tests in `tests/*.spy`
- Docs, examples, benchmarks
- SpyUI / LLVM backend improvements

## Coding Style

- C++17, minimal dependencies
- Match existing style — no extra comments unless needed
- Keep `src/` focused, add `.spy` tests for new features
- Run before PR:
  ```bash
  cmake -B build && cmake --build build
  ./build/spy tests/hello.spy
  ./build/spy tests/list_test.spy --c
  ```

## Adding a Feature Example

```bash
# 1. Edit src/Parser.cpp, src/Codegen.cpp, include/spy/AST.h
# 2. Add test
echo 'fn main(): print("new feature")' > tests/myfeature.spy
# 3. Verify
./build/spy tests/myfeature.spy --ast
./build/spy tests/myfeature.spy
```

## Pull Request Checklist

- [ ] Builds with CMake
- [ ] New test in `tests/` passes
- [ ] Hello world still works
- [ ] No binary files committed (`*.exe`, `*.obj`, `build/`)

## License

By contributing, you agree your code will be licensed under **GPL-3.0** same as Spy. You retain copyright. For future relicensing, you grant the project owner right to relicense your contribution (CLA implied by PR).

## Code of Conduct

Be respectful, constructive, no spam. Maintainer: [@m4 Spider](https://github.com/m4spylanguage)

## Questions?

Open an Issue or Discussion on GitHub. Happy hacking! 🚀
