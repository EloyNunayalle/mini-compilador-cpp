// Prueba de humo del DOM con jsdom (se omite si jsdom no esta instalado).
// Uso: API_BASE=http://127.0.0.1:8080 node test_frontend_dom.js
const assert = require("assert");
let JSDOM;
try { ({ JSDOM } = require("jsdom")); } catch (e) { console.log("omitida: jsdom no instalado"); process.exit(0); }
const base = (process.env.API_BASE || "http://127.0.0.1:8099") + "/";
const XSS = "<img src=x onerror=window.__pwned=1>";

(async () => {
  const dom = await JSDOM.fromURL(base, { runScripts: "dangerously", resources: "usable", pretendToBeVisual: true,
    beforeParse(w) {
      w.document.createRange = () => ({ setStart() {}, setEnd() {}, getBoundingClientRect: () => ({ left: 0, right: 0, top: 0, bottom: 0 }),
        getClientRects: () => [], commonAncestorContainer: w.document.body });
      w.Range.prototype.getClientRects = () => [];
      w.Range.prototype.getBoundingClientRect = () => ({ left: 0, right: 0, top: 0, bottom: 0 });
      w.HTMLElement.prototype.scrollIntoView = function () {};
      w.confirm = () => true;
      w.fetch = (u, o) => fetch(new URL(u, base), o);
      w.AbortController = AbortController;
    } });
  const w = dom.window, d = w.document;
  const errors = [];
  w.addEventListener("error", (e) => errors.push(e.message));
  await new Promise((r) => setTimeout(r, 1500));
  assert.ok(w.MCApp, "app inicializada");
  assert.ok(w.document.querySelectorAll("#example-select option").length > 5, "ejemplos cargados");
  assert.ok(w.MCApp.state.editor.getValue().includes("main"), "codigo inicial");

  const resp = {
    success: false, phase: "typecheck",
    errors: [{ phase: "typecheck", message: XSS, line: 2, column: 3 }, { phase: "run", message: XSS }],
    warnings: [{ phase: "typecheck", message: XSS, line: 1, column: 1 }],
    tokens: [{ type: XSS, lexeme: XSS, line: 1, column: 1 }],
    ast: { kind: XSS, label: XSS, type: XSS, children: [{ kind: "Hijo", label: XSS }, null] },
    typed_ast: { kind: "Program", attrs: { offset: XSS }, children: [] },
    layout: { records: [{ name: XSS, size: 8, align: 4, fields: [{ name: XSS, type: XSS, offset: 0, size: 4, align: 4 }] }],
      functions: [{ name: XSS, label: XSS, frame_size: 16, params: [{ name: XSS, type: "int", pass: XSS }], return: { type: XSS, pass: "VOID" } }] },
    assembly: "main:\n\tmovq " + XSS + ", %rax # " + XSS + "\n",
    optimizations: { enabled: true, passes: [XSS], stats: { fold: { count: 1, details: [XSS, { line: 1, description: XSS }] } }, log: [XSS] },
    stdout: XSS + "\n".repeat(3) + "x\n".repeat(1500), stderr: XSS, exit_code: 1, duration_ms: 3
  };
  w.fetch = async () => ({ status: 200, json: async () => resp });
  w.MCApp.state.editor.setValue("int main() {\n  int a;\n}\n");
  d.getElementById("btn-compile").click();
  await new Promise((r) => setTimeout(r, 400));

  assert.strictEqual(w.__pwned, undefined, "el payload no se ejecuto");
  assert.strictEqual(d.querySelectorAll(".panels img").length, 0, "no hay <img> inyectadas");
  const text = [...d.querySelectorAll(".panel")].map((p) => p.textContent).join("\n");
  assert.ok(text.includes(XSS), "el texto aparece literal");
  assert.ok(d.querySelectorAll("#panel-output .more-bar").length >= 1, "salida larga truncada con boton");
  assert.ok(d.querySelector("#panel-diag .diag-item"), "lista de diagnosticos");
  assert.strictEqual(d.querySelector("#badge-diag").hidden, false);
  assert.ok(d.querySelectorAll("#panel-tokens tbody tr").length === 1);
  assert.ok(d.querySelector("#panel-ast .tree"));
  assert.ok(d.querySelectorAll("#panel-asm .asm-line").length === 2);
  assert.ok(d.querySelectorAll("#panel-layout table").length >= 1, "tabla de campos");
  assert.ok(d.querySelector("#panel-layout .bytebar .bseg"), "diagrama de bytes");
  assert.ok(d.querySelector("#panel-layout .fn-card .slot"), "tarjeta de funcion con parametros");
  assert.ok(d.querySelector("#panel-diag .pipeline .pnode.fail"), "pipeline con fase fallida");
  assert.ok(d.querySelector("#panel-diag .snippet"), "fragmento de codigo en el error");
  assert.strictEqual(d.querySelectorAll("#panel-output .exit-badge").length + d.querySelectorAll("#panel-output .hero").length >= 1, true);
  assert.ok(d.querySelector("#panel-opt .fold"));
  assert.strictEqual(w.MCApp.state.marks.length, 2, "marcas de error y advertencia en el editor");
  assert.strictEqual(d.querySelector("#tab-diag").getAttribute("aria-selected"), "true");
  // teclado en pestanas
  d.querySelector("#tab-diag").dispatchEvent(new w.KeyboardEvent("keydown", { key: "ArrowRight", bubbles: true }));
  assert.strictEqual(d.querySelector("#tab-tokens").getAttribute("aria-selected"), "true");
  assert.strictEqual(d.querySelector(".rail-btn[data-view=tokens]").getAttribute("aria-current"), "page", "rail sincronizado");
  assert.strictEqual(d.getElementById("sb-errs").textContent, "2", "contador de errores en la barra de estado");
  assert.ok(d.querySelector("#panel-diag .pbar"), "barra de progreso del pipeline");
  // paleta de comandos con teclado
  d.dispatchEvent(new w.KeyboardEvent("keydown", { key: "k", ctrlKey: true, bubbles: true }));
  assert.strictEqual(d.getElementById("palette").hidden, false, "paleta abierta con Ctrl+K");
  const pin = d.getElementById("pal-input");
  pin.value = "tema osc";
  pin.dispatchEvent(new w.Event("input", { bubbles: true }));
  assert.ok(d.querySelectorAll("#pal-list [role=option]").length >= 1, "resultados difusos");
  assert.ok(d.querySelector("#pal-list [role=option][aria-selected=true]").textContent.includes("Tema oscuro"));
  pin.dispatchEvent(new w.KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
  await new Promise((r) => setTimeout(r, 50));
  assert.strictEqual(d.getElementById("palette").hidden, true, "paleta cerrada");
  assert.strictEqual(d.documentElement.getAttribute("data-theme"), "dark", "tema cambiado desde la paleta");
  // galeria de ejemplos
  d.getElementById("btn-examples").click();
  assert.ok(d.querySelectorAll("#ex-grid .ex-card").length > 5, "tarjetas de ejemplos");
  assert.ok(d.querySelector("#ex-grid .tag"), "etiquetas en tarjetas");
  // el texto del compilador nunca crea nodos HTML
  assert.strictEqual(d.querySelectorAll("#panel-diag img, #panel-layout img, #panel-opt img, #bubble img, #toast img").length, 0);
  assert.ok(["error", "enojada", "asustada"].includes(w.MCMascot.state()), "mascota reacciona al error: " + w.MCMascot.state());
  w.MCMascot.setEnabled(false);
  assert.strictEqual(d.getElementById("mascot-dock").classList.contains("off"), true, "mascota ocultable");
  // error de red
  w.fetch = async () => { throw new TypeError("fail"); };
  d.getElementById("btn-run").click();
  await new Promise((r) => setTimeout(r, 200));
  assert.ok(d.querySelector("#panel-diag").textContent.includes("No se pudo contactar"));
  assert.deepStrictEqual(errors, [], "sin errores de script: " + errors.join("; "));
  console.log("ok - frontend dom");
  w.close();
})().catch((e) => { console.error(e); process.exit(1); });
