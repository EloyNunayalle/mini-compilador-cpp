#!/usr/bin/env python3
"""Compilador falso con el mismo contrato de linea de comandos y JSON que el real.

Palabras clave en el codigo fuente para provocar comportamientos:
  ERROR_LEX, ERROR_PARSE, ERROR_TYPE   errores de cada fase
  WARN_DEMO                            una advertencia
  OPT_DEMO                             estadisticas de optimizacion no vacias
  RUN_STDIN, RUN_LOOP, RUN_BIG, RUN_SEGV, RUN_EXIT3, RUN_ASMERR
                                       comportamiento del programa generado
El ensamblado producido es x86-64 AT&T real, asi que se puede enlazar con gcc.
"""
import json
import re
import sys

KEYWORDS = {
    "int": "KW_INT", "double": "KW_DOUBLE", "char": "KW_CHAR", "bool": "KW_BOOL", "void": "KW_VOID",
    "if": "KW_IF", "else": "KW_ELSE", "while": "KW_WHILE", "for": "KW_FOR", "return": "KW_RETURN",
    "struct": "KW_STRUCT", "class": "KW_CLASS", "const": "KW_CONST", "new": "KW_NEW",
    "delete": "KW_DELETE", "using": "KW_USING", "namespace": "KW_NAMESPACE", "true": "KW_TRUE",
    "false": "KW_FALSE", "public": "KW_PUBLIC", "private": "KW_PRIVATE",
}
TOKEN_RE = re.compile(r'''
    (?P<ws>\s+)|(?P<comment>//[^\n]*)|(?P<preproc>\#[^\n]*)
   |(?P<num>\d+\.\d+|\d+)|(?P<id>[A-Za-z_]\w*)|(?P<str>"(?:\\.|[^"\\])*")
   |(?P<op><<|>>|==|!=|<=|>=|&&|\|\||\+\+|--|[-+*/%=<>!&|(){}\[\];,.:~])
''', re.X)
OP_NAMES = {"+": "PLUS", "-": "MINUS", "*": "STAR", "/": "SLASH", "%": "PERCENT", "=": "ASSIGN",
            "<": "LT", ">": "GT", "(": "LPAREN", ")": "RPAREN", "{": "LBRACE", "}": "RBRACE",
            "[": "LBRACKET", "]": "RBRACKET", ";": "SEMICOLON", ",": "COMMA", ".": "DOT",
            "<<": "SHL", ">>": "SHR", "==": "EQ", "!=": "NE", "<=": "LE", ">=": "GE"}
SIZES = {"int": 4, "double": 8, "char": 1, "bool": 1, "string": 16}


def loc(code, needle):
    i = code.find(needle)
    if i < 0:
        return 1, 1
    before = code[:i]
    return before.count("\n") + 1, i - (before.rfind("\n") + 1) + 1


def tokenize(code):
    tokens = []
    for m in TOKEN_RE.finditer(code):
        kind = m.lastgroup
        if kind in ("ws", "comment", "preproc"):
            continue
        text = m.group()
        line = code.count("\n", 0, m.start()) + 1
        col = m.start() - (code.rfind("\n", 0, m.start()) + 1) + 1
        if kind == "id":
            typ = KEYWORDS.get(text, "IDENTIFIER")
        elif kind == "num":
            typ = "FLOAT_LITERAL" if "." in text else "INT_LITERAL"
        elif kind == "str":
            typ = "STRING_LITERAL"
        else:
            typ = OP_NAMES.get(text, "OP")
        tokens.append({"type": typ, "lexeme": text, "line": line, "column": col})
    tokens.append({"type": "EOF", "lexeme": "", "line": code.count("\n") + 1, "column": 1})
    return tokens


def node(kind, line, col, label=None, typ=None, const=False, attrs=None, children=None):
    return {"kind": kind, "line": line, "col": col, "label": label, "type": typ, "const": const,
            "attrs": attrs or {}, "children": children or []}


def build_ast(code, typed):
    functions = []
    for m in re.finditer(r"^\s*(int|void|double|bool|char)\s+(\w+)\s*\(([^)]*)\)\s*\{", code, re.M):
        functions.append(m)
    unit = node("Program", 1, 1, "programa")
    lines = code.split("\n")
    for fm in functions:
        fl = code.count("\n", 0, fm.start()) + 1
        body = []
        offset = 0
        for ln in range(fl, len(lines)):
            text = lines[ln].strip()
            col = len(lines[ln]) - len(lines[ln].lstrip()) + 1
            v = re.match(r"(int|double|bool|char)\s+(\w+)\s*(?:=\s*([^;]+))?;", text)
            if v:
                offset -= SIZES[v.group(1)]
                attrs = {"offset": offset, "size": SIZES[v.group(1)], "align": SIZES[v.group(1)],
                         "storage": "local"} if typed else {}
                kids = []
                if v.group(3):
                    kids.append(node("IntLiteral" if v.group(3).strip().isdigit() else "Expression",
                                     ln + 1, col, v.group(3).strip(), v.group(1) if typed else None,
                                     v.group(3).strip().isdigit() and typed))
                body.append(node("VarDecl", ln + 1, col, v.group(2), v.group(1) if typed else None,
                                 False, attrs, kids))
            elif text.startswith("cout"):
                body.append(node("ExprStmt", ln + 1, col, "cout", None, False, {},
                                 [node("OutputExp", ln + 1, col, "<<")]))
            elif text.startswith("return"):
                body.append(node("ReturnStmt", ln + 1, col, "return",
                                 fm.group(1) if typed else None))
            if text == "}" and ln > fl:
                break
        attrs = {"frame_size": -offset} if typed else {}
        fn = node("FunDecl", fl, 1, fm.group(2), fm.group(1) if typed else None, False, attrs,
                  [node("Block", fl, len(fm.group(0)), "{}", None, False, {}, body)])
        unit["children"].append(fn)
    return unit


def build_layout(code):
    records = []
    for m in re.finditer(r"\b(struct|class)\s+(\w+)\s*\{(.*?)\n\};", code, re.S):
        fields = []
        off = 0
        align = 1
        for f in re.finditer(r"^\s*(?:const\s+)?(int|double|char|bool|string|\w+\s*\*)\s+(\w+)\s*(?:=[^;]*)?;",
                             m.group(3), re.M):
            t = f.group(1).replace(" ", "")
            size = 8 if t.endswith("*") else SIZES.get(t, 8)
            al = 8 if t in ("string",) or t.endswith("*") else size
            off = (off + al - 1) // al * al
            fields.append({"name": f.group(2), "type": t, "offset": off, "size": size, "align": al,
                           "const": False, "access": "private" if m.group(1) == "class" else "public"})
            off += size
            align = max(align, al)
        size = max(1, (off + align - 1) // align * align)
        has_str = any(x["type"] == "string" for x in fields)
        has_dtor = "~" + m.group(2) in code
        cls = "EMPTY" if not fields else "NON_TRIVIAL" if has_str or has_dtor else \
            "MEMORY" if size > 16 else "SSE" if all(x["type"] == "double" for x in fields) else "INTEGER"
        eb = [] if cls in ("MEMORY", "EMPTY", "NON_TRIVIAL") else \
            ["SSE" if cls == "SSE" else "INTEGER"] * ((size + 7) // 8)
        records.append({
            "name": m.group(2), "size": size, "align": align,
            "trivial_for_calls": cls in ("INTEGER", "SSE", "EMPTY", "MEMORY") and not has_dtor,
            "needs_dtor": has_str or has_dtor, "fields": fields,
            "special": {"default_ctor": "implicit", "param_ctor": "none",
                        "copy_ctor": "implicit", "copy_assign": "implicit",
                        "dtor": "user" if has_dtor else "implicit"},
            "abi": {"class": cls, "eightbytes": eb}})
    functions = []
    for m in re.finditer(r"^\s*(int|void|double|bool|char)\s+(\w+)\s*\(([^)]*)\)\s*\{", code, re.M):
        params = []
        ints = ["rdi", "rsi", "rdx", "rcx", "r8", "r9"]
        for i, p in enumerate([x.strip() for x in m.group(3).split(",") if x.strip()]):
            parts = p.rsplit(None, 1)
            t = parts[0]
            is_f = t == "double"
            params.append({"name": parts[-1], "type": t, "offset": -8 * (i + 1),
                           "pass": "SSE_REG" if is_f else "INT_REG",
                           "regs": ["xmm%d" % i] if is_f else [ints[i % 6]], "stack_offset": None})
        rt = m.group(1)
        functions.append({"name": m.group(2), "label": m.group(2), "frame_size": 16 + 16 * len(params),
                          "is_method": False, "owner": "", "params": params,
                          "return": {"type": rt, "pass": "VOID" if rt == "void" else
                                     "SSE_REG" if rt == "double" else "INT_REG"}})
    return {"records": records, "functions": functions}


def build_asm(code, optimize):
    lines = ["\t.text", "\t.globl main", "main:", "\tpushq %rbp", "\tmovq %rsp, %rbp"]
    pad = [] if optimize else ["\tmovq %rax, %rax", "\tnop"] * 3
    if "RUN_ASMERR" in code:
        lines.append("\tinstruccion_invalida %foo")
    elif "RUN_LOOP" in code:
        lines += ["1:", "\tjmp 1b"]
    elif "RUN_SEGV" in code:
        lines += ["\txorq %rax, %rax", "\tmovq (%rax), %rax"]
    elif "RUN_STDIN" in code:
        lines += ["1:", "\tcall getchar@PLT", "\tcmpl $-1, %eax", "\tje 2f", "\tmovl %eax, %edi",
                  "\tcall putchar@PLT", "\tjmp 1b", "2:"]
    elif "RUN_BIG" in code:
        lines += ["\tmovl $200000, %ebx", "1:", "\tleaq big(%rip), %rdi", "\tcall puts@PLT",
                  "\tdecl %ebx", "\tjnz 1b"]
    else:
        lines += ["\tleaq msg(%rip), %rdi", "\tcall puts@PLT"]
    lines += pad
    ret = 3 if "RUN_EXIT3" in code else 0
    lines += ["\tmovl $%d, %%eax" % ret, "\tpopq %rbp", "\tret", "\t.section .rodata",
              "msg:", '\t.string "hola desde el mock"',
              "big:", '\t.string "0123456789012345678901234567890123456789"',
              '\t.section .note.GNU-stack,"",@progbits']
    return "\n".join(lines) + "\n"


def main():
    args = sys.argv[1:]
    if "--version" in args:
        print("mock-compiler 0.1")
        return 0
    if "--api" not in args:
        sys.stderr.write("uso: mock_compiler.py --api --mode <modo> [-O0|-O1] [--passes a,b]\n")
        return 2
    mode = args[args.index("--mode") + 1] if "--mode" in args else "all"
    optimize = "-O1" in args
    passes = args[args.index("--passes") + 1].split(",") if "--passes" in args and optimize else \
        (["fold", "inline", "sethi"] if optimize else [])
    code = sys.stdin.read()

    out = {"success": True, "phase": "done", "errors": [], "warnings": [], "tokens": None, "ast": None,
           "typed_ast": None, "layout": None, "assembly": None,
           "optimizations": {"enabled": optimize, "passes": passes, "stats": {}, "log": [],
                             "ast_before": None, "ast_after": None}}

    def fail(phase, word, msg):
        line, col = loc(code, word)
        out["success"] = False
        out["phase"] = phase
        out["errors"] = [{"phase": phase, "message": msg, "line": line, "column": col}]
        print(json.dumps(out))
        return 0

    if "ERROR_LEX" in code:
        return fail("lex", "ERROR_LEX", "caracter no reconocido '@'")
    out["tokens"] = tokenize(code)
    if mode == "scan":
        print(json.dumps(out))
        return 0
    if "ERROR_PARSE" in code:
        return fail("parse", "ERROR_PARSE", "se esperaba ';' antes de este token")
    out["ast"] = build_ast(code, False)
    if mode == "ast":
        print(json.dumps(out))
        return 0
    if "ERROR_TYPE" in code or "edad = nombre;" in code:
        return fail("typecheck", "ERROR_TYPE" if "ERROR_TYPE" in code else "edad = nombre;", "no se puede asignar 'string' a 'int'")
    if "WARN_DEMO" in code:
        line, col = loc(code, "WARN_DEMO")
        out["warnings"] = [{"phase": "typecheck", "message": "variable declarada pero no usada",
                            "line": line, "column": col}]
    out["typed_ast"] = build_ast(code, True)
    out["layout"] = build_layout(code)
    out["phase"] = "layout"
    if mode == "check":
        print(json.dumps(out))
        return 0
    out["assembly"] = build_asm(code, optimize)
    out["phase"] = "done"
    if optimize:
        stats = {"fold": {"count": 0, "details": []}, "inline": {"count": 0, "details": []},
                 "sethi": {"count": 0, "details": []}}
        if "OPT_DEMO" in code:
            stats["fold"] = {"count": 2, "details": [
                {"line": 5, "description": "2 * 3 -> 6"}, {"line": 6, "description": "10 + 4 -> 14"}]}
            stats["inline"] = {"count": 1, "details": [{"line": 8, "description": "cuadrado(5) expandida"}]}
            stats["sethi"] = {"count": 1, "details": [{"line": 9, "description": "reordenada (a+b)*(c+d)"}]}
            out["optimizations"]["log"] = ["fold: 2 expresiones constantes", "inline: 1 llamada",
                                           "sethi: 1 expresion reordenada"]
            out["optimizations"]["ast_before"] = build_ast(code, True)
            out["optimizations"]["ast_after"] = build_ast(code, True)
        out["optimizations"]["stats"] = {k: v for k, v in stats.items() if k in passes}
    print(json.dumps(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
