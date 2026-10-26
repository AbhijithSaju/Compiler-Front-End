# Switch Statement Compiler Front End (C)

A teaching-oriented C11 compiler front end for a restricted C-like language. It demonstrates tokenization, recursive-descent parsing, and semantic checks for `switch`, `case`, `default`, and `break`.

## Build
```bash
gcc -std=c11 -Wall -Wextra -pedantic switch_compiler.c -o switch_compiler
```

## Run
```bash
./switch_compiler tests/valid_01.c
```
On Windows/MinGW use `switch_compiler.exe tests\valid_01.c`. Exit code 0 means no diagnostics, 1 means source errors, and 2 means invocation/file errors.

## Features
- Lexical analysis for keywords, identifiers, integer literals, operators, delimiters and comments.
- Syntax checks for switch labels and break statements.
- Integer constant-expression evaluation with unary +/− and binary +, −, *, /, %.
- Duplicate evaluated case detection per switch; duplicate default detection.
- Basic break-context checking and line-numbered diagnostics.

## Files
- `switch_compiler.c`: implementation
- `GRAMMAR_AND_CFG.md`: grammar and transformations
- `AST_AND_PARSE_TREE.md`: sample tree and AST design
- `SEMANTIC_ANALYSIS.md`: semantic rules and limitations
- `tests/`: valid and invalid test programs
- `TEST_RESULTS.md`: actual test outputs
- `CONTRIBUTIONS.md`: editable team contribution template
- `presentation.pptx`: project slides
- `evidence/`: test summary and output evidence

## Limitations
This is not a complete ISO C compiler. It supports a restricted statement/expression subset, does not perform full C type checking, and currently validates rather than constructing a complete AST. Character literals and all C integer suffix/conversion rules are not fully implemented.
