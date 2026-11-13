// Pruebas de la logica pura del frontend (node, sin dependencias).
const assert = require("assert");
const path = require("path");
const fs = require("fs");
const L = require(path.join(__dirname, "..", "web", "frontend", "js", "lib.js"));
let n = 0;
function t(name, fn) { fn(); n++; console.log("ok - " + name); }

const XSS = '<img src=x onerror=alert(1)>';

t("escapeHtml", () => {
  assert.strictEqual(L.escapeHtml(XSS), "&lt;img src=x onerror=alert(1)&gt;");
  assert.strictEqual(L.escapeHtml('a&b"c\'d'), "a&amp;b&quot;c&#39;d");
});

t("formatCode indenta por llaves y conserva cadenas", () => {
  const out = L.formatCode('int main(){\nif(x){\ncout<<"{";\n}\n}\n');
  assert.strictEqual(out, 'int main(){\n    if(x){\n        cout<<"{";\n    }\n}\n');
});
t("formatCode: etiquetas de acceso, preprocesador y lineas vacias", () => {
  const out = L.formatCode("#include <x>\n\n\n\nclass A {\npublic:\nint x;\n};\n");
  assert.strictEqual(out, "#include <x>\n\nclass A {\npublic:\n    int x;\n};\n");
});
t("formatCode es idempotente", () => {
  const once = L.formatCode("int f(int a,\nint b){\nreturn a;\n}\n");
  assert.strictEqual(L.formatCode(once), once);
});

t("tokenizeAsmLine conserva el texto exacto", () => {
  const lines = ["\tmovq $8, %rax # c <b>", 'msg: .string "a#b<img>"', "  call puts@PLT", "", ".L1:", "\tmovl -4(%rbp), %eax"];
  lines.forEach((ln) => assert.strictEqual(L.tokenizeAsmLine(ln).map((s) => s.s).join(""), ln));
  const segs = L.tokenizeAsmLine("\tmovq $8, %rax");
  assert.deepStrictEqual(segs.filter((s) => s.t !== "ws" && s.t !== "text").map((s) => s.t), ["instr", "imm", "reg"]);
  assert.ok(L.tokenizeAsmLine("main:").some((s) => s.t === "label"));
  assert.ok(L.tokenizeAsmLine("  .globl main").some((s) => s.t === "directive"));
});
t("tokenizeAsmLine no interpreta HTML", () => {
  const segs = L.tokenizeAsmLine("\tmovq " + XSS);
  assert.strictEqual(segs.map((s) => s.s).join(""), "\tmovq " + XSS);
});

t("extractStrings", () => {
  const r = L.extractStrings('.section .rodata\n.LC0:\n\t.string "hola"\nmsg:\n\t.asciz "x y"\n');
  assert.deepStrictEqual(r.map((x) => [x.label, x.value]), [[".LC0", "hola"], ["msg", "x y"]]);
});

t("diffLines basico", () => {
  const d = L.diffLines(["a", "b", "c", "d"], ["a", "c", "d", "e"]);
  assert.deepStrictEqual(d.map((x) => x.op + x.text), ["=a", "-b", "=c", "=d", "+e"]);
  const s = L.summarizeDiff(d, 0);
  assert.strictEqual(s.added, 1);
  assert.strictEqual(s.removed, 1);
  assert.ok(s.rows.some((r) => r.op === "~"));
});
t("diffLines identicos y vacios", () => {
  assert.strictEqual(L.summarizeDiff(L.diffLines(["a"], ["a"])).rows.length, 0);
  assert.deepStrictEqual(L.diffLines([], ["x"]), [{ op: "+", text: "x" }]);
});
t("diffLines grande usa respaldo", () => {
  const a = Array.from({ length: 3000 }, (_, i) => "a" + i), b = Array.from({ length: 3000 }, (_, i) => "b" + i);
  const d = L.diffLines(a, b);
  assert.strictEqual(d.length, 6000);
});

const tree = { kind: "Program", children: [
  { kind: "FunDecl", label: "main", type: "int", line: 1, col: 1, children: [
    { kind: "VarDecl", label: "x", type: "int", const: true, attrs: { offset: -4, size: 4 }, children: [] },
    null, { kind: "ReturnStmt" }] }] };

t("normalizeNode tolera campos ausentes", () => {
  const n = L.normalizeNode({ kind: "X" });
  assert.deepStrictEqual([n.label, n.type, n.line, n.children.length, n.isConst], ["", "", null, 0, false]);
  assert.strictEqual(L.normalizeNode(null), null);
  assert.strictEqual(L.normalizeNode({ children: "no" }).children.length, 0);
});
t("countNodes ignora nulos", () => assert.strictEqual(L.countNodes(tree), 4));
t("searchTree devuelve rutas y expande ancestros", () => {
  const r = L.searchTree(tree, "vardecl");
  assert.deepStrictEqual(Object.keys(r.matches), ["0.0.0"]);
  assert.ok(r.expand["0"] && r.expand["0.0"]);
  assert.strictEqual(L.searchTree(tree, "").count, 0);
  assert.strictEqual(L.searchTree(tree, "offset=-4").count, 1);
  assert.strictEqual(L.searchTree(tree, "const").count, 1);
});
t("attrChips solo en variante con tipos", () => {
  assert.deepStrictEqual(L.attrChips({ offset: -8, size: 4, storage: "local", extra: true, o: {} }, true),
    ["offset -8", "size 4", "storage local", "extra true"]);
  assert.deepStrictEqual(L.attrChips({ offset: 1 }, false), []);
});

t("filterTokens", () => {
  const toks = [{ type: "KW_INT", lexeme: "int", line: 1 }, { type: "IDENTIFIER", lexeme: "x", line: 2 }];
  assert.strictEqual(L.filterTokens(toks, "kw_").length, 1);
  assert.strictEqual(L.filterTokens(toks, "2").length, 1);
  assert.strictEqual(L.filterTokens(toks, "").length, 2);
  assert.deepStrictEqual(L.filterTokens(null, "a"), []);
});

t("truncateLines", () => {
  const text = Array.from({ length: 1000 }, (_, i) => "l" + i).join("\n");
  const r = L.truncateLines(text, 10);
  assert.strictEqual(r.text.split("\n").length, 10);
  assert.strictEqual(r.total, 1000);
  assert.ok(r.truncated);
  assert.ok(!L.truncateLines("a\nb\n", 10).truncated);
  assert.strictEqual(L.countLines("a\nb\n"), 2);
});

t("phaseStatuses", () => {
  let s = L.phaseStatuses({ success: true, phase: "done", errors: [], duration_ms: 3 }, "all");
  assert.ok(L.PHASES.every((p) => s[p] === "ok"));
  s = L.phaseStatuses({ success: false, phase: "typecheck", errors: [{ phase: "typecheck" }] }, "all");
  assert.deepStrictEqual([s.lex, s.parse, s.typecheck, s.layout, s.run], ["ok", "ok", "fail", "skip", "skip"]);
  s = L.phaseStatuses({ success: true, phase: "done", errors: [] }, "scan");
  assert.deepStrictEqual([s.lex, s.parse], ["ok", "skip"]);
  s = L.phaseStatuses({ success: false, phase: "run", errors: [{ phase: "run", message: "x" }], duration_ms: 1 }, "run");
  assert.deepStrictEqual([s.gencode, s.assemble, s.run], ["ok", "ok", "fail"]);
  s = L.phaseStatuses({ success: true, phase: "done", errors: [] }, "asm");
  assert.deepStrictEqual([s.gencode, s.assemble], ["ok", "skip"]);
});

t("describeExit", () => {
  assert.strictEqual(L.describeExit({ exit_code: 0, duration_ms: 1 }).kind, "ok");
  assert.strictEqual(L.describeExit({ exit_code: 139, signal: "SIGSEGV", duration_ms: 1 }).kind, "signal");
  assert.strictEqual(L.describeExit({ exit_code: null, duration_ms: 5000, errors: [{ phase: "run", message: "tiempo limite excedido" }] }).kind, "timeout");
  assert.strictEqual(L.describeExit({ exit_code: 3, duration_ms: 1 }).kind, "nonzero");
  assert.strictEqual(L.describeExit({ exit_code: null, duration_ms: null }), null);
});

t("tokenEndColumn", () => {
  assert.strictEqual(L.tokenEndColumn("int foo = 1;", 5), 8);
  assert.strictEqual(L.tokenEndColumn('x = "ab";', 5), 9);
  assert.strictEqual(L.tokenEndColumn("a;", 2), 3);
  assert.strictEqual(L.tokenEndColumn("", 1), 2);
});

t("errorReport incluye fase, linea y columna", () => {
  const r = L.errorReport({ success: false, phase: "parse", errors: [{ phase: "parse", message: XSS, line: 3, column: 7 }], warnings: [] }, "check");
  assert.ok(r.includes("[Sintáctico] " + XSS + " (línea 3, columna 7)"));
});

t("el frontend propio no usa APIs de HTML sin escapar", () => {
  const dir = path.join(__dirname, "..", "web", "frontend");
  ["js/app.js", "js/lib.js", "js/theme-init.js", "index.html"].forEach((f) => {
    const src = fs.readFileSync(path.join(dir, f), "utf8");
    assert.ok(!/innerHTML|outerHTML|insertAdjacentHTML|document\.write|eval\(|new Function/.test(src), f);
  });
  const html = fs.readFileSync(path.join(dir, "index.html"), "utf8");
  assert.ok(!/<script(?![^>]*\bsrc=)[^>]*>/.test(html), "sin scripts en linea");
  assert.ok(!/\son\w+=/.test(html), "sin manejadores en linea");
});

t("sideBySide empareja borrados y altas", () => {
  const rows = L.sideBySide([{ op: "=", text: "a" }, { op: "-", text: "b" }, { op: "-", text: "c" }, { op: "+", text: "B" }, { op: "~", text: "3 lineas iguales" }, { op: "+", text: "z" }]);
  assert.deepStrictEqual(rows.map((r) => r.kind || "gap"), ["eq", "chg", "del", "gap", "add"]);
  assert.strictEqual(rows[1].l, "b");
  assert.strictEqual(rows[1].r, "B");
  assert.strictEqual(rows[2].r, null);
});

t("mascotReaction elige estado segun la respuesta", () => {
  const err = (n) => ({ success: false, errors: Array.from({ length: n }, () => ({ phase: "typecheck", message: "x" })) });
  assert.strictEqual(L.mascotReaction(null), null);
  assert.strictEqual(L.mascotReaction(err(2)).estado, "error");
  assert.strictEqual(L.mascotReaction(err(5)).estado, "enojada");
  assert.strictEqual(L.mascotReaction({ success: true, errors: [], warnings: [{}], stdout: "" , exit_code: 0, duration_ms: 1 }).estado, "sospecha");
  assert.strictEqual(L.mascotReaction({ success: false, errors: [{ phase: "run", message: "Tiempo limite excedido" }], exit_code: 124, duration_ms: 5000 }).estado, "asustada");
  assert.strictEqual(L.mascotReaction({ success: false, errors: [{ phase: "run", message: "x" }], signal: "SIGSEGV", exit_code: 139, duration_ms: 3 }).estado, "asustada");
  assert.strictEqual(L.mascotReaction({ success: true, errors: [], stdout: "x", stdout_truncated: true, exit_code: 0, duration_ms: 3 }).estado, "asustada");
  const ok = { success: true, errors: [], warnings: [], stdout: "hola", exit_code: 0, duration_ms: 2 };
  assert.strictEqual(L.mascotReaction(ok, { hadErrors: true }).festejo, true);
  assert.strictEqual(L.mascotReaction(ok, { firstSuccess: true }).festejo, true);
  assert.strictEqual(L.mascotReaction(ok, {}).festejo, false);
  assert.strictEqual(L.mascotReaction(ok, {}).estado, "feliz");
});

t("suggestFor ofrece pistas segun el diagnostico", () => {
  assert.ok(L.suggestFor({ message: "Identificador no declarado: 'x'" }).includes("Declare"));
  assert.ok(L.suggestFor({ message: "No se puede asignar un valor de tipo 'string'" }).includes("tipos"));
  assert.strictEqual(L.suggestFor({ message: "zzz", phase: "typecheck" }), "");
  assert.strictEqual(L.suggestFor(null), "");
  assert.ok(typeof L.suggestFor({ message: XSS }) === "string");
});

console.log(n + " pruebas correctas");
