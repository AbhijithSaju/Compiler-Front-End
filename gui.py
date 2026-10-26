import os, shutil, subprocess, threading, tkinter as tk
from tkinter import ttk, filedialog, messagebox

HERE=os.path.dirname(os.path.abspath(__file__))
FILES=["main.c","lexer.c","parser.c","ast.c","semantic.c","io.c"]
EXE=os.path.join(HERE,"switch_compiler.exe" if os.name=="nt" else "switch_compiler")
SAMPLE="""switch (x) {
    case 1:
        x = x + 1;
        break;
    case 2:
        break;
    default:
        x = 0;
}
"""
class App:
    def __init__(self,root):
        self.root=root; root.title("Switch Statement Compiler"); root.geometry("1150x760"); root.minsize(850,600)
        style=ttk.Style()
        try: style.theme_use("clam")
        except tk.TclError: pass
        style.configure("Title.TLabel",font=("Segoe UI",19,"bold"))
        style.configure("Primary.TButton",background="#2457d6",foreground="white",padding=8)
        head=ttk.Frame(root,padding=16); head.pack(fill="x")
        ttk.Label(head,text="Switch Statement Compiler",style="Title.TLabel").pack(anchor="w")
        ttk.Label(head,text="Modular C compiler · Lexer · Parser · AST · Semantic analysis").pack(anchor="w",pady=4)
        bar=ttk.Frame(root,padding=(16,4)); bar.pack(fill="x")
        for label,cmd in [("Load File",self.load),("Save Input",self.save),("Sample",self.sample),("Clear",self.clear)]:
            ttk.Button(bar,text=label,command=cmd).pack(side="left",padx=(0,6))
        ttk.Button(bar,text="Compile & Analyze",style="Primary.TButton",command=self.compile).pack(side="right")
        area=ttk.Frame(root,padding=16); area.pack(fill="both",expand=True); area.columnconfigure((0,1),weight=1); area.rowconfigure(1,weight=1)
        ttk.Label(area,text="SOURCE CODE").grid(row=0,column=0,sticky="w",pady=5)
        ttk.Label(area,text="COMPILER OUTPUT").grid(row=0,column=1,sticky="w",padx=(12,0),pady=5)
        left=ttk.Frame(area); left.grid(row=1,column=0,sticky="nsew")
        right=ttk.Frame(area); right.grid(row=1,column=1,sticky="nsew",padx=(12,0))
        self.editor=tk.Text(left,font=("Consolas",11),wrap="none",bg="#101828",fg="#e6edf7",insertbackground="white",padx=10,pady=10)
        self.output=tk.Text(right,font=("Consolas",10),wrap="none",state="disabled",padx=10,pady=10)
        self._scroll(left,self.editor); self._scroll(right,self.output)
        self.status=tk.StringVar(value="Ready."); ttk.Label(root,textvariable=self.status,relief="sunken",padding=7).pack(fill="x",side="bottom")
        self.editor.insert("1.0",SAMPLE)
    def _scroll(self,frame,widget):
        y=ttk.Scrollbar(frame,orient="vertical",command=widget.yview); x=ttk.Scrollbar(frame,orient="horizontal",command=widget.xview)
        widget.configure(yscrollcommand=y.set,xscrollcommand=x.set); widget.grid(row=0,column=0,sticky="nsew"); y.grid(row=0,column=1,sticky="ns"); x.grid(row=1,column=0,sticky="ew"); frame.rowconfigure(0,weight=1); frame.columnconfigure(0,weight=1)
    def show(self,s):
        self.output.configure(state="normal"); self.output.delete("1.0","end"); self.output.insert("1.0",s); self.output.configure(state="disabled")
    def load(self):
        p=filedialog.askopenfilename(filetypes=[("Source files","*.c *.txt"),("All files","*.*")])
        if p:
            try:
                with open(p,encoding="utf-8",errors="replace") as f: s=f.read()
                self.editor.delete("1.0","end"); self.editor.insert("1.0",s); self.status.set("Loaded "+p)
            except OSError as e: messagebox.showerror("Open error",str(e))
    def save(self):
        p=filedialog.asksaveasfilename(defaultextension=".txt",filetypes=[("Source files","*.txt *.c")])
        if p:
            try:
                with open(p,"w",encoding="utf-8") as f: f.write(self.editor.get("1.0","end-1c"))
                self.status.set("Saved "+p)
            except OSError as e: messagebox.showerror("Save error",str(e))
    def sample(self): self.editor.delete("1.0","end"); self.editor.insert("1.0",SAMPLE); self.status.set("Sample loaded.")
    def clear(self): self.editor.delete("1.0","end"); self.show(""); self.status.set("Cleared.")
    def compile(self):
        code=self.editor.get("1.0","end-1c")
        if not code.strip(): messagebox.showwarning("Empty input","Enter source code first."); return
        threading.Thread(target=self.worker,args=(code,),daemon=True).start(); self.status.set("Compiling modules...")
    def worker(self,code):
        try:
            missing=[f for f in FILES if not os.path.exists(os.path.join(HERE,f))]
            if missing: raise FileNotFoundError("Missing files: "+", ".join(missing))
            gcc=shutil.which("gcc")
            if not gcc: raise FileNotFoundError("GCC not found. Install GCC and add it to PATH.")
            cmd=[gcc,"-std=c11","-Wall","-Wextra","-pedantic"]+[os.path.join(HERE,f) for f in FILES]+["-o",EXE]
            build=subprocess.run(cmd,capture_output=True,text=True,timeout=40)
            if build.returncode: raise RuntimeError("Compilation failed:\\n"+build.stderr)
            result=subprocess.run([EXE],input=code+"\n",capture_output=True,text=True,timeout=15)
            out=result.stdout
            if result.stderr: out+="\\n--- DIAGNOSTICS ---\\n"+result.stderr
            self.root.after(0,self.show,out or "(No output)")
            self.root.after(0,self.status.set,"Analysis completed." if result.returncode==0 else "Compiler reported errors.")
        except Exception as e:
            self.root.after(0,self.show,str(e)); self.root.after(0,self.status.set,"Unable to compile or run.")
if __name__=="__main__":
    root=tk.Tk(); App(root); root.mainloop()
