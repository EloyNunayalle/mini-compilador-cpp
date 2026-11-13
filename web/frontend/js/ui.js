/* Capa de interaccion: paleta de comandos, galeria de ejemplos y atajos globales.
 * Todo texto externo (titulos de ejemplos, etc.) se inserta con textContent. */
(function () {
  "use strict";

  var App = window.MCApp;
  var SVGNS = "http://www.w3.org/2000/svg";
  var examples = [];
  var currentId = null;
  var mac = /Mac|iPhone|iPad/.test(navigator.platform || "");

  function $(id) { return document.getElementById(id); }

  function el(tag, cls, text) {
    var n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text != null) n.textContent = text;
    return n;
  }

  function icon(name) {
    var s = document.createElementNS(SVGNS, "svg");
    s.setAttribute("class", "i");
    s.setAttribute("aria-hidden", "true");
    var u = document.createElementNS(SVGNS, "use");
    u.setAttribute("href", "#i-" + name);
    s.appendChild(u);
    return s;
  }

  function reduced() {
    return !!(window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches);
  }

  function norm(s) { return String(s).normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase(); }

  /* Popover con origen en su disparador. */
  function bindPopover(btn, pop, onOpen) {
    var host = btn.parentNode;
    function setOpen(on, focusBtn) {
      pop.classList.toggle("open", on);
      btn.setAttribute("aria-expanded", on ? "true" : "false");
      if (on && onOpen) onOpen();
      if (!on && focusBtn) btn.focus();
    }
    btn.addEventListener("click", function () { setOpen(!pop.classList.contains("open")); });
    document.addEventListener("pointerdown", function (ev) {
      if (pop.classList.contains("open") && !pop.contains(ev.target) && !btn.contains(ev.target)) setOpen(false);
    });
    host.addEventListener("keydown", function (ev) {
      if (ev.key === "Escape" && pop.classList.contains("open")) { ev.stopPropagation(); setOpen(false, true); }
    });
    return setOpen;
  }

  /* Galeria de ejemplos */
  var TAGS = {
    hola_mundo: ["string", "cout"], aritmetica_control: ["control", "funciones"], punteros_arreglos: ["ptr", "arreglos"],
    structs_metodos: ["ABI", "metodos"], constructores: ["ctor/dtor", "init"], ciclo_vida: ["ctor/dtor", "copia"],
    string_en_record: ["ABI", "string"], const_correctitud: ["const"], records_por_valor: ["ABI", "registros"],
    memoria_dinamica: ["heap", "ctor/dtor"], lectura_cin: ["cin", "stdin"], optimizacion: ["opt", "-O1"], error_de_tipo: ["error", "diagnóstico"]
  };

  function tagsFor(ex) {
    if (TAGS[ex.id]) return TAGS[ex.id];
    var t = norm(ex.titulo + " " + (ex.descripcion || ""));
    var out = [];
    if (/constructor|destructor/.test(t)) out.push("ctor/dtor");
    if (/const/.test(t)) out.push("const");
    if (/abi|convencion|record/.test(t)) out.push("ABI");
    if (/optimiz/.test(t)) out.push("opt");
    return out;
  }

  function tagClass(t) {
    if (t === "ctor/dtor") return "t-ctor";
    if (t === "const") return "t-const";
    if (t === "ABI") return "t-abi";
    if (t === "opt" || t === "-O1") return "t-opt";
    if (t === "error") return "t-err";
    return "";
  }

  var setExOpen = null;

  function renderGallery() {
    var grid = $("ex-grid");
    var q = norm($("ex-search").value.trim());
    while (grid.firstChild) grid.removeChild(grid.firstChild);
    var shown = 0;
    examples.forEach(function (ex) {
      var tags = tagsFor(ex);
      var hay = norm(ex.titulo + " " + (ex.descripcion || "") + " " + tags.join(" "));
      if (q && hay.indexOf(q) < 0) return;
      shown++;
      var card = el("button", "ex-card");
      card.type = "button";
      if (ex.id === currentId) card.setAttribute("aria-current", "true");
      card.appendChild(el("strong", null, ex.titulo));
      card.appendChild(el("p", null, ex.descripcion || ""));
      var tg = el("span", "ex-tags");
      tags.forEach(function (t) { tg.appendChild(el("span", "tag " + tagClass(t), t)); });
      card.appendChild(tg);
      card.addEventListener("click", function () {
        if (!App.confirmReplace()) return;
        if (setExOpen) setExOpen(false, true);
        App.applyExample(ex.id);
      });
      grid.appendChild(card);
    });
    if (!shown) grid.appendChild(el("div", "ex-empty", "Ningún ejemplo coincide con la búsqueda."));
  }

  function setCurrentLabel() {
    var ex = examples.filter(function (e) { return e.id === currentId; })[0];
    $("ex-current").textContent = ex ? ex.titulo : "Elegir…";
  }

  /* Paleta de comandos */
  var pal = { open: false, items: [], sel: 0, last: null, all: [] };

  function clickEl(id) { var n = $(id); if (n) n.click(); }

  function setRadio(name, value) {
    var r = document.querySelector("input[name=" + name + "][value='" + value + "']");
    if (r && !r.checked) { r.checked = true; r.dispatchEvent(new Event("change", { bubbles: true })); }
  }

  function togglePass(name) {
    var o1 = document.querySelector("input[name=opt][value='1']");
    if (!o1.checked) setRadio("opt", "1");
    var c = document.querySelector(".pass[value='" + name + "']");
    if (c) { c.click(); }
  }

  var VIEWS = [["output", "Salida"], ["diag", "Diagnóstico"], ["tokens", "Tokens"], ["ast", "AST"], ["typed", "AST con tipos"],
    ["layout", "Layout y ABI"], ["asm", "Assembly"], ["opt", "Optimizaciones"]];

  function buildCommands() {
    var list = [];
    function add(g, t, ico, run, sub) { list.push({ g: g, t: t, ico: ico, run: run, sub: sub || "" }); }
    var mod = mac ? "⌘" : "Ctrl";
    add("Acciones", "Compilar y ejecutar", "play", function () { App.execute("run"); }, mod + " ↵");
    add("Acciones", "Solo compilar", "cpu", function () { App.execute("compile"); });
    add("Acciones", "Formatear código", "format", function () { clickEl("btn-format"); });
    add("Acciones", "Copiar código", "copy", function () { clickEl("btn-copy"); });
    add("Acciones", "Descargar código .cpp", "download", function () { clickEl("btn-download"); });
    add("Acciones", "Restablecer el editor", "undo", function () { clickEl("btn-reset"); });
    add("Acciones", "Vaciar el editor", "trash", function () { clickEl("btn-clear"); });
    add("Acciones", "Copiar ensamblado", "copy", function () {
      var r = App.state.last && App.state.last.resp;
      if (r && typeof r.assembly === "string") App.copyText(r.assembly, "Ensamblado"); else App.toast("Aún no hay ensamblado", "err");
    });
    add("Acciones", "Descargar ensamblado .s", "download", function () {
      var r = App.state.last && App.state.last.resp;
      if (r && typeof r.assembly === "string") App.download("programa.s", r.assembly); else App.toast("Aún no hay ensamblado", "err");
    });
    VIEWS.forEach(function (v, i) { add("Vistas", "Ir a " + v[1], "arrow", function () { App.selectTab(v[0]); }, i < 8 ? "Alt " + (i + 1) : ""); });
    add("Optimización", "Usar -O0", "zap", function () { setRadio("opt", "0"); });
    add("Optimización", "Usar -O1", "zap", function () { setRadio("opt", "1"); });
    ["fold", "inline", "sethi"].forEach(function (p) { add("Optimización", "Alternar el pase " + p, "sparkle", function () { togglePass(p); }, "-O1"); });
    ["all", "run", "asm", "check", "ast", "scan"].forEach(function (m) {
      add("Modo de salida", "Modo " + m, "terminal", function () {
        var s = $("mode-select"); s.value = m; s.dispatchEvent(new Event("change"));
      });
    });
    add("Apariencia", "Tema claro", "sun", function () { setRadio("theme", "light"); });
    add("Apariencia", "Tema oscuro", "moon", function () { setRadio("theme", "dark"); });
    add("Apariencia", "Tema automático", "monitor", function () { setRadio("theme", "auto"); });
    add("Apariencia", "Mostrar u ocultar la mascota", "car", function () { clickEl("btn-mascot"); });
    examples.forEach(function (ex) {
      add("Ejemplos", "Cargar ejemplo: " + ex.titulo, "layers", function () {
        if (App.confirmReplace()) App.applyExample(ex.id);
      }, tagsFor(ex).slice(0, 2).join(" · "));
    });
    return list;
  }

  /* Coincidencia difusa por subsecuencia con bonos por inicio de palabra y rachas. */
  function fuzzy(q, text) {
    var t = norm(text), score = 0, ti = 0, last = -2, idx = [];
    for (var qi = 0; qi < q.length; qi++) {
      var c = q.charAt(qi);
      if (c === " ") continue;
      var f = t.indexOf(c, ti);
      if (f < 0) return null;
      score += 1;
      if (f === last + 1) score += 3;
      if (f === 0 || t.charAt(f - 1) === " " || t.charAt(f - 1) === ":") score += 4;
      score -= Math.min(3, (f - ti) * 0.2);
      idx.push(f);
      last = f;
      ti = f + 1;
    }
    if (t.indexOf(q) >= 0) score += 8;
    return { score: score, idx: idx };
  }

  function highlight(node, text, idx) {
    var set = {};
    (idx || []).forEach(function (i) { set[i] = true; });
    var buf = "", inMark = false;
    function flush() {
      if (!buf) return;
      if (inMark) node.appendChild(el("mark", null, buf)); else node.appendChild(document.createTextNode(buf));
      buf = "";
    }
    for (var i = 0; i < text.length; i++) {
      var m = !!set[i];
      if (m !== inMark) { flush(); inMark = m; }
      buf += text.charAt(i);
    }
    flush();
  }

  function renderPalette() {
    var list = $("pal-list"), input = $("pal-input");
    var q = norm(input.value.trim());
    while (list.firstChild) list.removeChild(list.firstChild);
    var rows = [];
    pal.all.forEach(function (c) {
      if (!q) { rows.push({ c: c, score: 0, idx: [] }); return; }
      var m = fuzzy(q, c.t);
      if (!m && c.sub) { var m2 = fuzzy(q, c.g + " " + c.sub); if (m2) m = { score: m2.score - 6, idx: [] }; }
      if (!m) { var m3 = fuzzy(q, c.g); if (m3) m = { score: m3.score - 8, idx: [] }; }
      if (m) rows.push({ c: c, score: m.score, idx: m.idx });
    });
    if (q) rows.sort(function (a, b) { return b.score - a.score; });
    rows = rows.slice(0, q ? 40 : 60);
    pal.items = [];
    var group = null;
    rows.forEach(function (r) {
      if (!q && r.c.g !== group) {
        group = r.c.g;
        var gh = el("li", "pal-group", group);
        gh.setAttribute("role", "presentation");
        list.appendChild(gh);
      }
      var li = el("li", "pal-item");
      li.id = "pal-opt-" + pal.items.length;
      li.setAttribute("role", "option");
      li.setAttribute("aria-selected", "false");
      li.appendChild(icon(r.c.ico));
      var title = el("span", "pal-title");
      highlight(title, r.c.t, r.idx);
      li.appendChild(title);
      if (r.c.sub) li.appendChild(el("span", "pal-sub", r.c.sub));
      var item = r.c;
      li.addEventListener("click", function () { run(item); });
      li.addEventListener("pointermove", function () { var k = Number(li.id.slice(8)); if (k !== pal.sel) select(k, false); });
      list.appendChild(li);
      pal.items.push({ li: li, c: item });
    });
    if (!pal.items.length) {
      var e = el("li", "pal-empty", "Sin resultados. Pruebe con otra palabra.");
      e.setAttribute("role", "presentation");
      list.appendChild(e);
    }
    select(0, false);
  }

  function select(i, scroll) {
    if (!pal.items.length) { $("pal-input").setAttribute("aria-activedescendant", ""); return; }
    pal.sel = (i + pal.items.length) % pal.items.length;
    pal.items.forEach(function (it, k) { it.li.setAttribute("aria-selected", k === pal.sel ? "true" : "false"); });
    var cur = pal.items[pal.sel].li;
    $("pal-input").setAttribute("aria-activedescendant", cur.id);
    if (scroll !== false && cur.scrollIntoView) cur.scrollIntoView({ block: "nearest" });
  }

  function openPalette() {
    if (pal.open) return;
    pal.open = true;
    pal.last = document.activeElement;
    pal.all = buildCommands();
    var box = $("palette");
    box.hidden = false;
    box.classList.remove("enter");
    if (!reduced()) { void box.offsetWidth; box.classList.add("enter"); }
    $("pal-input").value = "";
    renderPalette();
    $("pal-input").focus();
  }

  /* Cierre inmediato: sin animacion de salida. */
  function closePalette(restore) {
    if (!pal.open) return;
    pal.open = false;
    var box = $("palette");
    box.hidden = true;
    box.classList.remove("enter");
    if (restore !== false && pal.last && pal.last.focus) pal.last.focus();
  }

  function run(c) {
    closePalette(true);
    setTimeout(function () { c.run(); }, 0);
  }

  function initPalette() {
    var input = $("pal-input");
    input.addEventListener("input", renderPalette);
    input.addEventListener("keydown", function (ev) {
      if (ev.key === "ArrowDown") { ev.preventDefault(); select(pal.sel + 1); }
      else if (ev.key === "ArrowUp") { ev.preventDefault(); select(pal.sel - 1); }
      else if (ev.key === "Home") { ev.preventDefault(); select(0); }
      else if (ev.key === "End") { ev.preventDefault(); select(pal.items.length - 1); }
      else if (ev.key === "Enter") { ev.preventDefault(); if (pal.items[pal.sel]) run(pal.items[pal.sel].c); }
      else if (ev.key === "Escape") { ev.preventDefault(); ev.stopPropagation(); closePalette(true); }
      else if (ev.key === "Tab") ev.preventDefault();
    });
    $("pal-backdrop").addEventListener("click", function () { closePalette(true); });
    $("btn-palette").addEventListener("click", openPalette);
    if (mac) $("pal-kbd").textContent = "⌘ K";
  }

  function initShortcuts() {
    document.addEventListener("keydown", function (ev) {
      var mod = ev.ctrlKey || ev.metaKey;
      if (mod && (ev.key === "k" || ev.key === "K")) {
        ev.preventDefault();
        if (pal.open) closePalette(true); else openPalette();
      } else if (ev.altKey && !mod && /^Digit[1-8]$/.test(ev.code || "")) {
        ev.preventDefault();
        var n = Number(ev.code.slice(5));
        if (n === 1) { var ed = App.state.editor; if (ed) ed.focus(); return; }
        var views = ["output", "diag", "tokens", "ast", "layout", "asm", "opt"];
        App.selectTab(views[n - 2], false);
      }
    });
  }

  function init() {
    setExOpen = bindPopover($("btn-examples"), $("examples-pop"), function () {
      renderGallery();
      setTimeout(function () { $("ex-search").focus(); }, 0);
    });
    $("ex-search").addEventListener("input", renderGallery);
    initPalette();
    initShortcuts();
  }

  window.MCUI = {
    examplesLoaded: function (list) { examples = list || []; currentId = App.state.exampleId || currentId; setCurrentLabel(); renderGallery(); },
    exampleChanged: function (id) { currentId = id; setCurrentLabel(); renderGallery(); },
    openExamples: function () { var b = $("btn-examples"); if (b.getAttribute("aria-expanded") !== "true") b.click(); },
    openPalette: openPalette
  };

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
