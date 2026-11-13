/* Interfaz del editor. Todo el texto proveniente del usuario, del compilador o del
 * programa ejecutado se inserta con textContent o createTextNode, nunca como HTML. */
(function () {
  "use strict";

  var L = window.MC;
  var M = window.MCMascot;
  var STORE_KEY = "mcpp.v1";
  var SVGNS = "http://www.w3.org/2000/svg";
  var DEFAULT_CODE = "#include <iostream>\nusing namespace std;\n\nint main() {\n    cout << \"Hola\\n\";\n    return 0;\n}\n";
  var OUT_LINES = 400;
  var ASM_LINES = 2000;
  var TOKEN_ROWS = 500;
  var FIELD_HUES = [226, 158, 282, 340, 42, 190, 8, 100];

  var state = {
    editor: null, store: {}, status: null, running: false, abort: null, abortReason: null,
    last: null, compare: null, examples: [], marks: [], gutterLines: [], ticker: null,
    trees: {}, asmLines: [], asmHits: [], asmHitIdx: -1, tokenFilter: "", programmatic: false,
    exampleId: null, tab: "output", hadErrors: false, seed: 0, skelTimer: null, emptyM: null
  };

  function $(id) { return document.getElementById(id); }

  function prefersReduced() {
    return !!(window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches);
  }

  function el(tag, opts, kids) {
    var n = document.createElement(tag);
    opts = opts || {};
    if (opts.cls) n.className = opts.cls;
    if (opts.text != null) n.textContent = opts.text;
    if (opts.attrs) Object.keys(opts.attrs).forEach(function (k) { n.setAttribute(k, opts.attrs[k]); });
    if (opts.on) Object.keys(opts.on).forEach(function (k) { n.addEventListener(k, opts.on[k]); });
    (kids || []).forEach(function (c) { if (c) n.appendChild(typeof c === "string" ? document.createTextNode(c) : c); });
    return n;
  }

  function clear(n) { while (n.firstChild) n.removeChild(n.firstChild); }

  /* Icono del sprite definido en index.html. */
  function icon(name, cls) {
    var s = document.createElementNS(SVGNS, "svg");
    s.setAttribute("class", "i" + (cls ? " " + cls : ""));
    s.setAttribute("aria-hidden", "true");
    var u = document.createElementNS(SVGNS, "use");
    u.setAttribute("href", "#i-" + name);
    s.appendChild(u);
    return s;
  }

  function svgEl(tag, attrs) {
    var n = document.createElementNS(SVGNS, tag);
    Object.keys(attrs || {}).forEach(function (k) { n.setAttribute(k, attrs[k]); });
    return n;
  }

  function button(text, onclick, cls, ico) {
    var kids = [];
    if (ico) kids.push(icon(ico));
    kids.push(document.createTextNode(text));
    return el("button", { cls: "btn small" + (cls ? " " + cls : ""), attrs: { type: "button" }, on: { click: onclick } }, kids);
  }

  function chip(text, cls) { return el("span", { cls: "chip" + (cls ? " " + cls : ""), text: text }); }

  /* Almacenamiento local */
  function loadStore() {
    try { state.store = JSON.parse(localStorage.getItem(STORE_KEY) || "{}") || {}; }
    catch (e) { state.store = {}; }
  }
  var saveTimer = null;
  function saveStore(patch, immediate) {
    Object.keys(patch || {}).forEach(function (k) { state.store[k] = patch[k]; });
    var write = function () {
      try { localStorage.setItem(STORE_KEY, JSON.stringify(state.store)); } catch (e) { /* sin almacenamiento */ }
    };
    if (immediate) { clearTimeout(saveTimer); write(); return; }
    clearTimeout(saveTimer);
    saveTimer = setTimeout(write, 300);
  }

  /* Avisos: entran con transicion y salen mas rapido por el mismo camino. */
  function toast(msg, kind) {
    var host = $("toast");
    while (host.children.length >= 3) host.removeChild(host.firstChild);
    var t = el("div", { cls: "toast" + (kind === "err" ? " err" : "") }, [icon(kind === "err" ? "alert" : "check"), el("span", { text: msg })]);
    host.appendChild(t);
    requestAnimationFrame(function () { requestAnimationFrame(function () { t.classList.add("in"); }); });
    setTimeout(function () {
      t.classList.remove("in");
      setTimeout(function () { if (t.parentNode) t.parentNode.removeChild(t); }, 180);
    }, 2600);
  }

  /* Estado de ejecucion: texto visible y, aparte, texto para lectores de pantalla. */
  function setStatus(text, kind, silent) {
    var s = $("run-status");
    s.className = "run-status" + (kind ? " " + kind : "");
    s.firstChild.textContent = text || "";
    if (!silent) s.lastChild.textContent = text || "";
  }

  function setMiniPipe(statuses, busy) {
    var pipe = $("mini-pipe");
    pipe.classList.toggle("busy", !!busy);
    Array.prototype.forEach.call(pipe.children, function (n, i) {
      n.className = !busy && statuses ? (statuses[L.PHASES[i]] === "skip" ? "" : statuses[L.PHASES[i]]) : "";
    });
  }

  function copyText(text, what) {
    function fallback() {
      var ta = el("textarea", { attrs: { "aria-hidden": "true", tabindex: "-1" } });
      ta.value = text;
      ta.style.position = "fixed";
      ta.style.opacity = "0";
      document.body.appendChild(ta);
      ta.select();
      var ok = false;
      try { ok = document.execCommand("copy"); } catch (e) { ok = false; }
      document.body.removeChild(ta);
      if (ok) toast(what + " copiado"); else toast("No se pudo copiar", "err");
    }
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(text).then(function () { toast(what + " copiado"); }, fallback);
    } else fallback();
  }

  function download(name, text, mime) {
    var blob = new Blob([text], { type: mime || "text/plain;charset=utf-8" });
    var url = URL.createObjectURL(blob);
    var a = el("a", { attrs: { href: url, download: name } });
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
    toast("Descargando " + name);
  }

  /* Controles segmentados con indicador deslizante */
  function placeSeg(seg) {
    var opts = seg.querySelectorAll(".seg-opt");
    var ind = seg.querySelector(".seg-ind");
    var idx = 0;
    Array.prototype.forEach.call(opts, function (o, i) { if (o.querySelector("input").checked) idx = i; });
    seg.style.setProperty("--n", String(opts.length));
    ind.style.transform = "translateX(" + idx * 100 + "%)";
  }

  function initSegs() {
    var segs = document.querySelectorAll(".seg");
    Array.prototype.forEach.call(segs, function (seg) {
      placeSeg(seg);
      seg.addEventListener("change", function () { placeSeg(seg); });
    });
    requestAnimationFrame(function () {
      Array.prototype.forEach.call(segs, function (seg) { seg.classList.add("ready"); });
    });
  }

  /* Editor */
  function initEditor() {
    var run = function () { execute("run"); };
    var save = function () { persistNow(); toast("Guardado en este navegador"); };
    state.editor = CodeMirror.fromTextArea($("code"), {
      mode: "text/x-c++src", lineNumbers: true, indentUnit: 4, tabSize: 4, indentWithTabs: false,
      matchBrackets: true, autoCloseBrackets: true, styleActiveLine: true, lineWrapping: false,
      gutters: ["mc-gutter", "CodeMirror-linenumbers"], inputStyle: "contenteditable",
      extraKeys: {
        "Ctrl-Enter": run, "Cmd-Enter": run, "Ctrl-S": save, "Cmd-S": save,
        "Tab": "insertSoftTab", "Shift-Tab": "indentLess", "Ctrl-/": "toggleComment", "Cmd-/": "toggleComment",
        "Ctrl-G": "jumpToLine", "Cmd-G": "jumpToLine"
      }
    });
    var ed = state.editor;
    ed.getInputField().setAttribute("aria-label", "Código fuente C++");
    ed.on("change", function () {
      if (!state.programmatic) { clearMarks(); if (M) M.typing(); }
      saveStore({ code: ed.getValue() });
    });
    ed.on("focus", function () { if (M) M.focus(true); });
    ed.on("blur", function () { if (M) M.focus(false); });
    ed.on("cursorActivity", function () {
      var c = ed.getCursor();
      $("cursor-pos").textContent = "Ln " + (c.line + 1) + ", Col " + (c.ch + 1);
    });
    if (window.ResizeObserver) new ResizeObserver(function () { ed.refresh(); }).observe($("editor-region"));
  }

  function setCode(code) {
    state.programmatic = true;
    state.editor.setValue(code);
    state.editor.clearHistory();
    state.programmatic = false;
    clearMarks();
  }

  function persistNow() {
    saveStore({ code: state.editor.getValue(), stdin: $("stdin").value, opts: readOpts() }, true);
  }

  function clearMarks() {
    var ed = state.editor;
    if (!ed) return;
    ed.operation(function () {
      state.marks.forEach(function (m) { m.clear(); });
      state.gutterLines.forEach(function (g) {
        ed.setGutterMarker(g.line, "mc-gutter", null);
        ed.removeLineClass(g.line, "background", g.cls);
      });
    });
    state.marks = [];
    state.gutterLines = [];
  }

  function applyMarks(resp) {
    clearMarks();
    var ed = state.editor;
    var items = [];
    (resp.errors || []).forEach(function (e) { items.push({ d: e, warn: false }); });
    (resp.warnings || []).forEach(function (e) { items.push({ d: e, warn: true }); });
    ed.operation(function () {
      items.forEach(function (it) {
        var d = it.d || {};
        if (!(d.line >= 1) || d.line > ed.lineCount()) return;
        var ln = d.line - 1;
        var col = d.column >= 1 ? d.column : 1;
        var text = ed.getLine(ln) || "";
        var end = L.tokenEndColumn(text, col);
        var from = { line: ln, ch: Math.min(col - 1, text.length) };
        var to = { line: ln, ch: Math.min(Math.max(end - 1, from.ch + 1), Math.max(text.length, from.ch + 1)) };
        var cls = it.warn ? "mc-line-warn" : "mc-line-error";
        state.marks.push(ed.markText(from, to, {
          className: it.warn ? "cm-warn-underline" : "cm-error-underline",
          title: (d.message || "")
        }));
        var existing = ed.lineInfo(ln);
        if (!existing.gutterMarkers || !existing.gutterMarkers["mc-gutter"] || !it.warn) {
          var mk = el("span", { cls: "mc-marker" + (it.warn ? " warn" : ""), attrs: { title: d.message || "" } });
          ed.setGutterMarker(ln, "mc-gutter", mk);
        }
        ed.addLineClass(ln, "background", cls);
        state.gutterLines.push({ line: ln, cls: cls });
      });
    });
  }

  function goTo(line, col, focus) {
    var ed = state.editor;
    if (!ed || !(line >= 1)) return;
    var l = Math.min(line, ed.lineCount()) - 1;
    var c = Math.max((col || 1) - 1, 0);
    ed.scrollIntoView({ line: l, ch: c }, 140);
    ed.setCursor({ line: l, ch: c });
    ed.addLineClass(l, "background", "mc-line-focus");
    setTimeout(function () { ed.removeLineClass(l, "background", "mc-line-focus"); }, 2200);
    if (focus !== false) ed.focus();
  }

  /* Opciones */
  function readOpts() {
    var passes = [];
    document.querySelectorAll(".pass").forEach(function (c) { if (c.checked) passes.push(c.value); });
    return {
      opt: document.querySelector("input[name=opt]:checked").value,
      passes: passes, mode: $("mode-select").value, timeout: parseInt($("timeout-input").value, 10) || 5
    };
  }

  function applyOpts(o) {
    if (!o) return;
    var r = document.querySelector("input[name=opt][value='" + (o.opt === "1" ? "1" : "0") + "']");
    if (r) r.checked = true;
    if (Array.isArray(o.passes)) {
      document.querySelectorAll(".pass").forEach(function (c) { c.checked = o.passes.indexOf(c.value) >= 0; });
    }
    if (o.mode && $("mode-select").querySelector("option[value='" + o.mode + "']")) $("mode-select").value = o.mode;
    if (o.timeout >= 1 && o.timeout <= 10) $("timeout-input").value = o.timeout;
    syncPassState();
    syncModeChips();
    placeSeg($("opt-seg"));
    updateStatusbar();
  }

  function syncModeChips() {
    var cur = $("mode-select").value;
    Array.prototype.forEach.call(document.querySelectorAll(".mode-chip"), function (c) {
      c.setAttribute("aria-checked", c.dataset.mode === cur ? "true" : "false");
      c.tabIndex = c.dataset.mode === cur ? 0 : -1;
    });
  }

  function syncPassState() {
    var on = document.querySelector("input[name=opt]:checked").value === "1";
    document.querySelectorAll(".pass").forEach(function (c) { c.disabled = !on; });
  }

  /* Peticion al servidor */
  async function postCompile(body, signal) {
    var res;
    try {
      res = await fetch("api/compile", {
        method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), signal: signal
      });
    } catch (e) {
      if (e && e.name === "AbortError") throw e;
      return { netError: "No se pudo contactar con el servidor. Compruebe que el servicio está en ejecución." };
    }
    var data = null;
    try { data = await res.json(); } catch (e) { data = null; }
    if (!data || typeof data !== "object") {
      return { netError: "El servidor respondió con un formato inesperado (HTTP " + res.status + ")." };
    }
    if (res.status === 429) {
      data = { success: false, errors: [{ phase: "request", message: "El servidor está ocupado. Intente de nuevo en unos segundos." }] };
    }
    return { status: res.status, data: data };
  }

  function buildRequest(mode, extra) {
    var o = readOpts();
    var opt = o.opt === "1" && o.passes.length > 0;
    var req = {
      code: state.editor.getValue(), mode: mode, optimize: opt, passes: opt ? o.passes : [],
      stdin: $("stdin").value, timeout: Math.min(10, Math.max(1, o.timeout))
    };
    if (extra) Object.keys(extra).forEach(function (k) { req[k] = extra[k]; });
    return req;
  }

  function effectiveMode(kind) {
    var sel = $("mode-select").value;
    var mode;
    if (kind === "run") mode = sel;
    else mode = (sel === "all" || sel === "run") ? "asm" : sel;
    if ((mode === "all" || mode === "run") && state.status && state.status.run_available === false) {
      toast("La ejecución no está disponible en este servidor; se genera solo el ensamblado.", "err");
      return "asm";
    }
    return mode;
  }

  function startBusy(label, timeoutS) {
    state.running = true;
    var run = $("btn-run");
    run.disabled = true;
    run.classList.add("running");
    run.setAttribute("aria-busy", "true");
    $("btn-compile").disabled = true;
    $("btn-cancel").disabled = false;
    setMiniPipe(null, true);
    $("sb-phase").textContent = "Compilando…";
    var t0 = Date.now();
    var tick = function () {
      setStatus(label + " " + ((Date.now() - t0) / 1000).toFixed(1) + " s", "busy", true);
    };
    setStatus(label, "busy");
    tick();
    state.ticker = setInterval(tick, 200);
    state.abort = new AbortController();
    state.abortReason = null;
    clearTimeout(state.skelTimer);
    state.skelTimer = setTimeout(function () { $("skeleton").classList.add("show"); }, 700);
    if (M) M.show("cargando");
    var clientLimit = setTimeout(function () { state.abortReason = "timeout"; state.abort.abort(); }, (timeoutS + 25) * 1000);
    return clientLimit;
  }

  function endBusy(limitTimer) {
    clearTimeout(limitTimer);
    clearInterval(state.ticker);
    clearTimeout(state.skelTimer);
    $("skeleton").classList.remove("show");
    state.running = false;
    state.abort = null;
    var run = $("btn-run");
    run.disabled = false;
    run.classList.remove("running");
    run.removeAttribute("aria-busy");
    $("btn-compile").disabled = false;
    $("btn-cancel").disabled = true;
    updateStatusbar();
    if (M) M.done();
  }

  function mascotReact(r, mode) {
    if (!M) return;
    var reaction = L.mascotReaction(r, { hadErrors: state.hadErrors, firstSuccess: !state.store.everOk });
    if (!reaction) return;
    M.show(reaction.estado, { festejo: reaction.festejo });
    var nErr = (r.errors || []).length;
    if (nErr > 0 || r.success === false) state.hadErrors = true;
    else if (reaction.estado === "feliz") {
      state.hadErrors = false;
      if (!state.store.everOk) saveStore({ everOk: true }, true);
    }
  }

  async function execute(kind) {
    if (state.running) return;
    var mode = effectiveMode(kind);
    var req = buildRequest(mode);
    persistNow();
    var limit = startBusy(mode === "all" || mode === "run" ? "Ejecutando…" : "Compilando…", req.timeout);
    var out;
    try {
      out = await postCompile(req, state.abort.signal);
    } catch (e) {
      endBusy(limit);
      var timedOut = state.abortReason === "timeout";
      setMiniPipe(null, false);
      setStatus(timedOut ? "El servidor tardó demasiado en responder." : "Cancelado.", "error");
      showFailure(timedOut ? "La solicitud superó el tiempo máximo de espera del navegador." : "La operación fue cancelada.");
      if (M) M.show(timedOut ? "asustada" : "reposo");
      return;
    }
    endBusy(limit);
    if (out.netError) {
      setMiniPipe(null, false);
      setStatus("Error de red.", "error");
      showFailure(out.netError);
      if (M) M.show("asustada");
      return;
    }
    state.last = { resp: out.data, mode: mode, req: req, http: out.status };
    state.compare = null;
    var r = out.data;
    var nErr = (r.errors || []).length;
    setStatus(r.success ? "Listo" + (r.duration_ms != null ? " (ejecución " + L.formatMs(r.duration_ms) + ")" : "")
      : "Terminó con " + nErr + " error" + (nErr === 1 ? "" : "es"), r.success ? "" : "error");
    setMiniPipe(L.phaseStatuses(r, mode), false);
    applyMarks(r);
    renderAll();
    selectTab(chooseTab(r, mode));
    mascotReact(r, mode);
  }

  function chooseTab(r, mode) {
    if (!r.success && (r.errors || []).length) return "diag";
    return { scan: "tokens", ast: "ast", check: "typed", asm: "asm" }[mode] || "output";
  }

  function showFailure(msg) {
    state.last = { resp: { success: false, phase: "request", errors: [{ phase: "request", message: msg }], warnings: [] },
      mode: null, req: null };
    renderAll();
    selectTab("diag");
  }

  /* Pestanas */
  function placeTabInd() {
    var tabs = $("tablist");
    var sel = tabs.querySelector("[role=tab][aria-selected=true]");
    var ind = $("tab-ind");
    if (!sel || !ind) return;
    ind.style.transform = "translateX(" + sel.offsetLeft + "px) scaleX(" + (sel.offsetWidth / 100).toFixed(3) + ")";
    var left = sel.offsetLeft, right = left + sel.offsetWidth;
    if (left < tabs.scrollLeft) tabs.scrollLeft = Math.max(0, left - 12);
    else if (right > tabs.scrollLeft + tabs.clientWidth) tabs.scrollLeft = right - tabs.clientWidth + 12;
  }

  function flushPending(panel) {
    var list = panel._pending || [];
    panel._pending = [];
    list.forEach(function (fn) { fn(); });
  }

  function selectTab(name, focus, animate) {
    var prev = state.tab;
    document.querySelectorAll("#tablist [role=tab]").forEach(function (t) {
      var on = t.dataset.tab === name;
      t.setAttribute("aria-selected", on ? "true" : "false");
      t.tabIndex = on ? 0 : -1;
      if (on && focus) t.focus();
    });
    document.querySelectorAll(".panels .panel").forEach(function (p) {
      var show = p.id === "panel-" + name;
      p.hidden = !show;
      p.classList.remove("enter");
      if (show && animate && prev !== name && !prefersReduced()) {
        void p.offsetWidth;
        p.classList.add("enter");
      }
      if (show) flushPending(p);
    });
    state.tab = name;
    placeTabInd();
    syncRail(name);
  }

  /* El rail refleja la pestana activa; el indicador se desliza entre iconos. */
  function syncRail(name) {
    var rail = $("rail"), ind = $("rail-ind"), active = null;
    Array.prototype.forEach.call(rail.querySelectorAll(".rail-btn[data-view]"), function (b) {
      var on = b.dataset.view === name || (name === "typed" && b.dataset.view === "ast");
      if (on) { b.setAttribute("aria-current", "page"); active = b; } else b.removeAttribute("aria-current");
    });
    if (!active) return;
    var mobile = window.matchMedia && window.matchMedia("(max-width: 900px)").matches;
    ind.style.transform = mobile
      ? "translateX(" + Math.round(active.offsetLeft + active.offsetWidth / 2 - 12) + "px)"
      : "translateY(" + Math.round(active.offsetTop + active.offsetHeight / 2 - 12) + "px)";
  }

  function updateStatusbar() {
    $("sb-opt").textContent = readOpts().opt === "1" ? "-O1" : "-O0";
    var t = "Sin compilar";
    if (state.running) t = "Compilando…";
    else if (state.last) {
      var r = state.last.resp;
      t = r.success ? "Compilación correcta" + (r.duration_ms != null ? " · " + L.formatMs(r.duration_ms) : "") : "Falló en " + L.phaseLabel(r.phase);
    }
    $("sb-phase").textContent = t;
  }

  function initTabs() {
    var tabs = Array.prototype.slice.call(document.querySelectorAll("#tablist [role=tab]"));
    tabs.forEach(function (t, i) {
      t.addEventListener("click", function (ev) { selectTab(t.dataset.tab, false, ev.detail > 0); });
      t.addEventListener("keydown", function (ev) {
        var j = null;
        if (ev.key === "ArrowRight") j = (i + 1) % tabs.length;
        else if (ev.key === "ArrowLeft") j = (i - 1 + tabs.length) % tabs.length;
        else if (ev.key === "Home") j = 0;
        else if (ev.key === "End") j = tabs.length - 1;
        if (j !== null) { ev.preventDefault(); selectTab(tabs[j].dataset.tab, true); }
      });
    });
    document.querySelectorAll(".panels .panel").forEach(function (p) {
      p.addEventListener("animationend", function (ev) { if (ev.target === p) p.classList.remove("enter"); });
    });
    var tl = $("tablist");
    if (window.ResizeObserver) new ResizeObserver(placeTabInd).observe(tl);
    placeTabInd();
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(placeTabInd);
    requestAnimationFrame(function () { tl.classList.add("ready"); });
  }

  /* Cuenta animada corta; arranca cuando el panel es visible. */
  function whenVisible(panel, fn) {
    if (!panel.hidden) fn();
    else (panel._pending = panel._pending || []).push(fn);
  }

  function countUp(node, to, ms) {
    node.textContent = String(to);
    if (prefersReduced() || !(to > 0) || !window.requestAnimationFrame) return;
    var t0 = null;
    node.textContent = "0";
    function step(ts) {
      if (t0 === null) t0 = ts;
      var p = Math.min(1, (ts - t0) / ms);
      node.textContent = String(Math.round(to * (1 - Math.pow(1 - p, 3))));
      if (p < 1) requestAnimationFrame(step); else node.textContent = String(to);
    }
    requestAnimationFrame(step);
  }

  /* Render general */
  function placeholder(title, hint, ico) {
    return el("div", { cls: "empty" }, [
      el("span", { cls: "empty-ico" }, [icon(ico || "info")]),
      el("strong", { text: title }), el("span", { text: hint || "" })
    ]);
  }

  function renderAll() {
    var r = state.last && state.last.resp;
    renderOutput(r);
    renderDiag(r);
    renderTokens(r);
    renderTree("panel-ast", r && r.ast, false, "AST", "ast");
    renderTree("panel-typed", r && r.typed_ast, true, "AST con tipos", "typed");
    renderLayout(r);
    renderAsm(r);
    renderOpt(r);
    updateBadge(r);
    updateStatusbar();
  }

  function updateBadge(r) {
    var b = $("badge-diag");
    var n = r ? (r.errors || []).length : 0;
    var w = r ? (r.warnings || []).length : 0;
    b.hidden = n + w === 0;
    b.textContent = n ? String(n) : String(w);
    b.className = "badge" + (n ? "" : " warn");
    b.setAttribute("aria-label", n + " errores, " + w + " advertencias");
    var rb = $("rail-badge");
    rb.hidden = n + w === 0;
    rb.textContent = n ? String(n) : String(w);
    rb.className = "rail-badge" + (n ? "" : " warn");
    $("sb-errs").textContent = String(n);
    $("sb-warns").textContent = String(w);
    $("sb-diag").setAttribute("aria-label", "Ver diagnóstico: " + n + " errores, " + w + " advertencias");
    placeTabInd();
  }

  /* Salida */
  function renderBigText(pre, text) {
    var cut = L.truncateLines(text, OUT_LINES);
    pre.textContent = cut.text;
    var wrap = el("div");
    wrap.appendChild(pre);
    if (cut.truncated) {
      var bar = el("div", { cls: "more-bar" });
      bar.appendChild(el("span", { text: "Se muestran " + cut.shown + " de " + cut.total + " líneas." }));
      var btn = button("Mostrar todo", function () {
        btn.disabled = true;
        btn.textContent = "Cargando…";
        var rest = text.slice(cut.text.length);
        var pos = 0, step = 200000;
        (function chunk() {
          pre.appendChild(document.createTextNode(rest.slice(pos, pos + step)));
          pos += step;
          if (pos < rest.length) setTimeout(chunk, 0);
          else bar.remove();
        })();
      });
      bar.appendChild(btn);
      wrap.appendChild(bar);
    }
    return wrap;
  }

  function outputCard(title, text, cls, copyName, ico) {
    var head = el("div", { cls: "card-head" }, [el("h3", {}, [icon(ico || "terminal"), document.createTextNode(title)])]);
    var actions = el("div", { cls: "card-actions" });
    if (text) actions.appendChild(button("Copiar", function () { copyText(text, copyName); }, "", "copy"));
    head.appendChild(actions);
    var body = el("div", { cls: "card-body" });
    if (text) body.appendChild(renderBigText(el("pre", { cls: "out" + (cls ? " " + cls : "") }), text));
    else body.appendChild(el("pre", { cls: "out", text: "(vacío)" }));
    return el("section", { cls: "card" }, [head, body]);
  }

  function destroyEmptyMascot() {
    if (state.emptyM) {
      try { state.emptyM.destruir(); } catch (e) { /* ya destruida */ }
      state.emptyM = null;
    }
  }

  function hintCard(ico, title, text, onclick) {
    return el("button", { cls: "hint-card", attrs: { type: "button" }, on: { click: onclick } }, [
      el("span", { cls: "stat-ico" }, [icon(ico)]), el("strong", { text: title }), el("span", { cls: "h", text: text })
    ]);
  }

  function emptyOutput(p) {
    var useMascot = M && M.enabled() && window.Compi;
    var kids = [];
    var holder = null;
    if (useMascot) {
      holder = el("div", { cls: "empty-mascot", attrs: { "aria-hidden": "true" } });
      kids.push(el("div", { cls: "empty-mascot-wrap" }, [holder]));
    } else kids.push(el("span", { cls: "empty-ico" }, [icon("play")]));
    kids.push(el("strong", { text: "Todo listo para compilar" }));
    kids.push(el("span", {}, [
      document.createTextNode("Escriba o cargue un ejemplo y pulse "),
      el("kbd", { cls: "kbd", text: isMac() ? "⌘ ↵" : "Ctrl ↵" }),
      document.createTextNode(" para ver la salida, el diagnóstico por fase, el layout y el ensamblado.")
    ]));
    kids.push(el("div", { cls: "hint-cards" }, [
      hintCard("layers", "Probar un ejemplo", "Abre la galería con programas listos para compilar.", function () { if (window.MCUI) window.MCUI.openExamples(); }),
      hintCard("pen", "Escribir código", "Ve al editor y empieza; el diagnóstico te guía.", function () { state.editor.focus(); }),
      hintCard("shield", "Ver el pipeline", "Compila Hola mundo y recorre cada fase.", function () {
        applyExample("hola_mundo", true).then(function () { return execute("run"); }).then(function () { selectTab("diag"); });
      })
    ]));
    p.appendChild(el("div", { cls: "hero-empty" }, kids));
    if (holder) state.emptyM = M.create(holder, "reposo");
  }

  function isMac() { return /Mac|iPhone|iPad/.test(navigator.platform || ""); }

  function renderOutput(r) {
    var p = $("panel-output");
    destroyEmptyMascot();
    clear(p);
    if (!r) {
      emptyOutput(p);
      if (state.status && state.status.run_available === false) {
        p.appendChild(el("div", { cls: "banner" }, [icon("info"), el("span", { text: "La ejecución de programas no está disponible en este servidor (" + (state.status.run_unavailable_reason || "sin detalle") + "). Se mostrará solo el ensamblado." })]));
      }
      return;
    }
    var exit = L.describeExit(r);
    var opts = r.options || {};
    var chips = el("div", { cls: "chips" });
    chips.appendChild(chip(opts.optimize ? "-O1" : "-O0", "accent"));
    if (opts.optimize && opts.passes && opts.passes.length) chips.appendChild(chip(opts.passes.join(", ")));
    if (state.last.mode) chips.appendChild(chip("modo " + state.last.mode));
    if (exit) chips.appendChild(chip(exit.text, exit.kind === "ok" ? "ok" : exit.kind === "nonzero" ? "warn" : "fail"));
    if (r.duration_ms != null) chips.appendChild(chip("ejecución " + L.formatMs(r.duration_ms)));

    var ran = r.stdout != null || r.stderr != null || r.exit_code != null;
    var kind = "ok", title, ico = "check";
    if (!r.success) {
      kind = "fail"; ico = "alert";
      var n = (r.errors || []).length;
      title = "No terminó correctamente en la fase " + L.phaseLabel(r.phase) + " (" + n + " error" + (n === 1 ? "" : "es") + ")";
    } else if (exit && exit.kind !== "ok") {
      kind = "warn"; ico = "warn"; title = exit.text;
    } else if (ran) title = "Ejecución correcta";
    else { kind = "neutral"; ico = "info"; title = "Compilación correcta"; }

    var textBox = el("div", { cls: "hero-text" }, [el("strong", { text: title }), chips]);
    var hero = el("div", { cls: "hero " + kind, attrs: kind === "fail" ? { role: "alert" } : {} }, [el("span", { cls: "hero-ico" }, [icon(ico)]), textBox]);
    if (!r.success) textBox.appendChild(el("div", {}, [button("Ver diagnóstico", function () { selectTab("diag", true); }, "", "arrow")]));
    if (r.success && ran && r.exit_code != null) {
      var ek = r.exit_code === 0 ? "" : exit && exit.kind === "nonzero" ? " warn" : " fail";
      hero.appendChild(el("div", { cls: "exit-badge" + ek, attrs: { "aria-label": "Código de salida " + r.exit_code } }, [
        icon(r.exit_code === 0 ? "check" : "alert"), el("small", { text: "exit" }), document.createTextNode(String(r.exit_code))
      ]));
    }
    p.appendChild(hero);

    if (!ran) {
      if (r.success) {
        p.appendChild(el("div", { cls: "banner ok" }, [icon("check"), el("span", { text: "El modo seleccionado no ejecuta el programa; use el modo Completo o Ejecutar para ver la salida." })]));
      }
      return;
    }
    if (r.stdout_truncated || r.stderr_truncated) {
      p.appendChild(el("div", { cls: "banner" }, [icon("warn"), el("span", { text: "La salida superó el límite del servidor y fue truncada." })]));
    }
    var cards = el("div", { cls: "cards" });
    cards.appendChild(outputCard("stdout", r.stdout || "", "", "stdout"));
    if (r.stderr) cards.appendChild(outputCard("stderr", r.stderr, "err", "stderr", "alert"));
    p.appendChild(cards);
  }

  /* Diagnostico */
  var STATUS_TEXT = { ok: "Correcto", fail: "Fallo", skip: "Omitido" };
  var PHASE_META = {
    lex: { ico: "scan", desc: "Tokens del código fuente" },
    parse: { ico: "tree", desc: "Árbol sintáctico" },
    typecheck: { ico: "shield", desc: "Tipos, const y sobrecargas" },
    layout: { ico: "boxes", desc: "Tamaños, offsets y ABI" },
    gencode: { ico: "cpu", desc: "Generación de x86-64" },
    assemble: { ico: "package", desc: "Ensamblado y enlace" },
    run: { ico: "run", desc: "Ejecución del programa" }
  };

  function pipeline(st, r) {
    var ol = el("ol", { cls: "pipeline", attrs: { "aria-label": "Fases del compilador" } });
    L.PHASES.forEach(function (name, i) {
      var meta = PHASE_META[name] || { ico: "info", desc: "" };
      var tm = r && r.timings && typeof r.timings[name] === "number" ? r.timings[name] : (name === "run" && r && r.duration_ms != null && st[name] === "ok" ? r.duration_ms : null);
      var dur = tm != null ? L.formatMs(tm) : "";
      var on = el("span", { cls: "pic-on" }, [icon(meta.ico)]);
      var li = el("li", { cls: "pnode " + st[name], attrs: { "aria-label": L.phaseLabel(name) + ": " + STATUS_TEXT[st[name]] } }, [
        el("span", { cls: "pic" }, [icon(meta.ico), on]),
        el("span", { cls: "ptext" }, [el("span", { cls: "pname", text: L.phaseLabel(name) }), el("span", { cls: "pdesc", text: meta.desc }),
          dur ? el("span", { cls: "pdur", text: dur }) : null]),
        el("span", { cls: "pstate", text: STATUS_TEXT[st[name]] })
      ]);
      li.style.setProperty("--i", String(i));
      ol.appendChild(li);
    });
    return ol;
  }

  function padTo(s, n) { s = String(s); while (s.length < n) s = " " + s; return s; }
  function spaces(n) { return new Array(Math.max(0, n) + 1).join(" "); }

  /* Fragmento del codigo fuente con un marcador bajo la columna del diagnostico. */
  function snippetFor(d, codeLines) {
    if (!codeLines || !(d.line >= 1)) return null;
    var raw = codeLines[d.line - 1];
    if (typeof raw !== "string") return null;
    var text = raw.replace(/\t/g, "    ");
    var col = d.column >= 1 ? d.column : 1;
    var prefix = raw.slice(0, Math.max(0, col - 1));
    var vis = prefix.replace(/\t/g, "    ").length;
    var len = Math.min(40, Math.max(1, L.tokenEndColumn(raw, col) - col));
    var w = String(d.line).length;
    return el("span", { cls: "snippet" }, [
      el("span", { cls: "ln", text: padTo(d.line, w) + " | " }), document.createTextNode(text + "\n"),
      el("span", { cls: "ln", text: spaces(w) + " | " }), el("span", { cls: "caret", text: spaces(vis) + new Array(len + 1).join("^") })
    ]);
  }

  function diagList(items, kind, codeLines) {
    var ul = el("ul", { cls: "diag-list" });
    items.forEach(function (d) {
      d = d || {};
      var has = d.line >= 1;
      var pos = has ? "línea " + d.line + (d.column >= 1 ? ", col " + d.column : "") : "sin posición";
      var body = el("span", { cls: "diag-body" }, [
        el("span", { cls: "diag-top" }, [chip(L.phaseLabel(d.phase), kind === "error" ? "fail" : "warn")]),
        el("span", { cls: "msg", text: d.message || "" })
      ]);
      var snip = snippetFor(d, codeLines);
      if (snip) body.appendChild(snip);
      var tip = L.suggestFor(d);
      if (tip) body.appendChild(el("span", { cls: "hint-line" }, [icon("bulb"), el("span", { text: tip })]));
      var inner = [icon(kind === "error" ? "alert" : "warn"), body];
      if (has) inner.push(el("span", { cls: "pos" }, [document.createTextNode("Ln " + d.line + (d.column >= 1 ? ":" + d.column : "")), icon("arrow")]));
      var node = has
        ? el("button", { cls: "diag-item " + kind, attrs: { type: "button", "aria-label": (kind === "error" ? "Error" : "Advertencia") + ": " + (d.message || "") + ", " + pos + ". Ir a la línea." }, on: { click: function () { goTo(d.line, d.column, true); } } }, inner)
        : el("div", { cls: "diag-item " + kind }, inner);
      ul.appendChild(el("li", {}, [node]));
    });
    return ul;
  }

  function renderDiag(r) {
    var p = $("panel-diag");
    clear(p);
    if (!r) { p.appendChild(placeholder("Sin diagnóstico", "Aquí aparecerá el recorrido por cada fase del compilador y los errores con su línea.", "shield")); return; }
    var st = L.phaseStatuses(r, state.last.mode);
    var codeLines = state.last.req && typeof state.last.req.code === "string" ? state.last.req.code.split("\n") : null;
    p.appendChild(el("h3", { cls: "section-title", text: "Pipeline" }));
    var okCount = 0, failed = false;
    L.PHASES.forEach(function (n) { if (st[n] === "ok") okCount++; if (st[n] === "fail") failed = true; });
    var fill = el("i");
    fill.style.width = Math.round(((okCount + (failed ? 0.5 : 0)) / L.PHASES.length) * 100) + "%";
    p.appendChild(el("div", { cls: "pbar" + (failed ? " fail" : ""), attrs: { "aria-hidden": "true" } }, [fill]));
    p.appendChild(pipeline(st, r));
    var errs = r.errors || [], warns = r.warnings || [];
    p.appendChild(el("h3", { cls: "section-title", text: "Errores (" + errs.length + ")" }));
    p.appendChild(errs.length ? diagList(errs, "error", codeLines)
      : el("div", { cls: "ok-note" }, [icon("check"), document.createTextNode("Sin errores.")]));
    p.appendChild(el("h3", { cls: "section-title", text: "Advertencias (" + warns.length + ")" }));
    p.appendChild(warns.length ? diagList(warns, "warning", codeLines) : el("p", { cls: "muted", text: "Sin advertencias." }));
    var report = L.errorReport(r, state.last.mode);
    var tools = el("div", { cls: "tools" });
    tools.style.marginTop = "18px";
    tools.appendChild(button("Copiar reporte", function () { copyText(report, "Reporte"); }, "", "copy"));
    tools.appendChild(button("Descargar reporte", function () { download("reporte.txt", report); }, "", "download"));
    p.appendChild(tools);
  }

  /* Tokens */
  function renderTokens(r) {
    var p = $("panel-tokens");
    clear(p);
    var tokens = r && r.tokens;
    if (!Array.isArray(tokens)) {
      p.appendChild(placeholder("Sin tokens", "Compile con el modo Completo, Ensamblado o Tokens para ver el análisis léxico.", "scan"));
      return;
    }
    var filter = el("input", { attrs: { type: "search", "aria-label": "Filtrar tokens", placeholder: "Filtrar por tipo, lexema o línea" } });
    filter.value = state.tokenFilter;
    var count = el("span", { cls: "count" });
    var tools = el("div", { cls: "tools" }, [filter, count]);
    var tbody = el("tbody");
    var more = el("div", { cls: "tools" });
    var table = el("table", { cls: "grid" }, [
      el("caption", { text: "Tokens producidos por el analizador léxico", cls: "sr-only" }),
      el("thead", {}, [el("tr", {}, [
        el("th", { text: "#", cls: "num", attrs: { scope: "col" } }), el("th", { text: "Tipo", attrs: { scope: "col" } }),
        el("th", { text: "Lexema", attrs: { scope: "col" } }), el("th", { text: "Línea", cls: "num", attrs: { scope: "col" } }),
        el("th", { text: "Columna", cls: "num", attrs: { scope: "col" } })])]),
      tbody
    ]);
    function draw(limit) {
      state.tokenFilter = filter.value;
      var list = L.filterTokens(tokens, filter.value);
      clear(tbody);
      clear(more);
      list.slice(0, limit).forEach(function (t, i) {
        var tr = el("tr", { cls: "clickable", on: { click: function () { goTo(t.line, t.column, true); } } }, [
          el("td", { cls: "num mono", text: String(i + 1) }), el("td", { cls: "mono", text: String(t.type) }),
          el("td", { cls: "mono", text: JSON.stringify(String(t.lexeme == null ? "" : t.lexeme)) }),
          el("td", { cls: "num", text: String(t.line) }), el("td", { cls: "num", text: String(t.column) })]);
        tbody.appendChild(tr);
      });
      count.textContent = list.length + " de " + tokens.length + " tokens";
      if (list.length > limit) {
        more.appendChild(button("Mostrar " + Math.min(TOKEN_ROWS, list.length - limit) + " más", function () { draw(limit + TOKEN_ROWS); }));
        more.appendChild(el("span", { cls: "count", text: "Mostrando " + limit + " de " + list.length }));
      }
    }
    filter.addEventListener("input", function () { draw(TOKEN_ROWS); });
    draw(TOKEN_ROWS);
    p.appendChild(tools);
    p.appendChild(el("div", { cls: "table-wrap" }, [table]));
    p.appendChild(more);
  }

  /* Arbol del AST */
  function renderTree(panelId, root, typed, title, key) {
    var p = $(panelId);
    clear(p);
    if (!root) {
      p.appendChild(placeholder("Sin " + title, key === "typed"
        ? "El AST con tipos se genera desde el modo Verificar en adelante, si no hubo errores previos."
        : "Compile con el modo Completo, Ensamblado, Verificar o AST.", "tree"));
      return;
    }
    var total = L.countNodes(root);
    var ctl = createTree(root, typed);
    state.trees[key] = ctl;
    var graph = null, allMode = "default";
    var view = state.store.astView === "graph" ? "graph" : "list";
    var search = el("input", { attrs: { type: "search", "aria-label": "Buscar nodos en " + title, placeholder: "Buscar nodo, etiqueta, tipo…" } });
    var count = el("span", { cls: "count", text: total + " nodos" });
    var seg = el("div", { cls: "seg seg-ast", attrs: { role: "radiogroup", "aria-label": "Vista del árbol" } }, [el("span", { cls: "seg-ind", attrs: { "aria-hidden": "true" } })]);
    ["list", "graph"].forEach(function (v) {
      var inp = el("input", { attrs: { type: "radio", name: "astview-" + key, value: v } });
      if (v === view) inp.checked = true;
      seg.appendChild(el("label", { cls: "seg-opt" }, [inp, el("span", { text: v === "list" ? "Lista" : "Gráfico" })]));
    });
    var tools = el("div", { cls: "tools" }, [
      seg, search, button("Expandir todo", function () { allMode = "expanded"; ctl.expandAll(); if (graph) graph.expandAll(); }),
      button("Contraer todo", function () { allMode = "collapsed"; ctl.collapseAll(); if (graph) graph.collapseAll(); }), count
    ]);
    var svgTools = el("span", { cls: "ag-export" }, [
      button("Descargar SVG", function () { if (graph) graph.downloadSvg(); }, "", "download"),
      button("Descargar PNG", function () { if (graph) graph.downloadPng(); }, "", "download")
    ]);
    if (key === "ast" || key === "typed") {
      tools.appendChild(button("Copiar JSON", function () { copyText(JSON.stringify(root, null, 2), "JSON"); }, "", "copy"));
      tools.appendChild(button("Descargar JSON", function () { download(key + ".json", JSON.stringify(root, null, 2), "application/json"); }, "", "download"));
    }
    tools.appendChild(svgTools);
    var graphHost = el("div", { cls: "ag-host" });
    function showView(v) {
      ctl.element.hidden = v === "graph";
      graphHost.hidden = v !== "graph";
      svgTools.hidden = v !== "graph";
      if (v === "graph") {
        if (!graph) {
          graph = createGraph(root, typed, allMode);
          graphHost.appendChild(graph.element);
          if (search.value.trim()) graph.search(search.value);
        }
        graph.fit();
      }
    }
    seg.addEventListener("change", function (ev) {
      var v = ev.target.value;
      if (v !== "list" && v !== "graph") return;
      saveStore({ astView: v }, true);
      placeSeg(seg);
      showView(v);
    });
    search.addEventListener("input", function () {
      var n = ctl.search(search.value);
      if (graph) graph.search(search.value);
      count.textContent = search.value.trim() ? n + " coincidencias de " + total + " nodos" : total + " nodos";
    });
    search.addEventListener("keydown", function (ev) {
      if (ev.key === "Enter" && graph && !graphHost.hidden) { ev.preventDefault(); graph.next(); }
    });
    p.appendChild(tools);
    p.appendChild(ctl.element);
    p.appendChild(graphHost);
    placeSeg(seg);
    requestAnimationFrame(function () { seg.classList.add("ready"); });
    showView(view);
  }

  function createGraph(root, typed, initial) {
    var collapsed = initial === "expanded" ? {} : L.initialCollapsed(root, initial === "collapsed" ? 2 : (L.countNodes(root) <= 60 ? 99 : 4));
    var view = { k: 1, tx: 24, ty: 24 };
    var lay = null, selected = null, focusId = "0", hits = {}, hitList = [], hitIdx = -1, touched = false, prevIds = null;
    var host = el("div", { cls: "agraph" });
    var svg = svgEl("svg", { role: "tree", "aria-label": typed ? "Grafico del AST con tipos" : "Grafico del AST" });
    var world = svgEl("g", { "class": "ag-world" });
    var gEdges = svgEl("g", {}), gNodes = svgEl("g", {});
    world.appendChild(gEdges);
    world.appendChild(gNodes);
    svg.appendChild(world);
    var viewEl = el("div", { cls: "ag-view" }, [svg]);
    var info = el("div", { cls: "ag-info", attrs: { hidden: "" } });
    var mini = svgEl("svg", { "class": "ag-mini", "aria-hidden": "true", viewBox: "0 0 160 100" });
    var miniDots = svgEl("g", {}), miniVp = svgEl("rect", { "class": "ag-vp", rx: 2 });
    mini.appendChild(miniDots);
    mini.appendChild(miniVp);
    var counter = el("span", { cls: "ag-count", attrs: { "aria-live": "polite" } });

    function size() { return { w: viewEl.clientWidth, h: viewEl.clientHeight }; }

    function applyView(animate) {
      world.classList.toggle("anim", !!animate && !prefersReduced());
      world.style.transform = "translate(" + view.tx.toFixed(1) + "px," + view.ty.toFixed(1) + "px) scale(" + view.k.toFixed(4) + ")";
      updateMini();
    }

    function updateMini() {
      var sz = size();
      if (!lay || !sz.w) { miniVp.setAttribute("width", "0"); return; }
      var sc = Math.min(150 / Math.max(lay.width, 1), 90 / Math.max(lay.height, 1));
      var vx = -view.tx / view.k, vy = -view.ty / view.k;
      miniVp.setAttribute("x", (5 + vx * sc).toFixed(1));
      miniVp.setAttribute("y", (5 + vy * sc).toFixed(1));
      miniVp.setAttribute("width", Math.max(4, sz.w / view.k * sc).toFixed(1));
      miniVp.setAttribute("height", Math.max(4, sz.h / view.k * sc).toFixed(1));
    }

    function fit(animate) {
      var sz = size();
      if (!lay || !sz.w || !sz.h) return;
      var pad = 40;
      var k = Math.min((sz.w - pad) / Math.max(lay.width, 1), (sz.h - pad) / Math.max(lay.height, 1), 1.1);
      /* Con arboles grandes se prefiere legibilidad: no bajar de 0.65 y centrar en la raiz. */
      view.k = Math.max(0.65, k);
      view.tx = Math.max(20, (sz.w - lay.width * view.k) / 2);
      var root0 = lay.byId["0"];
      var cy = root0.y + root0.h / 2;
      view.ty = lay.height * view.k + 32 <= sz.h ? (sz.h - lay.height * view.k) / 2 : sz.h / 2 - cy * view.k;
      touched = false;
      applyView(animate);
    }

    function zoomAt(f, cx, cy, animate) {
      var sz = size();
      if (cx == null) { cx = sz.w / 2; cy = sz.h / 2; }
      var nk = Math.min(2.5, Math.max(0.12, view.k * f));
      var r = nk / view.k;
      view.tx = cx - (cx - view.tx) * r;
      view.ty = cy - (cy - view.ty) * r;
      view.k = nk;
      touched = true;
      applyView(animate);
    }

    function centerOn(id, animate) {
      var n = lay && lay.byId[id], sz = size();
      if (!n || !sz.w) return;
      view.tx = sz.w / 2 - (n.x + n.w / 2) * view.k;
      view.ty = sz.h / 2 - (n.y + n.h / 2) * view.k;
      touched = true;
      applyView(animate);
    }

    function ensureVisible(id) {
      var n = lay && lay.byId[id], sz = size();
      if (!n || !sz.w) return;
      var l = n.x * view.k + view.tx, t = n.y * view.k + view.ty, r = l + n.w * view.k, b = t + n.h * view.k;
      if (l < 8 || t < 8 || r > sz.w - 8 || b > sz.h - 8) centerOn(id, true);
    }

    function nodeEl(n) {
      var cls = "ag-node cat-" + n.cat + (selected === n.id ? " sel" : "") + (hits[n.id] ? " hit" : "") + (prevIds && !prevIds[n.id] ? " new" : "");
      var attrs = { "class": cls, role: "treeitem", "data-id": n.id, "aria-level": String(n.depth + 1),
        tabindex: focusId === n.id ? "0" : "-1", transform: "translate(" + Math.round(n.x) + " " + Math.round(n.y) + ")",
        "aria-label": n.kind + (n.label ? " " + n.label : "") + (n.line != null ? ", línea " + n.line : "") };
      if (n.childCount) attrs["aria-expanded"] = n.collapsed ? "false" : "true";
      var g = svgEl("g", attrs);
      var tip = svgEl("title", {});
      tip.textContent = n.kind + (n.label ? ": " + n.label : "") + (n.type ? " : " + n.type : "") + (n.line != null ? "  (" + n.line + (n.col != null ? ":" + n.col : "") + ")" : "");
      g.appendChild(tip);
      g.appendChild(svgEl("rect", { "class": "card", width: n.w, height: n.h, rx: 11 }));
      g.appendChild(svgEl("rect", { "class": "strip", x: 0, y: 10, width: 4, height: n.h - 20, rx: 2 }));
      var t1 = svgEl("text", { "class": "ag-kind", x: 14, y: 20 });
      t1.textContent = n.kind;
      g.appendChild(t1);
      if (n.shown) {
        var t2 = svgEl("text", { "class": "ag-label", x: 14, y: 38 });
        t2.textContent = n.shown;
        g.appendChild(t2);
      }
      if (n.chips) {
        var cx = 14;
        if (n.type) {
          var tt = n.type.length > 22 ? n.type.slice(0, 21) + "…" : n.type;
          var cw = tt.length * 6.6 + 14;
          g.appendChild(svgEl("rect", { "class": "ag-chip", x: cx, y: 44, width: cw, height: 16, rx: 8 }));
          var tc = svgEl("text", { "class": "ag-chip-t", x: cx + 7, y: 56 });
          tc.textContent = tt;
          g.appendChild(tc);
          cx += cw + 6;
        }
        if (n.isConst) {
          g.appendChild(svgEl("rect", { "class": "ag-chip const", x: cx, y: 44, width: 40, height: 16, rx: 8 }));
          var tk = svgEl("text", { "class": "ag-chip-t const", x: cx + 7, y: 56 });
          tk.textContent = "const";
          g.appendChild(tk);
        }
      }
      if (n.collapsed) {
        g.appendChild(svgEl("circle", { "class": "ag-badge", cx: n.w, cy: n.h / 2, r: 12 }));
        var tb = svgEl("text", { "class": "ag-badge-t", x: n.w, y: n.h / 2 + 4 });
        tb.textContent = "+" + (n.hidden > 99 ? "99" : n.hidden);
        g.appendChild(tb);
      }
      return g;
    }

    function render(animateNew) {
      lay = L.layoutTree(root, { collapsed: collapsed, typed: typed });
      clear(gEdges); clear(gNodes); clear(miniDots);
      lay.edges.forEach(function (e) {
        var a = lay.byId[e.from], b = lay.byId[e.to];
        var x1 = a.x + a.w, y1 = a.y + a.h / 2, x2 = b.x, y2 = b.y + b.h / 2, dx = (x2 - x1) * 0.55;
        gEdges.appendChild(svgEl("path", { "class": "ag-edge" + (selected && (e.to === selected || e.from === selected) ? " hot" : ""), d: "M" + x1 + " " + y1 + " C" + (x1 + dx) + " " + y1 + " " + (x2 - dx) + " " + y2 + " " + x2 + " " + y2 }));
      });
      var frag = document.createDocumentFragment();
      lay.nodes.forEach(function (n) { frag.appendChild(nodeEl(n)); });
      gNodes.appendChild(frag);
      var sc = Math.min(150 / Math.max(lay.width, 1), 90 / Math.max(lay.height, 1));
      lay.nodes.forEach(function (n) {
        miniDots.appendChild(svgEl("rect", { x: (5 + n.x * sc).toFixed(1), y: (5 + n.y * sc).toFixed(1), width: Math.max(1.5, n.w * sc).toFixed(1), height: Math.max(1.2, n.h * sc).toFixed(1) }));
      });
      counter.textContent = lay.visible + " de " + lay.total + " nodos visibles";
      prevIds = {};
      lay.nodes.forEach(function (n) { prevIds[n.id] = true; });
      updateMini();
    }

    function showInfo(n) {
      clear(info);
      if (!n) { info.hidden = true; return; }
      info.hidden = false;
      info.appendChild(el("strong", { text: n.kind }));
      if (n.label) info.appendChild(el("span", { cls: "mono", text: n.label }));
      var rows = [];
      if (n.line != null) rows.push(["Posición", n.line + (n.col != null ? ":" + n.col : "")]);
      if (n.type) rows.push(["Tipo", n.type]);
      if (n.isConst) rows.push(["const", "sí"]);
      Object.keys(n.attrs || {}).forEach(function (k) {
        var v = n.attrs[k];
        if (v !== null && typeof v !== "object" && (typed || k === "offset" || k === "size")) rows.push([k, String(v)]);
      });
      var dl = el("dl");
      rows.slice(0, 10).forEach(function (r) { dl.appendChild(el("div", {}, [el("dt", { text: r[0] }), el("dd", { text: r[1] })])); });
      info.appendChild(dl);
    }

    function toggle(id) {
      var n = lay.byId[id];
      if (!n || !n.childCount) return;
      var old = { x: n.x * view.k + view.tx, y: n.y * view.k + view.ty };
      if (collapsed[id]) delete collapsed[id]; else collapsed[id] = true;
      focusId = id;
      render(true);
      var nn = lay.byId[id];
      view.tx = old.x - nn.x * view.k;
      view.ty = old.y - nn.y * view.k;
      applyView(false);
      refocus();
    }

    function refocus() {
      var node = gNodes.querySelector('[data-id="' + focusId + '"]');
      if (node && node.focus) node.focus({ preventScroll: true });
    }

    function select(id, goto) {
      var n = lay.byId[id];
      if (!n) return;
      selected = id;
      focusId = id;
      showInfo(n);
      if (goto && n.line != null) goTo(n.line, n.col, false);
    }

    function activate(id) {
      select(id, true);
      var n = lay.byId[id];
      if (n && n.childCount) toggle(id); else { render(false); refocus(); }
    }

    /* Raton y tactil: arrastrar para mover, pellizcar para ampliar, clic corto para activar. */
    var ptrs = {}, down = null, pinch = null;
    svg.addEventListener("pointerdown", function (ev) {
      ptrs[ev.pointerId] = { x: ev.clientX, y: ev.clientY };
      var ids = Object.keys(ptrs);
      if (ids.length === 2) {
        var a = ptrs[ids[0]], b = ptrs[ids[1]];
        pinch = { d: Math.hypot(a.x - b.x, a.y - b.y), k: view.k };
        down = null;
      } else {
        var t = ev.target.closest ? ev.target.closest(".ag-node") : null;
        down = { x: ev.clientX, y: ev.clientY, tx: view.tx, ty: view.ty, node: t, moved: false };
      }
    });
    svg.addEventListener("pointermove", function (ev) {
      if (!ptrs[ev.pointerId]) return;
      ptrs[ev.pointerId] = { x: ev.clientX, y: ev.clientY };
      var ids = Object.keys(ptrs);
      if (ids.length === 2 && pinch) {
        var a = ptrs[ids[0]], b = ptrs[ids[1]], r = viewEl.getBoundingClientRect();
        var d = Math.hypot(a.x - b.x, a.y - b.y);
        zoomAt((pinch.k * d / pinch.d) / view.k, (a.x + b.x) / 2 - r.left, (a.y + b.y) / 2 - r.top, false);
      } else if (down) {
        var dx = ev.clientX - down.x, dy = ev.clientY - down.y;
        if (!down.moved && Math.hypot(dx, dy) > 5) { down.moved = true; viewEl.classList.add("panning"); try { svg.setPointerCapture(ev.pointerId); } catch (e) { /* sin captura */ } }
        if (down.moved) { view.tx = down.tx + dx; view.ty = down.ty + dy; touched = true; applyView(false); }
      }
    });
    function up(ev) {
      var d = down;
      delete ptrs[ev.pointerId];
      if (Object.keys(ptrs).length < 2) pinch = null;
      viewEl.classList.remove("panning");
      if (d && !d.moved && d.node && ev.type === "pointerup") activate(d.node.getAttribute("data-id"));
      down = null;
    }
    svg.addEventListener("pointerup", up);
    svg.addEventListener("pointercancel", up);
    svg.addEventListener("wheel", function (ev) {
      ev.preventDefault();
      var r = viewEl.getBoundingClientRect();
      var f = Math.exp(-ev.deltaY * (ev.ctrlKey ? 0.012 : 0.0016));
      zoomAt(f, ev.clientX - r.left, ev.clientY - r.top, false);
    }, { passive: false });
    mini.addEventListener("pointerdown", function (ev) {
      var r = mini.getBoundingClientRect(), sc = Math.min(150 / Math.max(lay.width, 1), 90 / Math.max(lay.height, 1)), sz = size();
      var wx = ((ev.clientX - r.left) / r.width * 160 - 5) / sc, wy = ((ev.clientY - r.top) / r.height * 100 - 5) / sc;
      view.tx = sz.w / 2 - wx * view.k;
      view.ty = sz.h / 2 - wy * view.k;
      touched = true;
      applyView(true);
    });

    /* Teclado: flechas entre padre, hijos y hermanos; Enter alterna. */
    svg.addEventListener("keydown", function (ev) {
      var t = ev.target.closest ? ev.target.closest(".ag-node") : null;
      if (!t) return;
      var id = t.getAttribute("data-id"), n = lay.byId[id], to = null;
      var kids = lay.nodes.filter(function (m) { return m.parent === id; });
      var sibs = lay.nodes.filter(function (m) { return m.parent === n.parent; });
      var col = lay.nodes.filter(function (m) { return m.depth === n.depth; });
      if (ev.key === "ArrowRight") { if (n.collapsed) { ev.preventDefault(); toggle(id); return; } if (kids.length) to = kids[0].id; }
      else if (ev.key === "ArrowLeft") { if (n.parent) to = n.parent; }
      else if (ev.key === "ArrowDown" || ev.key === "ArrowUp") {
        var list = n.parent ? sibs : col, i = list.indexOf(n) + (ev.key === "ArrowDown" ? 1 : -1);
        if (i < 0 || i >= list.length) list = col, i = list.indexOf(n) + (ev.key === "ArrowDown" ? 1 : -1);
        if (list[i]) to = list[i].id;
      }
      else if (ev.key === "Home") to = "0";
      else if (ev.key === "Enter" || ev.key === " ") { ev.preventDefault(); activate(id); return; }
      else return;
      ev.preventDefault();
      if (to) {
        focusId = to;
        Array.prototype.forEach.call(gNodes.children, function (g) { g.setAttribute("tabindex", g.getAttribute("data-id") === to ? "0" : "-1"); });
        refocus();
        select(to, false);
        Array.prototype.forEach.call(gNodes.children, function (g) { g.classList.toggle("sel", g.getAttribute("data-id") === to); });
        ensureVisible(to);
      }
    });
    svg.addEventListener("focusin", function (ev) {
      var t = ev.target.closest ? ev.target.closest(".ag-node") : null;
      if (t) { focusId = t.getAttribute("data-id"); }
    });

    function btn(label, aria, fn) {
      return el("button", { cls: "ag-btn", text: label, attrs: { type: "button", "aria-label": aria, title: aria }, on: { click: fn } });
    }
    var bar = el("div", { cls: "ag-bar", attrs: { role: "toolbar", "aria-label": "Controles del gráfico" } }, [
      btn("−", "Reducir zoom", function () { zoomAt(1 / 1.25, null, null, true); }),
      btn("+", "Aumentar zoom", function () { zoomAt(1.25, null, null, true); }),
      btn("Ajustar", "Ajustar a la pantalla", function () { fit(true); }),
      btn("1:1", "Restablecer zoom y posición", function () {
        view.k = 1; view.tx = 24; view.ty = Math.max(16, size().h / 2 - (lay.byId["0"].y + lay.byId["0"].h / 2));
        touched = true; applyView(true);
      })
    ]);
    var hint = el("div", { cls: "ag-hint", text: "Rueda: zoom · Arrastrar: mover · Clic: contraer o expandir · Flechas: navegar" });
    host.appendChild(viewEl);
    host.appendChild(bar);
    host.appendChild(counter);
    host.appendChild(info);
    host.appendChild(mini);
    host.appendChild(hint);

    if (window.ResizeObserver) new ResizeObserver(function () { if (!touched) fit(false); else updateMini(); }).observe(viewEl);

    function exportSvg() {
      var cs = getComputedStyle(document.documentElement);
      function v(name, fb) { return (cs.getPropertyValue(name) || fb).trim() || fb; }
      var cat = { decl: v("--cm-def", "#2a49c9"), stmt: v("--cm-type", "#0a6d8f"), expr: v("--cm-keyword", "#7c3aed"), literal: v("--cm-number", "#15803d"), ident: v("--cm-meta", "#92580a") };
      var css = ".card{fill:" + v("--surface", "#fff") + ";stroke:" + v("--line-strong", "#c3cbe6") + ";stroke-width:1.2}" +
        ".ag-kind{font:700 12px system-ui,sans-serif;fill:" + v("--ink", "#121832") + "}.ag-label{font:500 11.5px ui-monospace,Menlo,monospace;fill:" + v("--ink-2", "#444e72") + "}" +
        ".ag-chip{fill:" + v("--info-soft", "#e0f1fb") + "}.ag-chip-t{font:600 10px ui-monospace,Menlo,monospace;fill:" + v("--info", "#0b6fa8") + "}" +
        ".ag-badge{fill:" + v("--accent-fill", "#3b5bdb") + "}.ag-badge-t{font:700 10px system-ui,sans-serif;fill:#fff;text-anchor:middle}" +
        ".ag-edge{fill:none;stroke:" + v("--line-strong", "#c3cbe6") + ";stroke-width:1.5}";
      Object.keys(cat).forEach(function (k) { css += ".cat-" + k + " .strip{fill:" + cat[k] + "}"; });
      var pad = 20, W = Math.ceil(lay.width + pad * 2), H = Math.ceil(lay.height + pad * 2);
      var out = svgEl("svg", { viewBox: "0 0 " + W + " " + H, width: W, height: H });
      var st = svgEl("style", {});
      st.textContent = css;
      out.appendChild(st);
      out.appendChild(svgEl("rect", { width: W, height: H, fill: v("--surface-2", "#f6f8fe") }));
      var g = svgEl("g", { transform: "translate(" + pad + " " + pad + ")" });
      g.appendChild(gEdges.cloneNode(true));
      var nodesCopy = gNodes.cloneNode(true);
      Array.prototype.forEach.call(nodesCopy.querySelectorAll("title"), function (t) { t.parentNode.removeChild(t); });
      g.appendChild(nodesCopy);
      out.appendChild(g);
      return { text: '<?xml version="1.0" encoding="UTF-8"?>\n' + new XMLSerializer().serializeToString(out), w: W, h: H };
    }

    function exportPng(name) {
      var e = exportSvg(), img = new Image();
      img.onload = function () {
        var c = document.createElement("canvas"), sc = 2;
        c.width = e.w * sc; c.height = e.h * sc;
        var ctx = c.getContext("2d");
        ctx.scale(sc, sc);
        ctx.drawImage(img, 0, 0);
        c.toBlob(function (blob) {
          if (!blob) { toast("No se pudo generar el PNG", "err"); return; }
          var url = URL.createObjectURL(blob), a = el("a", { attrs: { href: url, download: name } });
          document.body.appendChild(a); a.click(); document.body.removeChild(a);
          setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
          toast("Descargando " + name);
        }, "image/png");
      };
      img.onerror = function () { toast("No se pudo generar el PNG", "err"); };
      img.src = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(e.text);
    }

    render(false);
    applyView(false);

    return {
      element: host,
      fit: function () { fit(false); },
      expandAll: function () { collapsed = {}; render(false); fit(false); },
      collapseAll: function () { collapsed = L.initialCollapsed(root, 2); render(false); fit(false); },
      search: function (q) {
        var res = L.searchTree(root, q);
        hits = res.matches;
        hitList = Object.keys(hits).sort();
        hitIdx = -1;
        if (String(q || "").trim()) Object.keys(res.expand).forEach(function (p) { delete collapsed[p]; });
        render(false);
        if (hitList.length) { hitIdx = 0; focusId = hitList[0]; centerOn(hitList[0], true); }
        return res.count;
      },
      next: function () {
        if (!hitList.length) return;
        hitIdx = (hitIdx + 1) % hitList.length;
        L.revealPath(collapsed, hitList[hitIdx]);
        render(false);
        centerOn(hitList[hitIdx], true);
      },
      downloadSvg: function () { download("ast-grafico.svg", exportSvg().text, "image/svg+xml"); },
      downloadPng: function () { exportPng("ast-grafico.png"); }
    };
  }

  function createTree(root, typed) {
    var expanded = {}, matches = {}, selected = null;
    var container = el("div");
    var ul;

    function initExpansion(raw, path, depth, max) {
      var n = L.normalizeNode(raw);
      if (!n || !n.children.length) return;
      if (depth < max) expanded[path] = true;
      n.children.forEach(function (c, i) { initExpansion(c, path + "." + i, depth + 1, max); });
    }

    function nodeItem(raw, path) {
      var n = L.normalizeNode(raw);
      if (!n) return null;
      var li = el("li", { attrs: { role: "treeitem" } });
      if (n.children.length) li.setAttribute("aria-expanded", "false");
      var row = el("div", { cls: "node-row" + (matches[path] ? " match" : "") + (selected === path ? " selected" : "") });
      var toggle = el("button", { cls: "node-toggle" + (n.children.length ? "" : " leaf"), attrs: { type: "button", "aria-label": "Expandir o contraer " + n.kind, tabindex: n.children.length ? "0" : "-1" } });
      var main = el("button", { cls: "node-main", attrs: { type: "button" } });
      main.appendChild(el("span", { cls: "node-kind", text: n.kind }));
      if (n.label) main.appendChild(el("span", { cls: "node-label", text: n.label }));
      if (n.type) main.appendChild(el("span", { cls: "node-type", text: n.type }));
      if (n.isConst) main.appendChild(el("span", { cls: "node-const", text: "const" }));
      L.attrChips(n.attrs, typed).forEach(function (c) {
        var k = c.split(" ")[0];
        main.appendChild(el("span", { cls: "node-attr" + (k === "offset" || k === "size" ? " " + k : ""), text: c }));
      });
      if (n.line != null) main.appendChild(el("span", { cls: "node-pos", text: n.line + (n.col != null ? ":" + n.col : "") }));
      row.appendChild(toggle);
      row.appendChild(main);
      li.appendChild(row);
      var sub = null;
      /* Con animate, el subarbol entra colapsado y se expande con grid-template-rows. */
      function open(animate) {
        if (sub || !n.children.length) return;
        var group = el("ul", { attrs: { role: "group" } });
        n.children.forEach(function (c, i) {
          var ch = nodeItem(c, path + "." + i);
          if (ch) group.appendChild(ch);
        });
        var wrap = el("div", { cls: "sub" + (animate && !prefersReduced() ? " collapsed" : ""), attrs: { role: "none" } }, [group]);
        sub = wrap;
        li.appendChild(wrap);
        li.setAttribute("aria-expanded", "true");
        if (wrap.classList.contains("collapsed")) {
          requestAnimationFrame(function () { requestAnimationFrame(function () { wrap.classList.remove("collapsed"); }); });
        }
      }
      function close() {
        if (!sub) return;
        var wrap = sub;
        sub = null;
        li.setAttribute("aria-expanded", "false");
        var done = function () { if (wrap.parentNode) wrap.parentNode.removeChild(wrap); };
        if (prefersReduced()) { done(); return; }
        wrap.classList.add("collapsed");
        wrap.addEventListener("transitionend", function (ev) { if (ev.target === wrap && ev.propertyName === "grid-template-rows") done(); });
        setTimeout(done, 280);
      }
      function flip() {
        if (sub) { delete expanded[path]; close(); } else { expanded[path] = true; open(true); }
      }
      if (n.children.length) {
        toggle.appendChild(icon("chevron"));
        toggle.addEventListener("click", flip);
        if (expanded[path]) open(false);
      }
      main.addEventListener("click", function () {
        var prev = container.querySelector(".node-row.selected");
        if (prev) prev.classList.remove("selected");
        row.classList.add("selected");
        selected = path;
        if (n.line != null) goTo(n.line, n.col, false);
      });
      main.addEventListener("dblclick", function () { if (n.children.length) flip(); });
      return li;
    }

    function render() {
      clear(container);
      ul = el("ul", { cls: "tree", attrs: { role: "tree", "aria-label": "Árbol sintáctico" } });
      var top = nodeItem(root, "0");
      if (top) ul.appendChild(top);
      container.appendChild(ul);
    }

    function setAll(on) {
      expanded = {};
      if (on) {
        var total = L.countNodes(root);
        initExpansion(root, "0", 0, total > 5000 ? 8 : 100000);
        if (total > 5000) toast("Árbol grande: se expandieron los primeros 8 niveles.");
      } else expanded["0"] = true;
      render();
    }

    initExpansion(root, "0", 0, 3);
    render();
    return {
      element: container,
      expandAll: function () { setAll(true); },
      collapseAll: function () { setAll(false); },
      search: function (q) {
        var res = L.searchTree(root, q);
        matches = res.matches;
        if (q.trim()) {
          expanded = {};
          Object.keys(res.expand).forEach(function (k) { expanded[k] = true; });
        }
        render();
        var first = container.querySelector(".node-row.match");
        if (first && first.scrollIntoView) first.scrollIntoView({ block: "nearest" });
        return res.count;
      }
    };
  }

  /* Layout y ABI */
  function kvItem(dl, k, v) {
    dl.appendChild(el("div", {}, [el("dt", { text: k }), el("dd", { text: v == null ? "-" : String(v) })]));
  }

  function simpleTable(caption, heads, rows, numCols) {
    var thead = el("tr");
    heads.forEach(function (h, i) { thead.appendChild(el("th", { text: h, cls: numCols && numCols.indexOf(i) >= 0 ? "num" : "", attrs: { scope: "col" } })); });
    var tbody = el("tbody");
    rows.forEach(function (r) {
      var tr = el("tr");
      r.forEach(function (c, i) {
        var td = el("td", { cls: (numCols && numCols.indexOf(i) >= 0 ? "num " : "") + "mono" });
        if (c && c.nodeType) td.appendChild(c); else td.textContent = c == null ? "-" : String(c);
        tr.appendChild(td);
      });
      tbody.appendChild(tr);
    });
    return el("div", { cls: "table-wrap" }, [el("table", { cls: "grid" }, [caption ? el("caption", { text: caption, cls: "sr-only" }) : null, el("thead", {}, [thead]), tbody])]);
  }

  function pct(v, total) { return (Math.max(0, v) / total * 100).toFixed(3) + "%"; }

  /* Diagrama de bytes proporcional: campos coloreados y padding rayado. */
  function byteDiagram(rec, fields) {
    var size = rec.size;
    var wrap = el("div", { cls: "bytes" });
    var used = 0;
    fields.forEach(function (f) { used += f.size > 0 ? f.size : 0; });
    var padBytes = Math.max(0, size - used);
    wrap.appendChild(el("div", { cls: "bytes-cap" }, [
      el("span", { text: size + " bytes, alineación " + rec.align }),
      el("span", { text: padBytes ? padBytes + " B de padding" : "sin padding" })
    ]));
    var bar = el("div", { cls: "bytebar", attrs: { role: "img", "aria-label": "Distribución de campos en memoria de " + rec.name + ": " + fields.map(function (f) { return f.name + " en offset " + f.offset + " de " + f.size + " bytes"; }).join("; ") } });
    var legend = el("ul", { cls: "legend" });
    var readout = el("div", { cls: "readout", attrs: { "aria-live": "off" } });
    var segs = [], items = [];
    function hot(i, on) {
      segs[i].classList.toggle("hot", on);
      items[i].classList.toggle("hot", on);
      var f = fields[i];
      readout.textContent = on ? f.name + " : " + f.type + "   offset " + f.offset + "   tamaño " + f.size + " B   align " + f.align : "";
    }
    var ticks = [];
    fields.forEach(function (f, i) {
      var hue = FIELD_HUES[i % FIELD_HUES.length];
      var seg = el("span", { cls: "bseg", attrs: { title: f.name + ": offset " + f.offset + ", " + f.size + " bytes" } }, [
        el("b", { text: f.name }), el("small", { text: f.size + " B" })
      ]);
      seg.style.left = pct(f.offset, size);
      seg.style.width = pct(f.size, size);
      seg.style.minWidth = "3px";
      seg.style.setProperty("--h", String(hue));
      seg.style.setProperty("--k", String(i));
      bar.appendChild(seg);
      var sw = el("i");
      sw.style.setProperty("--h", String(hue));
      var li = el("li", { attrs: { tabindex: "0" } }, [sw, el("span", { cls: "mono", text: f.name + " : " + f.type + " @" + f.offset })]);
      legend.appendChild(li);
      segs.push(seg);
      items.push(li);
      [seg, li].forEach(function (n) {
        n.addEventListener("mouseenter", function () { hot(i, true); });
        n.addEventListener("mouseleave", function () { hot(i, false); });
      });
      li.addEventListener("focus", function () { hot(i, true); });
      li.addEventListener("blur", function () { hot(i, false); });
      ticks.push(f.offset);
    });
    if (padBytes) legend.appendChild(el("li", {}, [el("i", { cls: "pad" }), el("span", { text: "padding" })]));
    wrap.appendChild(bar);
    var ruler = el("div", { cls: "ruler", attrs: { "aria-hidden": "true" } });
    var shown = [];
    ticks.concat([size]).sort(function (a, b) { return a - b; }).forEach(function (t) {
      if (shown.length && t === shown[shown.length - 1]) return;
      if (shown.length && (t - shown[shown.length - 1]) / size < 0.05 && t !== size) return;
      shown.push(t);
    });
    shown.forEach(function (t) {
      var s = el("span", { text: String(t) });
      s.style.left = pct(t, size);
      if (t === size) s.className = "last";
      ruler.appendChild(s);
    });
    wrap.appendChild(ruler);
    wrap.appendChild(readout);
    return [wrap, legend];
  }

  function regChip(reg, pass) {
    var cls = "reg";
    if (/^xmm/.test(reg) || /SSE/.test(pass || "")) cls += " sse";
    return el("span", { cls: cls, text: reg });
  }

  /* Mapa de registros de la llamada: cada parametro o retorno ocupa un registro. */
  function regMap(f) {
    var used = {};
    (f.params || []).forEach(function (a) { (a.regs || []).forEach(function (rg) { used[rg] = { name: String(a.name), kind: "param" }; }); });
    var ret = f.return || {};
    (ret.regs || []).forEach(function (rg) { if (!used[rg]) used[rg] = { name: "ret", kind: "ret" }; });
    var ints = ["rdi", "rsi", "rdx", "rcx", "r8", "r9", "rax"], sses = ["xmm0", "xmm1", "xmm2", "xmm3"];
    function box(rg) {
      var u = used[rg];
      var cls = "rbox" + (u ? " used" + (u.kind === "ret" ? " ret" : /^xmm/.test(rg) ? " sse" : "") : "");
      return el("div", { cls: cls, attrs: { title: u ? rg + ": " + u.name : rg + ": libre" } }, [el("b", { text: rg }), el("small", { text: u ? u.name : "" })]);
    }
    var wrap = el("div", { attrs: { role: "group", "aria-label": "Registros usados en la llamada" } }, [
      el("div", { cls: "reg-caption", text: "Registros enteros" }),
      el("div", { cls: "regmap" }, ints.map(box))
    ]);
    if (sses.some(function (rg) { return used[rg]; })) {
      wrap.appendChild(el("div", { cls: "reg-caption", text: "Registros SSE" }));
      wrap.appendChild(el("div", { cls: "regmap sse" }, sses.map(box)));
    }
    return wrap;
  }

  function fnCard(f) {
    var title = (f.is_method && f.owner ? f.owner + "::" : "") + f.name;
    var shortName = f.is_method && f.owner && f.name.indexOf(f.owner + "::") === 0 ? f.name : title;
    var head = el("div", { cls: "fn-head" }, [
      el("span", { cls: "fn-name", text: shortName }),
      el("span", { cls: "chips" }, [chip(f.label || "", "info"), chip("frame " + (f.frame_size == null ? "-" : f.frame_size) + " B")])
    ]);
    var body = el("div", { cls: "fn-body" });
    var params = f.params || [];
    if (!params.length) body.appendChild(el("span", { cls: "muted", text: "Sin parámetros." }));
    params.forEach(function (a) {
      var regs = el("span", { cls: "slot-regs" });
      var stackLike = /STACK|MEMORY/.test(a.pass || "") || !(a.regs && a.regs.length);
      (a.regs || []).forEach(function (rg) { regs.appendChild(regChip(rg, a.pass)); });
      if (stackLike) regs.appendChild(el("span", { cls: "reg stack", text: "pila +" + (a.stack_offset == null ? 0 : a.stack_offset) }));
      var meta = el("span", { cls: "slot-meta" });
      if (a.pass) meta.appendChild(el("span", { text: String(a.pass) }));
      if (a.size != null) meta.appendChild(el("span", { text: a.size + " B" }));
      if (a.offset != null) meta.appendChild(el("span", { text: "rbp" + (a.offset >= 0 ? "+" : "") + a.offset }));
      body.appendChild(el("div", { cls: "slot" }, [
        el("span", { cls: "slot-name" }, [document.createTextNode(String(a.name) + " "), el("em", { text: ": " + a.type })]),
        regs, meta
      ]));
    });
    var ret = f.return || {};
    var rr = el("span", { cls: "slot-regs" });
    if (ret.regs && ret.regs.length) ret.regs.forEach(function (rg) { var c = regChip(rg, ret.pass); c.className += " ret"; rr.appendChild(c); });
    else rr.appendChild(el("span", { cls: "reg void", text: /MEMORY/.test(ret.pass || "") ? "memoria" : "void" }));
    body.appendChild(el("div", { cls: "slot ret" }, [
      el("span", { cls: "slot-name" }, [el("span", { cls: "slot-label", text: "retorna " }), el("em", { text: (ret.type || "void") })]),
      rr, ret.pass ? el("span", { cls: "slot-meta" }, [el("span", { text: String(ret.pass) })]) : null
    ]));
    body.appendChild(regMap(f));
    return el("section", { cls: "fn-card" }, [head, body]);
  }

  function renderLayout(r) {
    var p = $("panel-layout");
    clear(p);
    var lay = r && r.layout;
    if (!lay) {
      p.appendChild(placeholder("Sin layout", "Compile con el modo Completo, Ensamblado o Verificar para ver tamaños, offsets y ABI.", "boxes"));
      return;
    }
    var recs = Array.isArray(lay.records) ? lay.records : [];
    p.appendChild(el("h3", { cls: "section-title", text: "Records (" + recs.length + ")" }));
    if (!recs.length) p.appendChild(el("p", { cls: "muted", text: "El programa no declara structs ni clases." }));
    recs.forEach(function (rec) {
      var abi = rec.abi || {}, sp = rec.special || {};
      var head = el("div", { cls: "card-head" }, [el("h3", { text: rec.name || "(sin nombre)" })]);
      var chips = el("div", { cls: "chips" });
      if (abi.class) chips.appendChild(chip("ABI " + abi.class, "accent"));
      if (Array.isArray(abi.eightbytes) && abi.eightbytes.length) chips.appendChild(chip("[" + abi.eightbytes.join(", ") + "]"));
      chips.appendChild(chip(rec.trivial_for_calls ? "trivial en llamadas" : "no trivial en llamadas", rec.trivial_for_calls ? "ok" : "warn"));
      if (rec.needs_dtor) chips.appendChild(chip("requiere destructor", "warn"));
      head.appendChild(chips);
      var card = el("section", { cls: "card record" }, [head]);
      var dl = el("dl", { cls: "kv" });
      kvItem(dl, "Tamaño", rec.size); kvItem(dl, "Alineación", rec.align);
      kvItem(dl, "Ctor por defecto", sp.default_ctor); kvItem(dl, "Ctor con parámetros", sp.param_ctor);
      kvItem(dl, "Ctor de copia", sp.copy_ctor); kvItem(dl, "Asignación de copia", sp.copy_assign);
      kvItem(dl, "Destructor", sp.dtor);
      card.appendChild(dl);
      var fields = Array.isArray(rec.fields) ? rec.fields : [];
      if (fields.length && rec.size > 0) {
        byteDiagram(rec, fields).forEach(function (n) { card.appendChild(n); });
        var fold = el("details", { cls: "fold" }, [el("summary", { text: "Tabla de campos" })]);
        fold.appendChild(el("div", {}, [simpleTable("Campos de " + rec.name, ["Campo", "Tipo", "Offset", "Tamaño", "Align", "const", "Acceso"],
          fields.map(function (f) { return [f.name, f.type, f.offset, f.size, f.align, f.const ? "sí" : "no", f.access]; }), [2, 3, 4])]));
        card.appendChild(fold);
      } else if (!fields.length) {
        card.appendChild(el("p", { cls: "muted pad", text: "Sin campos." }));
      }
      p.appendChild(card);
    });

    var fns = Array.isArray(lay.functions) ? lay.functions : [];
    p.appendChild(el("h3", { cls: "section-title", text: "Funciones y convención de llamada (" + fns.length + ")" }));
    if (fns.length) {
      var grid = el("div", { cls: "fn-grid" });
      fns.forEach(function (f) { grid.appendChild(fnCard(f)); });
      p.appendChild(grid);
    } else p.appendChild(el("p", { cls: "muted", text: "Sin funciones." }));

    var strs = (Array.isArray(lay.strings) && lay.strings.length ? lay.strings.map(function (s) {
      return typeof s === "object" ? { label: s.label || s.name || "", value: String(s.value != null ? s.value : s.text || "") } : { label: "", value: String(s) };
    }) : L.extractStrings(r.assembly));
    p.appendChild(el("h3", { cls: "section-title", text: "Literales de cadena (" + strs.length + ")" }));
    if (strs.length) p.appendChild(simpleTable("Literales", ["Etiqueta", "Valor", "Bytes"], strs.map(function (s) { return [s.label, JSON.stringify(s.value), s.value.length]; }), [2]));
    else p.appendChild(el("p", { cls: "muted", text: "Sin literales de cadena." }));
  }

  /* Assembly */
  function renderAsm(r) {
    var p = $("panel-asm");
    clear(p);
    var asm = r && r.assembly;
    if (typeof asm !== "string") {
      p.appendChild(placeholder("Sin ensamblado", "Genere código con Compilar y ejecutar, Solo compilar o el modo Ensamblado.", "cpu"));
      return;
    }
    var lines = asm.replace(/\n$/, "").split("\n");
    state.asmLines = lines;
    var search = el("input", { attrs: { type: "search", "aria-label": "Buscar en el ensamblado", placeholder: "Buscar (Enter: siguiente)" } });
    var count = el("span", { cls: "count", text: lines.length + " líneas" });
    var tools = el("div", { cls: "tools" }, [
      search, button("Anterior", function () { stepHit(-1); }), button("Siguiente", function () { stepHit(1); }), count,
      button("Copiar", function () { copyText(asm, "Ensamblado"); }, "", "copy"),
      button("Descargar .s", function () { download("programa.s", asm); }, "", "download")
    ]);
    var pre = el("pre", { cls: "asm", attrs: { tabindex: "0", "aria-label": "Listado de ensamblado" } });
    var wrap = el("div", { cls: "asm-wrap" }, [pre]);
    var rendered = 0;
    var moreBar = el("div", { cls: "more-bar" });
    /* Funciones: etiquetas globales (sin prefijo .L) que abren un bloque plegable. */
    var fnStart = {}, fnOf = [], fnNames = [], folded = {};
    lines.forEach(function (ln, i) {
      var m = /^([A-Za-z_][\w$]*):/.exec(ln);
      if (m) { fnNames.push({ name: m[1], line: i }); fnStart[i] = fnNames.length - 1; }
      fnOf.push(fnNames.length - 1);
    });

    function lineNode(i) {
      var code = el("span", { cls: "code" });
      L.tokenizeAsmLine(lines[i]).forEach(function (s) {
        if (s.t === "ws" || s.t === "text" || s.t === "sym") code.appendChild(document.createTextNode(s.s));
        else code.appendChild(el("span", { cls: "t-" + s.t, text: s.s }));
      });
      if (!lines[i]) code.appendChild(document.createTextNode(" "));
      var k = fnOf[i];
      var gut = el("span", { cls: "fg" });
      var attrs = { id: "asm-l" + (i + 1) };
      var cls = "asm-line";
      if (k >= 0) attrs["data-fn"] = String(k);
      if (fnStart[i] !== undefined) {
        cls += " asm-fn";
        var fb = el("button", { cls: "fold-btn", attrs: { type: "button", "aria-expanded": folded[k] ? "false" : "true", "aria-label": "Plegar o desplegar la función " + fnNames[k].name } }, [icon("chevron")]);
        fb.addEventListener("click", function () { setFold(k, !folded[k]); });
        gut.appendChild(fb);
      } else if (k >= 0 && folded[k]) cls += " folded";
      return el("div", { cls: cls, attrs: attrs }, [gut, el("span", { cls: "ln", text: String(i + 1) }), code]);
    }
    function setFold(k, on) {
      folded[k] = on;
      Array.prototype.forEach.call(pre.querySelectorAll('.asm-line[data-fn="' + k + '"]'), function (n) {
        if (!n.classList.contains("asm-fn")) n.classList.toggle("folded", on);
        else n.querySelector(".fold-btn").setAttribute("aria-expanded", on ? "false" : "true");
      });
    }
    function jumpTo(i) {
      if (i >= rendered) renderUpTo(i + 80);
      var n = $("asm-l" + (i + 1));
      if (!n) return;
      if (n.scrollIntoView) n.scrollIntoView({ block: "start" });
      n.classList.add("flash");
      setTimeout(function () { n.classList.remove("flash"); }, 1200);
    }
    function renderUpTo(n) {
      var frag = document.createDocumentFragment();
      for (; rendered < Math.min(n, lines.length); rendered++) frag.appendChild(lineNode(rendered));
      pre.appendChild(frag);
      if (rendered >= lines.length) moreBar.remove();
    }
    renderUpTo(ASM_LINES);
    if (rendered < lines.length) {
      moreBar.appendChild(el("span", { text: "Se muestran " + rendered + " de " + lines.length + " líneas." }));
      moreBar.appendChild(button("Mostrar todo", function () { renderUpTo(lines.length); }));
    }

    function runSearch() {
      pre.querySelectorAll(".asm-line.hit").forEach(function (n) { n.classList.remove("hit"); });
      var q = search.value.toLowerCase();
      state.asmHits = [];
      state.asmHitIdx = -1;
      if (q) lines.forEach(function (ln, i) { if (ln.toLowerCase().indexOf(q) >= 0) state.asmHits.push(i); });
      if (state.asmHits.length) {
        Object.keys(folded).forEach(function (k) { if (folded[k]) setFold(Number(k), false); });
        renderUpTo(lines.length);
        state.asmHits.forEach(function (i) { var n = $("asm-l" + (i + 1)); if (n) n.classList.add("hit"); });
      }
      count.textContent = q ? state.asmHits.length + " coincidencias" : lines.length + " líneas";
    }
    function stepHit(dir) {
      if (!state.asmHits.length) return;
      state.asmHitIdx = (state.asmHitIdx + dir + state.asmHits.length) % state.asmHits.length;
      var n = $("asm-l" + (state.asmHits[state.asmHitIdx] + 1));
      if (n && n.scrollIntoView) n.scrollIntoView({ block: "center" });
      count.textContent = (state.asmHitIdx + 1) + " de " + state.asmHits.length;
    }
    search.addEventListener("input", runSearch);
    search.addEventListener("keydown", function (ev) {
      if (ev.key === "Enter") { ev.preventDefault(); stepHit(ev.shiftKey ? -1 : 1); }
    });
    p.appendChild(tools);
    if (fnNames.length) {
      var idx = el("div", { cls: "asm-idx", attrs: { role: "group", "aria-label": "Ir a una función o etiqueta" } });
      fnNames.slice(0, 28).forEach(function (f) {
        idx.appendChild(el("button", { attrs: { type: "button", title: "Línea " + (f.line + 1) }, text: f.name, on: { click: function () { jumpTo(f.line); } } }));
      });
      p.appendChild(idx);
    }
    p.appendChild(wrap);
    if (rendered < lines.length) p.appendChild(moreBar);
  }

  /* Optimizaciones */
  function describeDetail(d) {
    if (d == null) return "";
    if (typeof d !== "object") return String(d);
    var parts = [];
    if (d.line != null) parts.push("línea " + d.line);
    Object.keys(d).forEach(function (k) {
      if (k === "line" || d[k] === null || typeof d[k] === "object") return;
      parts.push(k === "description" || k === "message" ? String(d[k]) : k + ": " + d[k]);
    });
    return parts.join(" - ");
  }

  var PASS_META = {
    fold: { ico: "sparkle", text: "Plegado de constantes" },
    inline: { ico: "layers", text: "Expansión en línea" },
    sethi: { ico: "tree", text: "Sethi-Ullman: orden de evaluación" }
  };

  function statCard(panel, value, label, opts) {
    opts = opts || {};
    var b = el("b", { cls: opts.small ? "small" : "", text: String(value) });
    var card = el("div", { cls: "stat " + (opts.cls || "") }, [
      el("span", { cls: "stat-ico" }, [icon(opts.ico || "zap")]), b, el("span", { text: label })
    ]);
    if (opts.bar != null) {
      var fill = el("div", { cls: "bar-fill" });
      fill.style.width = Math.max(4, Math.round(opts.bar * 100)) + "%";
      card.appendChild(el("div", { cls: "bar-track" }, [fill]));
    }
    if (typeof opts.count === "number") whenVisible(panel, function () { countUp(b, opts.count, 420); });
    return card;
  }

  function renderOpt(r) {
    var p = $("panel-opt");
    clear(p);
    p._pending = [];
    if (!r) { p.appendChild(placeholder("Sin resultados de optimización", "Compile con -O1 para aplicar fold, inline y sethi.", "zap")); return; }
    var opt = r.optimizations;
    var enabled = !!(opt && opt.enabled);
    var passes = (opt && opt.passes) || [];
    var stats = (opt && opt.stats) || {};
    var total = 0, maxCount = 0;
    Object.keys(stats).forEach(function (k) { var c = (stats[k] && stats[k].count) || 0; total += c; if (c > maxCount) maxCount = c; });
    var grid = el("div", { cls: "stat-grid" });
    grid.appendChild(statCard(p, enabled ? "-O1" : "-O0", "Nivel de optimización", { ico: "zap", small: false }));
    grid.appendChild(statCard(p, passes.length ? passes.join(", ") : "ninguno", "Pases activos", { ico: "layers", small: true }));
    grid.appendChild(statCard(p, total, "Transformaciones aplicadas", { ico: "sparkle", cls: total ? "good" : "", count: total }));
    p.appendChild(grid);
    if (!enabled) {
      p.appendChild(el("div", { cls: "banner" }, [icon("info"), el("span", { text: "Compilado con -O0: no se aplicó ninguna optimización. Seleccione -O1 y vuelva a compilar." })]));
    }
    var names = Object.keys(stats);
    if (names.length) {
      p.appendChild(el("h3", { cls: "section-title", text: "Transformaciones por pase" }));
      var pg = el("div", { cls: "stat-grid" });
      names.forEach(function (name) {
        var s = stats[name] || {};
        var meta = PASS_META[name] || { ico: "zap", text: name };
        pg.appendChild(statCard(p, s.count || 0, name + " - " + meta.text, { ico: meta.ico, cls: s.count ? "accent" : "", count: s.count || 0, bar: maxCount ? (s.count || 0) / maxCount : 0 }));
      });
      p.appendChild(pg);
      names.forEach(function (name) {
        var s = stats[name] || {};
        var details = Array.isArray(s.details) ? s.details : [];
        var fold = el("details", { cls: "fold" }, [el("summary", { text: name + " - " + (s.count || 0) + (s.count === 1 ? " transformación" : " transformaciones") })]);
        var body = el("div");
        if (details.length) {
          var ul = el("ul", { cls: "plain" });
          details.forEach(function (d) { ul.appendChild(el("li", { text: describeDetail(d) })); });
          body.appendChild(ul);
        } else body.appendChild(el("p", { cls: "muted", text: "Sin detalles para este pase." }));
        fold.appendChild(body);
        if (details.length) fold.open = true;
        p.appendChild(fold);
      });
    }
    if (opt && Array.isArray(opt.log) && opt.log.length) {
      p.appendChild(el("h3", { cls: "section-title", text: "Registro" }));
      p.appendChild(el("pre", { cls: "log", text: opt.log.join("\n") }));
    }
    if (opt && (opt.ast_before || opt.ast_after)) {
      p.appendChild(el("h3", { cls: "section-title", text: "AST antes y después" }));
      [["AST antes de optimizar", opt.ast_before], ["AST después de optimizar", opt.ast_after]].forEach(function (pair) {
        var fold = el("details", { cls: "fold" }, [el("summary", { text: pair[0] })]);
        var host = el("div");
        fold.appendChild(host);
        fold.addEventListener("toggle", function () {
          if (fold.open && !host.firstChild) host.appendChild(pair[1] ? createTree(pair[1], true).element : el("p", { cls: "muted", text: "No disponible." }));
        });
        p.appendChild(fold);
      });
    }
    p.appendChild(el("h3", { cls: "section-title", text: "Comparación -O0 contra -O1" }));
    var cbtn = button(state.compare ? "Repetir comparación" : "Comparar -O0 y -O1", runCompare, "primary", "zap");
    cbtn.id = "btn-compare";
    p.appendChild(el("div", { cls: "cmp-actions" }, [cbtn, el("span", { cls: "count muted", text: "Compila y ejecuta ambas variantes con el mismo código y entrada." })]));
    var host2 = el("div", { attrs: { id: "compare-host" } });
    p.appendChild(host2);
    if (state.compare) renderCompare(p, host2, state.compare);
  }

  async function runCompare() {
    if (state.running) return;
    var sel = $("mode-select").value;
    var mode = (sel === "asm" || sel === "run" || sel === "all") ? sel : "all";
    if ((mode === "all" || mode === "run") && state.status && state.status.run_available === false) mode = "asm";
    var req = buildRequest(mode, { compare: true });
    var limit = startBusy("Comparando…", req.timeout);
    var out;
    try { out = await postCompile(req, state.abort.signal); }
    catch (e) { endBusy(limit); setMiniPipe(null, false); setStatus("Comparación cancelada.", "error"); if (M) M.show("reposo"); return; }
    endBusy(limit);
    setMiniPipe(state.last ? L.phaseStatuses(state.last.resp, state.last.mode) : null, false);
    if (out.netError) { setStatus("Error de red.", "error"); toast(out.netError, "err"); if (M) M.show("asustada"); return; }
    var c = out.data && out.data.compare;
    if (!c) {
      setStatus("La comparación falló.", "error");
      state.compare = { error: ((out.data.errors || [])[0] || {}).message || "El servidor no devolvió datos de comparación." };
      if (M) M.show("error");
    } else {
      setStatus("Comparación lista", "");
      state.compare = c;
      if (M) M.show(c.same_output === false ? "sospecha" : "feliz");
    }
    renderOpt(state.last && state.last.resp);
    selectTab("opt");
  }

  /* Grafica de barras SVG propia: lineas de ensamblado en -O0 y -O1. */
  function asmChart(o0, o1) {
    var W = 560, left = 52, barW = W - left - 150, max = Math.max(o0, o1, 1);
    var svg = svgEl("svg", { viewBox: "0 0 " + W + " 108", role: "img", "aria-label": "Líneas de ensamblado: -O0 " + o0 + ", -O1 " + o1 });
    [["-O0", o0, "o0", 12], ["-O1", o1, "o1", 58]].forEach(function (r) {
      var w = Math.max(3, Math.round(barW * r[1] / max));
      var t = svgEl("text", { x: 0, y: r[3] + 22 }); t.textContent = r[0];
      var bar = svgEl("rect", { "class": "bar " + r[2] + (r[2] === "o1" && o1 < o0 ? " better" : ""), x: left, y: r[3], width: w, height: 32, rx: 8 });
      var v = svgEl("text", { "class": "val", x: left + w + 10, y: r[3] + 22 }); v.textContent = r[1] + " líneas";
      svg.appendChild(t); svg.appendChild(bar); svg.appendChild(v);
    });
    var d = o1 - o0;
    var note = svgEl("text", { x: left, y: 104 });
    note.textContent = d === 0 ? "Sin cambios" : (d < 0 ? "Se eliminaron " + (-d) : "Se añadieron " + d) + " líneas (" + (d > 0 ? "+" : "") + Math.round(d / Math.max(o0, 1) * 100) + " %)";
    svg.appendChild(note);
    return el("div", { cls: "chart" }, [el("h4", { text: "Líneas de ensamblado" }), svg]);
  }

  function asmSpans(text) {
    var code = el("span", { cls: "asm" });
    L.tokenizeAsmLine(text).forEach(function (s) {
      if (s.t === "ws" || s.t === "text" || s.t === "sym") code.appendChild(document.createTextNode(s.s));
      else code.appendChild(el("span", { cls: "t-" + s.t, text: s.s }));
    });
    return code;
  }

  function sideBySideDiff(rows) {
    var box = el("div", { cls: "sbs", attrs: { tabindex: "0", role: "region", "aria-label": "Diferencias del ensamblado entre -O0 y -O1" } });
    box.appendChild(el("div", { cls: "sbs-head" }, [el("span", { text: "-O0" }), el("span", { text: "-O1" })]));
    L.sideBySide(rows).forEach(function (row) {
      if (row.gap !== undefined) { box.appendChild(el("div", { cls: "sbs-row gaprow" }, [el("div", { cls: "sbs-gap", text: row.gap })])); return; }
      var left = el("div", { cls: "c " + (row.l === null ? "none" : row.kind === "eq" ? "" : "del"), attrs: row.l !== null && row.kind !== "eq" ? { "data-sign": "-" } : {} });
      var right = el("div", { cls: "c " + (row.r === null ? "none" : row.kind === "eq" ? "" : "add"), attrs: row.r !== null && row.kind !== "eq" ? { "data-sign": "+" } : {} });
      if (row.l !== null) { left.appendChild(asmSpans(row.l)); left.title = row.l; }
      if (row.r !== null) { right.appendChild(asmSpans(row.r)); right.title = row.r; }
      box.appendChild(el("div", { cls: "sbs-row" }, [left, right]));
    });
    return box;
  }

  function renderCompare(panel, host, c) {
    clear(host);
    if (c.error) { host.appendChild(el("div", { cls: "banner error" }, [icon("alert"), el("span", { text: c.error })])); return; }
    var grid = el("div", { cls: "stat-grid" });
    var sameOut = c.same_output, sameExit = c.same_exit_code;
    grid.appendChild(statCard(panel, sameOut == null ? "n/d" : sameOut ? "Sí" : "No", "Misma salida", { ico: sameOut === false ? "x" : "check", cls: sameOut === false ? "bad" : sameOut ? "good" : "" }));
    grid.appendChild(statCard(panel, sameExit == null ? "n/d" : sameExit ? "Sí" : "No", "Mismo código de salida", { ico: sameExit === false ? "x" : "check", cls: sameExit === false ? "bad" : sameExit ? "good" : "" }));
    var al = c.asm_lines || {};
    grid.appendChild(statCard(panel, al.o0 == null ? "n/d" : al.o0, "Líneas de asm en -O0", { ico: "cpu", count: typeof al.o0 === "number" ? al.o0 : undefined }));
    var delta = (al.o0 != null && al.o1 != null) ? al.o1 - al.o0 : null;
    grid.appendChild(statCard(panel, al.o1 == null ? "n/d" : al.o1 + (delta != null && delta !== 0 ? " (" + (delta > 0 ? "+" : "") + delta + ")" : ""), "Líneas de asm en -O1", { ico: "cpu", cls: delta != null && delta < 0 ? "good" : "" }));
    host.appendChild(grid);
    if (typeof al.o0 === "number" && typeof al.o1 === "number") host.appendChild(asmChart(al.o0, al.o1));
    var runs = el("div", { cls: "cards" });
    [["o0", "-O0"], ["o1", "-O1"]].forEach(function (pair) {
      var d = c[pair[0]] || {};
      var parts = [];
      if (!d.success) parts.push("Fallo: " + (((d.errors || [])[0] || {}).message || "sin detalle"));
      var ex = L.describeExit(d);
      var card = el("section", { cls: "card" }, [el("div", { cls: "card-head" }, [el("h3", {}, [icon("terminal"), document.createTextNode("Resultado " + pair[1])]), ex ? chip(ex.text, ex.kind === "ok" ? "ok" : "warn") : null])]);
      var body = el("div", { cls: "card-body" });
      if (parts.length) body.appendChild(el("div", { cls: "banner error" }, [icon("alert"), el("span", { text: parts[0] })]));
      if (d.stdout != null) body.appendChild(renderBigText(el("pre", { cls: "out" }), d.stdout || "(vacío)"));
      else if (!parts.length) body.appendChild(el("pre", { cls: "out", text: "(el modo no ejecuta el programa)" }));
      card.appendChild(body);
      runs.appendChild(card);
    });
    host.appendChild(runs);
    var a0 = (c.o0 && c.o0.assembly), a1 = (c.o1 && c.o1.assembly);
    if (typeof a0 === "string" && typeof a1 === "string") {
      var d = L.summarizeDiff(L.diffLines(a0.replace(/\n$/, "").split("\n"), a1.replace(/\n$/, "").split("\n")), 2, 800);
      host.appendChild(el("h3", { cls: "section-title", text: "Diferencias del ensamblado (-" + d.removed + " / +" + d.added + " líneas)" }));
      if (!d.rows.length || (!d.added && !d.removed)) host.appendChild(el("p", { cls: "muted", text: "El ensamblado es idéntico en ambos niveles." }));
      else {
        host.appendChild(sideBySideDiff(d.rows));
        if (d.truncated) host.appendChild(el("p", { cls: "muted", text: "Diferencia truncada a 800 filas." }));
      }
    }
  }

  /* Separadores */
  function setupSplitter(handle, cfg) {
    var dragging = false;
    function apply(v, persist) {
      v = Math.min(cfg.max(), Math.max(cfg.min, v));
      cfg.set(v);
      handle.setAttribute("aria-valuenow", String(Math.round(v)));
      if (persist) saveStore(cfg.store(v));
      if (state.editor) state.editor.refresh();
    }
    handle.setAttribute("aria-valuemin", String(cfg.min));
    handle.addEventListener("pointerdown", function (ev) {
      dragging = true;
      handle.setPointerCapture(ev.pointerId);
      handle.classList.add("dragging");
      document.body.classList.add("is-dragging", cfg.axis);
      ev.preventDefault();
    });
    handle.addEventListener("pointermove", function (ev) { if (dragging) apply(cfg.fromPointer(ev), false); });
    function end(ev) {
      if (!dragging) return;
      dragging = false;
      handle.classList.remove("dragging");
      document.body.classList.remove("is-dragging", cfg.axis);
      try { handle.releasePointerCapture(ev.pointerId); } catch (e) { /* ya liberado */ }
      apply(cfg.fromPointer(ev), true);
    }
    handle.addEventListener("pointerup", end);
    handle.addEventListener("pointercancel", end);
    handle.addEventListener("keydown", function (ev) {
      var cur = cfg.get();
      var next = null;
      if (ev.key === cfg.dec) next = cur - cfg.step;
      else if (ev.key === cfg.inc) next = cur + cfg.step;
      else if (ev.key === "Home") next = cfg.min;
      else if (ev.key === "End") next = cfg.max();
      if (next !== null) { ev.preventDefault(); apply(next, true); }
    });
    apply(cfg.get(), false);
  }

  function initSplitters() {
    var ws = $("workspace"), left = $("pane-left");
    var pct0 = typeof state.store.leftPct === "number" ? state.store.leftPct : 46;
    ws.style.setProperty("--left", pct0 + "%");
    setupSplitter($("split-main"), {
      axis: "col", min: 20, max: function () { return 75; }, step: 2, dec: "ArrowLeft", inc: "ArrowRight",
      get: function () { return pct0; },
      set: function (v) { pct0 = v; ws.style.setProperty("--left", v + "%"); },
      store: function (v) { return { leftPct: v }; },
      fromPointer: function (ev) { var r = ws.getBoundingClientRect(); return ((ev.clientX - r.left - 29) / (r.width - 40)) * 100; }
    });
    var sh = typeof state.store.stdinH === "number" ? state.store.stdinH : 130;
    setupSplitter($("split-stdin"), {
      axis: "row", min: 60, max: function () { return Math.max(80, left.clientHeight - 220); }, step: 20, dec: "ArrowDown", inc: "ArrowUp",
      get: function () { return sh; },
      set: function (v) { sh = v; left.style.setProperty("--stdin-h", v + "px"); },
      store: function (v) { return { stdinH: v }; },
      fromPointer: function (ev) { var r = left.getBoundingClientRect(); return r.bottom - ev.clientY - 7; }
    });
  }

  /* Ejemplos */
  async function loadExamples() {
    try {
      var res = await fetch("api/examples");
      state.examples = await res.json();
    } catch (e) { state.examples = []; }
    var sel = $("example-select");
    state.examples.forEach(function (ex) {
      sel.appendChild(el("option", { text: ex.titulo, attrs: { value: ex.id, title: ex.descripcion || "" } }));
    });
    if (state.exampleId) sel.value = state.exampleId;
    if (window.MCUI) window.MCUI.examplesLoaded(state.examples);
  }

  async function fetchExample(id) {
    var res = await fetch("api/examples/" + encodeURIComponent(id));
    if (!res.ok) throw new Error("no disponible");
    return res.json();
  }

  async function applyExample(id, silent) {
    try {
      var ex = await fetchExample(id);
      setCode(ex.code);
      $("stdin").value = ex.stdin || "";
      state.exampleId = id;
      $("example-select").value = id;
      saveStore({ exampleId: id, stdin: $("stdin").value, code: ex.code });
      if (window.MCUI) window.MCUI.exampleChanged(id);
      if (!silent) toast("Ejemplo cargado: " + ex.titulo);
      return true;
    } catch (e) {
      toast("No se pudo cargar el ejemplo.", "err");
      return false;
    }
  }

  function confirmReplace() {
    var cur = state.editor.getValue().trim();
    if (!cur) return true;
    return window.confirm("Se reemplazará el código actual del editor. ¿Continuar?");
  }

  /* Estado del servidor */
  async function loadStatus() {
    try {
      var res = await fetch("api/status");
      state.status = await res.json();
    } catch (e) { state.status = null; }
    if (!state.last) renderOutput(null);
    var st = state.status;
    $("sb-dot").className = "sb-dot " + (st && st.compiler_available ? "ok" : "bad");
    $("sb-server-text").textContent = !st ? "Servidor no disponible" : !st.compiler_available ? "Compilador no disponible" : st.run_available === false ? "Solo ensamblado" : "Servidor listo";
    $("foot-info").textContent = st ? [st.compiler_version, st.gcc_version ? "gcc " + st.gcc_version : null, st.platform].filter(Boolean).join("  ·  ") : "Servidor no disponible";
  }

  /* Popover de opciones, con origen en su disparador */
  function initPopover() {
    var btn = $("btn-options"), pop = $("options-pop");
    function setOpen(on, focusBtn) {
      pop.classList.toggle("open", on);
      btn.setAttribute("aria-expanded", on ? "true" : "false");
      if (!on && focusBtn) btn.focus();
    }
    btn.addEventListener("click", function () { setOpen(!pop.classList.contains("open")); });
    document.addEventListener("pointerdown", function (ev) {
      if (pop.classList.contains("open") && !pop.contains(ev.target) && !btn.contains(ev.target)) setOpen(false);
    });
    pop.parentNode.addEventListener("keydown", function (ev) {
      if (ev.key === "Escape" && pop.classList.contains("open")) { ev.stopPropagation(); setOpen(false, true); }
    });
    pop.parentNode.addEventListener("focusout", function (ev) {
      if (ev.relatedTarget && !pop.parentNode.contains(ev.relatedTarget)) setOpen(false);
    });
  }

  function initMascot() {
    var on = state.store.mascot !== false;
    var btn = $("btn-mascot");
    btn.setAttribute("aria-pressed", on ? "true" : "false");
    if (!M) { btn.hidden = true; return; }
    M.init({ host: $("mascot"), dock: $("mascot-dock"), enabled: on });
    btn.addEventListener("click", function () {
      var next = !M.enabled();
      M.setEnabled(next);
      btn.setAttribute("aria-pressed", next ? "true" : "false");
      saveStore({ mascot: next }, true);
      if (!state.last) renderOutput(null);
      if (next) M.show("curiosa");
    });
  }

  function initActions() {
    $("btn-run").addEventListener("click", function () { execute("run"); });
    $("btn-compile").addEventListener("click", function () { execute("compile"); });
    $("btn-cancel").addEventListener("click", function () { if (state.abort) { state.abortReason = "user"; state.abort.abort(); } });
    $("btn-format").addEventListener("click", function () {
      var ed = state.editor, src = ed.getValue(), out = L.formatCode(src, 4);
      if (out === src) { toast("El código ya está formateado"); return; }
      var cur = ed.getCursor();
      ed.replaceRange(out, { line: 0, ch: 0 }, { line: ed.lastLine(), ch: ed.getLine(ed.lastLine()).length });
      ed.setCursor(Math.min(cur.line, ed.lastLine()), 0);
      toast("Código formateado");
    });
    $("btn-reset").addEventListener("click", async function () {
      if (!confirmReplace()) return;
      await applyExample(state.exampleId || "hola_mundo", true) || setCode(DEFAULT_CODE);
      toast("Editor restablecido");
    });
    $("btn-clear").addEventListener("click", function () {
      state.editor.setValue("");
      state.editor.focus();
      toast("Editor vaciado (Ctrl/Cmd+Z para deshacer)");
    });
    $("btn-copy").addEventListener("click", function () { copyText(state.editor.getValue(), "Código"); });
    $("btn-download").addEventListener("click", function () { download("programa.cpp", state.editor.getValue()); });
    document.querySelectorAll("input[name=opt]").forEach(function (r) {
      r.addEventListener("change", function () { syncPassState(); saveStore({ opts: readOpts() }); updateStatusbar(); });
    });
    document.querySelectorAll(".pass").forEach(function (c) { c.addEventListener("change", function () { saveStore({ opts: readOpts() }); }); });
    $("mode-select").addEventListener("change", function () { saveStore({ opts: readOpts() }); syncModeChips(); });
    $("timeout-input").addEventListener("change", function () {
      var v = parseInt(this.value, 10);
      this.value = isNaN(v) ? 5 : Math.min(10, Math.max(1, v));
      saveStore({ opts: readOpts() });
    });
    $("stdin").addEventListener("input", function () { saveStore({ stdin: this.value }); });
    Array.prototype.forEach.call(document.querySelectorAll(".mode-chip"), function (c) {
      c.addEventListener("click", function () {
        $("mode-select").value = c.dataset.mode;
        $("mode-select").dispatchEvent(new Event("change"));
        syncModeChips();
      });
    });
    function bump(d) {
      var inp = $("timeout-input");
      inp.value = Math.min(10, Math.max(1, (parseInt(inp.value, 10) || 5) + d));
      inp.dispatchEvent(new Event("change"));
    }
    $("timeout-dec").addEventListener("click", function () { bump(-1); });
    $("timeout-inc").addEventListener("click", function () { bump(1); });
    $("rail").addEventListener("click", function (ev) {
      var b = ev.target.closest(".rail-btn");
      if (!b) return;
      var v = b.dataset.view;
      if (v === "editor") { document.getElementById("pane-left").scrollIntoView({ block: "nearest" }); state.editor.focus(); }
      else if (v === "palette") { if (window.MCUI) window.MCUI.openPalette(); }
      else selectTab(v, false, ev.detail > 0);
    });
    $("sb-diag").addEventListener("click", function () { selectTab("diag", true); });
    window.addEventListener("resize", function () { syncRail(state.tab); });
    $("example-select").addEventListener("change", async function () {
      var id = this.value;
      if (!id) return;
      if (!confirmReplace()) { this.value = state.exampleId || ""; return; }
      await applyExample(id);
    });
    document.querySelectorAll("input[name=theme]").forEach(function (r) {
      r.addEventListener("change", function () { applyTheme(r.value, true); saveStore({ theme: r.value }, true); });
    });
    document.addEventListener("keydown", function (ev) {
      var mod = ev.ctrlKey || ev.metaKey;
      if (mod && ev.key === "Enter") { ev.preventDefault(); execute("run"); }
      else if (mod && (ev.key === "s" || ev.key === "S")) { ev.preventDefault(); persistNow(); toast("Guardado en este navegador"); }
    });
    if (isMac()) {
      $("run-kbd").textContent = "⌘ ↵";
      document.querySelectorAll(".foot-keys .kbd").forEach(function (k) { k.textContent = k.textContent.replace("Ctrl", "⌘"); });
    }
  }

  function applyTheme(t, animate) {
    var root = document.documentElement;
    if (animate) {
      root.classList.add("theme-anim");
      setTimeout(function () { root.classList.remove("theme-anim"); }, 260);
    }
    if (t === "light" || t === "dark") root.setAttribute("data-theme", t);
    else root.removeAttribute("data-theme");
  }

  async function init() {
    loadStore();
    var theme = state.store.theme || "auto";
    var themeRadio = document.querySelector("input[name=theme][value='" + theme + "']") || document.querySelector("input[name=theme][value=auto]");
    themeRadio.checked = true;
    applyTheme(theme, false);
    var rs = $("run-status");
    rs.appendChild(el("span", { attrs: { "aria-hidden": "true" } }));
    rs.appendChild(el("span", { cls: "sr-only" }));
    state.exampleId = state.store.exampleId || null;
    initEditor();
    initTabs();
    initSegs();
    initSplitters();
    initActions();
    initPopover();
    initMascot();
    syncRail(state.tab);
    requestAnimationFrame(function () { $("rail").classList.add("ready"); });
    Array.prototype.forEach.call($("mini-pipe").children, function (n, i) { n.style.setProperty("--k", String(i)); });
    applyOpts(state.store.opts);
    if (typeof state.store.code === "string" && state.store.code.length) setCode(state.store.code);
    else setCode(DEFAULT_CODE);
    $("stdin").value = typeof state.store.stdin === "string" ? state.store.stdin : "";
    renderAll();
    var examplesDone = loadExamples();
    loadStatus();
    await examplesDone;
    if (!(typeof state.store.code === "string" && state.store.code.length)) {
      await applyExample("hola_mundo", true);
    }
    state.editor.refresh();
  }

  window.MCApp = {
    state: state, goTo: goTo, renderAll: renderAll, execute: execute, selectTab: selectTab, applyExample: applyExample,
    toast: toast, copyText: copyText, download: download, confirmReplace: confirmReplace, isMac: isMac, readOpts: readOpts
  };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
