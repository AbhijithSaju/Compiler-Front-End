# Modular Switch Statement Compiler + Single GUI

## Modules
- `Shared declarations are placed at the top of each C module, so no header file is required.`: shared tokens, AST types and function declarations
- `lexer.c`: lexical analysis and tokenization
- `parser.c`: syntax analysis and AST construction
- `ast.c`: AST creation, printing and memory cleanup
- `semantic.c`: constant expression evaluation and semantic checks
- `io.c`: token table and source input
- `main.c`: compiler pipeline
- `gui.py`: one single-file Tkinter GUI for the project

## Compile manually
Linux:
`gcc -std=c11 -Wall -Wextra -pedantic main.c lexer.c parser.c ast.c semantic.c io.c -o switch_compiler`
`./switch_compiler`

Windows MinGW:
`gcc -std=c11 -Wall -Wextra -pedantic main.c lexer.c parser.c ast.c semantic.c io.c -o switch_compiler.exe`
`switch_compiler.exe`

## Run GUI
Keep `gui.py` and all C modules in the same folder, then run `python gui.py`.
The GUI compiles all C modules together when Compile & Analyze is clicked.

Use the GUI to manually test valid and invalid programs, then record actual outputs and screenshots in the report.
