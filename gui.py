import os
import subprocess
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

APP_TITLE = "Switch Statement Compiler"
HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE_FILE = os.path.join(HERE, "switch_compiler.c")
EXECUTABLE = os.path.join(HERE, "switch_compiler.exe" if os.name == "nt" else "switch_compiler")

SAMPLE = """switch (x) {
    case 1:
        x = x + 1;
        break;

    case 2:
        break;

    default:
        x = 0;
}
"""

class CompilerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1120x760")
        self.root.minsize(850, 600)
        self.root.configure(bg="#f3f5f9")
        self._style()
        self._build()
        self.set_status("Ready — enter source code, then click Compile & Analyze.")

    def _style(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("TFrame", background="#f3f5f9")
        style.configure("Card.TFrame", background="#ffffff")
        style.configure("TLabel", background="#f3f5f9", foreground="#172033", font=("Segoe UI", 10))
        style.configure("Title.TLabel", font=("Segoe UI", 20, "bold"), foreground="#14213d")
        style.configure("Sub.TLabel", font=("Segoe UI", 10), foreground="#667085")
        style.configure("TButton", font=("Segoe UI", 10, "bold"), padding=(12, 8))
        style.configure("Primary.TButton", background="#2457d6", foreground="#ffffff")
        style.map("Primary.TButton", background=[("active", "#1d45ad")])
        style.configure("Status.TLabel", background="#e8eefc", foreground="#243b70", padding=8)

    def _build(self):
        header = ttk.Frame(self.root, padding=(20, 16, 20, 8))
        header.pack(fill="x")
        ttk.Label(header, text="Switch Statement Compiler", style="Title.TLabel").pack(anchor="w")
        ttk.Label(header, text="Lexical analysis  •  Syntax analysis  •  AST  •  Semantic validation",
                  style="Sub.TLabel").pack(anchor="w", pady=(3, 0))

        toolbar = ttk.Frame(self.root, padding=(20, 8))
        toolbar.pack(fill="x")
        ttk.Button(toolbar, text="Load File", command=self.load_file).pack(side="left", padx=(0, 8))
        ttk.Button(toolbar, text="Save Input", command=self.save_input).pack(side="left", padx=(0, 8))
        ttk.Button(toolbar, text="Load Sample", command=self.load_sample).pack(side="left", padx=(0, 8))
        ttk.Button(toolbar, text="Clear", command=self.clear_all).pack(side="left", padx=(0, 8))
        ttk.Button(toolbar, text="Compile & Analyze", style="Primary.TButton",
                   command=self.compile_source).pack(side="right")

        body = ttk.Frame(self.root, padding=(20, 4, 20, 12))
        body.pack(fill="both", expand=True)
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(1, weight=1)

        ttk.Label(body, text="SOURCE CODE").grid(row=0, column=0, sticky="w", pady=(0, 6))
        ttk.Label(body, text="COMPILER OUTPUT").grid(row=0, column=1, sticky="w", padx=(12, 0), pady=(0, 6))

        editor_frame = ttk.Frame(body, style="Card.TFrame")
        editor_frame.grid(row=1, column=0, sticky="nsew")
        output_frame = ttk.Frame(body, style="Card.TFrame")
        output_frame.grid(row=1, column=1, sticky="nsew", padx=(12, 0))

        self.editor = tk.Text(editor_frame, wrap="none", undo=True, font=("Consolas", 11),
                              bg="#101828", fg="#e6edf7", insertbackground="#ffffff",
                              selectbackground="#344b75", padx=12, pady=12,
                              borderwidth=0, relief="flat")
        editor_y = ttk.Scrollbar(editor_frame, orient="vertical", command=self.editor.yview)
        editor_x = ttk.Scrollbar(editor_frame, orient="horizontal", command=self.editor.xview)
        self.editor.configure(yscrollcommand=editor_y.set, xscrollcommand=editor_x.set)
        self.editor.grid(row=0, column=0, sticky="nsew")
        editor_y.grid(row=0, column=1, sticky="ns")
        editor_x.grid(row=1, column=0, sticky="ew")
        editor_frame.rowconfigure(0, weight=1)
        editor_frame.columnconfigure(0, weight=1)

        self.output = tk.Text(output_frame, wrap="none", state="disabled", font=("Consolas", 10),
                              bg="#ffffff", fg="#172033", padx=12, pady=12,
                              borderwidth=0, relief="flat")
        output_y = ttk.Scrollbar(output_frame, orient="vertical", command=self.output.yview)
        output_x = ttk.Scrollbar(output_frame, orient="horizontal", command=self.output.xview)
        self.output.configure(yscrollcommand=output_y.set, xscrollcommand=output_x.set)
        self.output.grid(row=0, column=0, sticky="nsew")
        output_y.grid(row=0, column=1, sticky="ns")
        output_x.grid(row=1, column=0, sticky="ew")
        output_frame.rowconfigure(0, weight=1)
        output_frame.columnconfigure(0, weight=1)

        self.output.tag_configure("success", foreground="#137547", font=("Consolas", 10, "bold"))
        self.output.tag_configure("error", foreground="#b42318", font=("Consolas", 10, "bold"))

        self.status = ttk.Label(self.root, text="", style="Status.TLabel", anchor="w")
        self.status.pack(fill="x", side="bottom")

        self.editor.insert("1.0", SAMPLE)
        self.editor.focus_set()

    def set_status(self, message):
        self.status.configure(text=message)

    def _set_output(self, text):
        self.output.configure(state="normal")
        self.output.delete("1.0", "end")
        self.output.insert("1.0", text)
        if "Result: SUCCESS" in text:
            start = "1.0"
            while True:
                pos = self.output.search("Result: SUCCESS", start, stopindex="end")
                if not pos:
                    break
                end = f"{pos}+{len('Result: SUCCESS')}c"
                self.output.tag_add("success", pos, end)
                start = end
        if "ERROR]" in text or "Result: FAILED" in text:
            start = "1.0"
            while True:
                pos = self.output.search("ERROR]", start, stopindex="end")
                if not pos:
                    break
                end = f"{pos}+6c"
                self.output.tag_add("error", pos, end)
                start = end
        self.output.configure(state="disabled")

    def load_file(self):
        path = filedialog.askopenfilename(
            title="Open source file",
            filetypes=[("C/source files", "*.c *.txt *.cpp"), ("All files", "*.*")]
        )
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
            self.editor.delete("1.0", "end")
            self.editor.insert("1.0", content)
            self.set_status(f"Loaded: {path}")
        except OSError as exc:
            messagebox.showerror("Open failed", str(exc))

    def save_input(self):
        path = filedialog.asksaveasfilename(
            title="Save source input",
            defaultextension=".txt",
            filetypes=[("Source files", "*.txt *.c"), ("All files", "*.*")]
        )
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.editor.get("1.0", "end-1c"))
            self.set_status(f"Saved: {path}")
        except OSError as exc:
            messagebox.showerror("Save failed", str(exc))

    def load_sample(self):
        self.editor.delete("1.0", "end")
        self.editor.insert("1.0", SAMPLE)
        self.set_status("Sample program loaded.")

    def clear_all(self):
        self.editor.delete("1.0", "end")
        self._set_output("")
        self.set_status("Cleared.")

    def compile_source(self):
        code = self.editor.get("1.0", "end-1c")
        if not code.strip():
            messagebox.showwarning("No source code", "Enter or load a source program first.")
            return
        threading.Thread(target=self._run_compiler, args=(code,), daemon=True).start()
        self.set_status("Compiling and analyzing source...")

    def _run_compiler(self, code):
        try:
            if not os.path.isfile(SOURCE_FILE):
                raise FileNotFoundError(
                    "switch_compiler.c was not found beside gui.py. Keep both files in the same folder."
                )
            if not os.path.isfile(EXECUTABLE):
                compiler = shutil_which("gcc")
                if not compiler:
                    raise FileNotFoundError(
                        "GCC was not found. Install GCC and add it to PATH, then reopen this GUI."
                    )
                command = [compiler, "-std=c11", "-Wall", "-Wextra", "-pedantic",
                           SOURCE_FILE, "-o", EXECUTABLE]
                build = subprocess.run(command, capture_output=True, text=True, timeout=30)
                if build.returncode != 0:
                    raise RuntimeError("Could not compile compiler source:\n" + build.stderr)
            result = subprocess.run([EXECUTABLE], input=code + "\n",
                                    capture_output=True, text=True, timeout=15)
            combined = result.stdout
            if result.stderr:
                combined += "\n--- DIAGNOSTICS ---\n" + result.stderr
            if not combined.strip():
                combined = "(Compiler returned no output.)"
            self.root.after(0, self._set_output, combined)
            status = "Compilation analysis completed." if result.returncode == 0 else \
                     f"Compiler reported errors (exit code {result.returncode})."
            self.root.after(0, self.set_status, status)
        except Exception as exc:
            self.root.after(0, self._set_output, str(exc))
            self.root.after(0, self.set_status, "Could not run compiler.")

def shutil_which(program):
    # Small wrapper avoids requiring any third-party Python package.
    import shutil
    return shutil.which(program)

def main():
    root = tk.Tk()
    CompilerGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
