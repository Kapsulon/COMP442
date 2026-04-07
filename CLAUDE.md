# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a multi-stage compiler for a custom statically-typed OO language targeting the MOON virtual machine, built for COMP442. The pipeline is implemented as five independent CLI executables, each corresponding to a compiler phase.

## Build Commands

```sh
xmake                          # Build all default targets
xmake build [target]           # Build specific target (e.g. semantic-analyzer)
xmake build -r                 # Rebuild (clean + build)
xmake config -m debug          # Enable ASAN + UBSAN sanitizers
xmake config -m release        # Enable -O3 optimizations
xmake run [target] -- [args]   # Build and run a target
```

**Targets:** `lexical-analyzer`, `syntactic-analyzer`, `ast-generator`, `semantic-analyzer`, `compiler`

All binaries are placed in `bin/`.

## Running the Tools

Each tool accepts a `.src` file as input:

```sh
./bin/lexical-analyzer <file.src>
./bin/syntactic-analyzer <file.src>
./bin/ast-generator <file.src>
./bin/semantic-analyzer <file.src>
./bin/compiler <file.src>
```

Test inputs live in `assignments/assignment<N>/tests/` and sample programs (`bubblesort.src`, `polynomial.src`, `simplemain.src`) in their respective `resources/` directories.

## Architecture

The compiler pipeline has a strict linear data flow:

```
Source (.src)
  → LexicalAnalyzer   → token stream
  → SyntacticAnalyzer → AST (via semantic actions during parsing)
  → SemanticAnalyzer  → symbol tables + validated AST
  → Compiler          → MOON assembly (Assignment 5, WIP)
```

**Key design decisions:**

- **LexicalAnalyzer** uses `mmap` for file I/O and `ctre` (compile-time regex) for token matching. Token positions (line, column) are tracked for error reporting.
- **SyntacticAnalyzer** is a hand-written LL(1) recursive descent parser. Grammar non-terminals are defined as an enum. Semantic actions (115+ types like `MakeId`, `MakeFuncCall`, `MakeAssignStat`) are interleaved with parsing and build the AST onto a stack.
- **ASTNode** uses `shared_ptr` children and raw parent pointers. Node kinds match the language constructs (`Prog`, `ClassList`, `FuncDef`, `StatBlock`, etc.).
- **SemanticAnalyzer** performs two-pass analysis: first builds hierarchical symbol tables (global → class → function scopes), then validates semantics. Checks are split into `TypeChecks.cpp` and `SymbolTableChecks.cpp`.
- **Problems** is the unified diagnostics system — all errors/warnings/infos go through it. It attaches source tokens for location-aware output and reports to stderr.
- Each phase's `*_driver.cpp` wires up `argparse`, instantiates the relevant analyzer(s), and handles output formatting.

## Language

The source language is a statically-typed OO language with:
- Types: `integer`, `float`, `void`, arrays (multi-dimensional)
- Classes with `inherits`, `public`/`private` members and methods
- Control flow: `if/then/else/end`, `while/do/end`
- I/O: `read`, `write`
- `//` and `/* */` comments
