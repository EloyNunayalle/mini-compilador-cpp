/* Funciones puras del frontend (sin DOM). Se cargan en el navegador como window.MC
 * y en node con require() para las pruebas. */
(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.MC = factory();
})(typeof self !== "undefined" ? self : this, function () {
  "use strict";

  var PHASES = ["lex", "parse", "typecheck", "layout", "gencode", "assemble", "run"];
  var PHASE_LABELS = {
    lex: "Léxico", parse: "Sintáctico", typecheck: "Typecheck", layout: "Layout",
    gencode: "Gencode", assemble: "Ensamblado", run: "Ejecución", internal: "Interno", request: "Petición"
  };
  var MODE_PHASES = {
    scan: 1, ast: 2, check: 4, asm: 5, all: 7, run: 7
  };

  /* Escapa texto para contextos HTML; el frontend usa textContent, esto es para exportar. */
  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  }

  function phaseLabel(p) { return PHASE_LABELS[p] || String(p || ""); }

  /* Formateo simple por llaves. Respeta cadenas, caracteres y comentarios. */
  function formatCode(src, unit) {
    unit = unit || 4;
    var lines = String(src).replace(/\r\n?/g, "\n").split("\n");
    var depth = 0;
    var paren = 0;
    var inBlock = false;
    var out = [];
    var blank = 0;
    for (var i = 0; i < lines.length; i++) {
      var raw = lines[i].replace(/\s+$/, "");
      var text = raw.replace(/^\s+/, "");
      if (text === "") {
        blank++;
        if (blank <= 1 && out.length) out.push("");
        continue;
      }
      blank = 0;
      if (inBlock) {
        var pad0 = new Array(depth * unit + 1).join(" ");
        out.push(text.charAt(0) === "*" ? pad0 + " " + text : raw);
        if (text.indexOf("*/") >= 0) inBlock = false;
        continue;
      }
      if (text.charAt(0) === "#") { out.push(text); continue; }
      var scan = scanLine(text, inBlock);
      inBlock = scan.inBlock;
      var lead = 0;
      while (lead < text.length && text.charAt(lead) === "}") lead++;
      var level = depth - Math.min(lead, depth);
      var isLabel = /^(public|private|protected)\s*:/.test(text);
      if (isLabel && level > 0) level--;
      var extra = paren > 0 ? 1 : 0;
      out.push(new Array((level + extra) * unit + 1).join(" ") + text);
      depth = Math.max(0, depth + scan.braces);
      paren = Math.max(0, paren + scan.parens);
    }
    while (out.length && out[out.length - 1] === "") out.pop();
    return out.join("\n") + "\n";
  }

  function scanLine(text, inBlock) {
    var braces = 0, parens = 0, i = 0, n = text.length;
    while (i < n) {
      var c = text.charAt(i), d = text.charAt(i + 1);
      if (inBlock) {
        if (c === "*" && d === "/") { inBlock = false; i += 2; continue; }
        i++; continue;
      }
      if (c === "/" && d === "/") break;
      if (c === "/" && d === "*") { inBlock = true; i += 2; continue; }
      if (c === '"' || c === "'") {
        var q = c; i++;
        while (i < n && text.charAt(i) !== q) { if (text.charAt(i) === "\\") i++; i++; }
        i++; continue;
      }
      if (c === "{") braces++;
      else if (c === "}") braces--;
      else if (c === "(") parens++;
      else if (c === ")") parens--;
      i++;
    }
    return { braces: braces, parens: parens, inBlock: inBlock };
  }

  /* Segmenta una linea AT&T en trozos {t, s}; la concatenacion de s es la linea original. */
  function tokenizeAsmLine(line) {
    var segs = [];
    function push(t, s) { if (s) segs.push({ t: t, s: s }); }
    var m, rest = line, ci = findComment(line);
    var comment = "";
    if (ci >= 0) { comment = line.slice(ci); rest = line.slice(0, ci); }
    m = /^(\s*)([A-Za-z_.$][\w.$@]*|\d+):/.exec(rest);
    if (m) {
      push("ws", m[1]);
      push("label", m[2] + ":");
      rest = rest.slice(m[0].length);
    }
    m = /^(\s*)(\.[\w.]+)/.exec(rest);
    if (m) {
      push("ws", m[1]);
      push("directive", m[2]);
      rest = rest.slice(m[0].length);
    } else {
      m = /^(\s*)([A-Za-z][\w]*)/.exec(rest);
      if (m) {
        push("ws", m[1]);
        push("instr", m[2]);
        rest = rest.slice(m[0].length);
      }
    }
    var re = /("(?:\\.|[^"\\])*"|%[a-z][a-z0-9]*|\$-?(?:0x[0-9a-fA-F]+|\w+)|-?0x[0-9a-fA-F]+|-?\d+|[A-Za-z_.$][\w.$@]*)/g;
    var last = 0;
    while ((m = re.exec(rest))) {
      push("text", rest.slice(last, m.index));
      var tok = m[0], t;
      if (tok.charAt(0) === '"') t = "string";
      else if (tok.charAt(0) === "%") t = "reg";
      else if (tok.charAt(0) === "$") t = "imm";
      else if (/^-?(0x[0-9a-fA-F]+|\d+)$/.test(tok)) t = "num";
      else t = "sym";
      push(t, tok);
      last = m.index + tok.length;
    }
    push("text", rest.slice(last));
    push("comment", comment);
    return segs;
  }

  function findComment(line) {
    var inStr = false;
    for (var i = 0; i < line.length; i++) {
      var c = line.charAt(i);
      if (inStr) {
        if (c === "\\") i++;
        else if (c === '"') inStr = false;
      } else if (c === '"') inStr = true;
      else if (c === "#") return i;
    }
    return -1;
  }

  /* Literales de cadena declarados con .string/.asciz y la etiqueta que los precede. */
  function extractStrings(asm) {
    var out = [];
    if (typeof asm !== "string") return out;
    var label = null;
    asm.split("\n").forEach(function (ln, idx) {
      var m = /^\s*([A-Za-z_.$][\w.$@]*):\s*(?:#.*)?$/.exec(ln);
      if (m) { label = m[1]; return; }
      m = /^\s*(?:\.[\w.]*:\s*)?\.(string|asciz|ascii)\s+("(?:\\.|[^"\\])*")/.exec(ln);
      if (m) {
        out.push({ label: label || "", value: m[2].slice(1, -1), line: idx + 1 });
        label = null;
      }
    });
    return out;
  }

  /* Diff de lineas: prefijo y sufijo comunes y LCS en el centro (con tope de tamano). */
  function diffLines(a, b) {
    var s = 0;
    while (s < a.length && s < b.length && a[s] === b[s]) s++;
    var ea = a.length, eb = b.length;
    while (ea > s && eb > s && a[ea - 1] === b[eb - 1]) { ea--; eb--; }
    var ma = a.slice(s, ea), mb = b.slice(s, eb);
    var ops = [], i;
    for (i = 0; i < s; i++) ops.push({ op: "=", text: a[i] });
    var mid = [];
    if (ma.length * mb.length <= 4000000) {
      var n = ma.length, m = mb.length, w = m + 1;
      var t = new Uint32Array((n + 1) * w);
      for (i = n - 1; i >= 0; i--) {
        for (var j = m - 1; j >= 0; j--) {
          t[i * w + j] = ma[i] === mb[j] ? t[(i + 1) * w + j + 1] + 1
            : Math.max(t[(i + 1) * w + j], t[i * w + j + 1]);
        }
      }
      i = 0; var k = 0;
      while (i < n && k < m) {
        if (ma[i] === mb[k]) { mid.push({ op: "=", text: ma[i] }); i++; k++; }
        else if (t[(i + 1) * w + k] >= t[i * w + k + 1]) { mid.push({ op: "-", text: ma[i++] }); }
        else { mid.push({ op: "+", text: mb[k++] }); }
      }
      while (i < n) mid.push({ op: "-", text: ma[i++] });
      while (k < m) mid.push({ op: "+", text: mb[k++] });
    } else {
      ma.forEach(function (x) { mid.push({ op: "-", text: x }); });
      mb.forEach(function (x) { mid.push({ op: "+", text: x }); });
    }
    ops = ops.concat(mid);
    for (i = ea; i < a.length; i++) ops.push({ op: "=", text: a[i] });
    return ops;
  }

  /* Reduce un diff a las zonas cambiadas con `ctx` lineas de contexto. */
  function summarizeDiff(ops, ctx, maxRows) {
    ctx = ctx == null ? 2 : ctx;
    maxRows = maxRows || 600;
    var keep = new Array(ops.length), i, j;
    for (i = 0; i < ops.length; i++) {
      if (ops[i].op !== "=") {
        for (j = Math.max(0, i - ctx); j <= Math.min(ops.length - 1, i + ctx); j++) keep[j] = true;
      }
    }
    var rows = [], skipped = 0, added = 0, removed = 0;
    for (i = 0; i < ops.length; i++) {
      if (ops[i].op === "+") added++;
      if (ops[i].op === "-") removed++;
      if (keep[i]) {
        if (skipped) { rows.push({ op: "~", text: skipped + " líneas iguales" }); skipped = 0; }
        if (rows.length < maxRows) rows.push(ops[i]);
      } else skipped++;
    }
    if (skipped && (added || removed)) rows.push({ op: "~", text: skipped + " líneas iguales" });
    if (!added && !removed) rows = [];
    return { rows: rows, added: added, removed: removed, truncated: rows.length >= maxRows };
  }

  /* Normaliza un nodo del AST: tolera campos ausentes o nulos. */
  function normalizeNode(n) {
    if (!n || typeof n !== "object") return null;
    return {
      kind: typeof n.kind === "string" && n.kind ? n.kind : "Nodo",
      line: isNum(n.line) ? n.line : null,
      col: isNum(n.col) ? n.col : (isNum(n.column) ? n.column : null),
      label: n.label == null ? "" : String(n.label),
      type: n.type == null ? "" : String(n.type),
      isConst: n.const === true,
      attrs: n.attrs && typeof n.attrs === "object" && !Array.isArray(n.attrs) ? n.attrs : {},
      children: Array.isArray(n.children) ? n.children : []
    };
  }

  /* Categoria visual de un nodo del AST segun su kind. */
  function astCategory(kind) {
    var k = String(kind || "");
    if (/^(Number|Bool|Char|Real|String|Null)Exp$/.test(k) || /Literal$/.test(k)) return "literal";
    if (/^IdExp$|Ident$|^Name$/.test(k)) return "ident";
    if (/Stmt$/.test(k)) return "stmt";
    if (/Exp$|Expr$/.test(k)) return "expr";
    return "decl";
  }

  /* Rutas a contraer al inicio: los nodos con hijos a profundidad maxDepth-1 o mas. */
  function initialCollapsed(root, maxDepth) {
    var out = {};
    function walk(raw, path, depth) {
      var n = normalizeNode(raw);
      if (!n || !n.children.length) return;
      if (depth >= maxDepth - 1) { out[path] = true; return; }
      n.children.forEach(function (c, i) { walk(c, path + "." + i, depth + 1); });
    }
    walk(root, "0", 0);
    return out;
  }

  /* Quita la marca de contraido en todos los ancestros de path (para revelar un nodo). */
  function revealPath(collapsed, path) {
    var parts = String(path).split(".");
    for (var i = 1; i < parts.length; i++) delete collapsed[parts.slice(0, i).join(".")];
    return collapsed;
  }

  function trunc(s, n) { s = String(s); return s.length > n ? s.slice(0, n - 1) + "\u2026" : s; }

  /* Diseno horizontal del arbol visible: columnas por profundidad y hojas en filas consecutivas.
   * Cada padre se centra sobre sus hijos, asi que no hay solapes. Devuelve nodos, aristas y tamano. */
  function layoutTree(root, opts) {
    opts = opts || {};
    var collapsed = opts.collapsed || {};
    var typed = !!opts.typed;
    var gapX = opts.gapX == null ? 46 : opts.gapX, gapY = opts.gapY == null ? 14 : opts.gapY;
    var nodes = [], edges = [], cols = [], next = 0, byId = {};

    function descendants(raw) {
      var n = normalizeNode(raw);
      return n ? countNodes(n) - 1 : 0;
    }

    function visit(raw, path, depth, parent) {
      var n = normalizeNode(raw);
      if (!n) return null;
      var hasKids = n.children.length > 0;
      var isCol = hasKids && !!collapsed[path];
      var label = trunc(n.label, 30);
      var chips = typed && (n.type || n.isConst);
      var w = Math.max(n.kind.length * 7.6 + 30, label.length * 6.9 + 30, chips ? trunc(n.type, 22).length * 6.6 + (n.isConst ? 52 : 28) : 0, 120);
      w = Math.min(Math.round(w), 270);
      var h = chips ? 64 : 48;
      var info = {
        id: path, parent: parent, depth: depth, kind: n.kind, label: n.label, shown: label, type: n.type, isConst: n.isConst,
        line: n.line, col: n.col, attrs: n.attrs, cat: astCategory(n.kind), childCount: n.children.length,
        collapsed: isCol, hidden: isCol ? descendants(raw) : 0, w: w, h: h, x: 0, y: 0, chips: !!chips
      };
      nodes.push(info);
      byId[path] = info;
      cols[depth] = Math.max(cols[depth] || 0, w);
      var kids = [];
      if (hasKids && !isCol) {
        n.children.forEach(function (c, i) {
          var ch = visit(c, path + "." + i, depth + 1, path);
          if (ch) { kids.push(ch); edges.push({ from: path, to: ch.id }); }
        });
      }
      if (kids.length) {
        var a = kids[0], b = kids[kids.length - 1];
        info.y = (a.y + a.h / 2 + b.y + b.h / 2) / 2 - h / 2;
      } else {
        info.y = next;
        next += h + gapY;
      }
      return info;
    }

    visit(root, "0", 0, null);
    var xs = [], acc = 0;
    cols.forEach(function (w, d) { xs[d] = acc; acc += w + gapX; });
    var width = 0, height = 0, minY = 0;
    nodes.forEach(function (n) { if (n.y < minY) minY = n.y; });
    nodes.forEach(function (n) {
      n.x = xs[n.depth];
      n.y -= minY;
      width = Math.max(width, n.x + n.w);
      height = Math.max(height, n.y + n.h);
    });
    return { nodes: nodes, edges: edges, byId: byId, width: width, height: height, visible: nodes.length, total: root ? countNodes(root) : 0 };
  }

  function isNum(v) { return typeof v === "number" && isFinite(v); }

  /* Busca nodos cuyo texto contenga q; devuelve rutas coincidentes y rutas a expandir. */
  function searchTree(root, q) {
    var matches = {}, expand = {}, count = 0;
    q = String(q || "").trim().toLowerCase();
    if (!q || !root) return { matches: matches, expand: expand, count: 0 };
    function text(n) {
      var parts = [n.kind, n.label, n.type];
      Object.keys(n.attrs).forEach(function (k) { parts.push(k + "=" + n.attrs[k]); });
      if (n.isConst) parts.push("const");
      return parts.join(" ").toLowerCase();
    }
    function walk(raw, path) {
      var n = normalizeNode(raw);
      if (!n) return false;
      var hit = text(n).indexOf(q) >= 0;
      var below = false;
      for (var i = 0; i < n.children.length; i++) {
        if (walk(n.children[i], path + "." + i)) below = true;
      }
      if (hit) { matches[path] = true; count++; }
      if (below) expand[path] = true;
      return hit || below;
    }
    walk(root, "0");
    return { matches: matches, expand: expand, count: count };
  }

  function countNodes(raw) {
    var n = normalizeNode(raw);
    if (!n) return 0;
    var c = 1;
    for (var i = 0; i < n.children.length; i++) c += countNodes(n.children[i]);
    return c;
  }

  /* Resumen corto de atributos para mostrar junto al nodo. */
  function attrChips(attrs, typed) {
    var order = ["offset", "size", "align", "storage"];
    var chips = [];
    if (!typed || !attrs) return chips;
    order.forEach(function (k) { if (attrs[k] != null) chips.push(k + " " + attrs[k]); });
    Object.keys(attrs).forEach(function (k) {
      var v = attrs[k];
      if (order.indexOf(k) >= 0 || v === null || typeof v === "object") return;
      chips.push(k + " " + v);
    });
    return chips;
  }

  function filterTokens(tokens, q) {
    if (!Array.isArray(tokens)) return [];
    q = String(q || "").trim().toLowerCase();
    if (!q) return tokens;
    return tokens.filter(function (t) {
      return String(t.type).toLowerCase().indexOf(q) >= 0 ||
        String(t.lexeme).toLowerCase().indexOf(q) >= 0 ||
        String(t.line) === q;
    });
  }

  /* Corta texto largo en un maximo de lineas. */
  function truncateLines(text, maxLines) {
    text = text == null ? "" : String(text);
    var idx = -1, count = 0;
    while (count < maxLines) {
      idx = text.indexOf("\n", idx + 1);
      if (idx < 0) return { text: text, shown: countLines(text), total: countLines(text), truncated: false };
      count++;
    }
    var total = countLines(text);
    return { text: text.slice(0, idx), shown: maxLines, total: total, truncated: total > maxLines };
  }

  function countLines(text) {
    if (!text) return 0;
    var n = 1, i = -1;
    while ((i = text.indexOf("\n", i + 1)) >= 0) n++;
    if (text.charAt(text.length - 1) === "\n") n--;
    return n;
  }

  /* Estado por fase: "ok", "fail" o "skip", segun la respuesta y el modo pedido. */
  function phaseStatuses(resp, mode) {
    var res = {};
    PHASES.forEach(function (p) { res[p] = "skip"; });
    if (!resp) return res;
    var expected = MODE_PHASES[mode] || 7;
    var errs = Array.isArray(resp.errors) ? resp.errors : [];
    var failIdx = -1;
    errs.forEach(function (e) {
      var i = PHASES.indexOf(e && e.phase);
      if (i >= 0 && (failIdx < 0 || i < failIdx)) failIdx = i;
    });
    if (failIdx < 0 && resp.success === false) {
      var pi = PHASES.indexOf(resp.phase);
      if (pi >= 0) failIdx = pi;
    }
    var reached = expected;
    if (failIdx >= 0) reached = failIdx;
    else if (resp.success && resp.phase && PHASES.indexOf(resp.phase) >= 0 && resp.phase !== "done") {
      reached = Math.min(expected, PHASES.indexOf(resp.phase) + 1);
    }
    if (failIdx < 0 && resp.success === true && resp.duration_ms == null && expected > 5) {
      reached = Math.min(reached, 5);
    }
    for (var i = 0; i < PHASES.length; i++) {
      if (i < reached) res[PHASES[i]] = "ok";
    }
    if (failIdx >= 0) res[PHASES[failIdx]] = "fail";
    return res;
  }

  function describeExit(resp) {
    if (!resp || resp.duration_ms == null && resp.exit_code == null) return null;
    var timeout = (resp.errors || []).some(function (e) { return e && e.phase === "run" && /tiempo l/i.test(e.message || ""); });
    if (timeout) return { kind: "timeout", text: "Tiempo límite excedido" };
    if (resp.signal) return { kind: "signal", text: "Terminado por señal " + resp.signal + " (código " + resp.exit_code + ")" };
    if (resp.exit_code === 0) return { kind: "ok", text: "Código de salida 0" };
    if (resp.exit_code == null) return null;
    return { kind: "nonzero", text: "Código de salida " + resp.exit_code };
  }

  /* Columna final (exclusiva, base 1) del token que empieza en col. */
  function tokenEndColumn(lineText, col) {
    var s = String(lineText || ""), i = Math.max(0, (col || 1) - 1);
    if (i >= s.length) return Math.max(1, s.length) + 1;
    var m = /^([A-Za-z_]\w*|\d+(?:\.\d+)?)/.exec(s.slice(i));
    if (m) return i + m[0].length + 1;
    if (s.charAt(i) === '"') {
      var j = i + 1;
      while (j < s.length && s.charAt(j) !== '"') { if (s.charAt(j) === "\\") j++; j++; }
      return Math.min(s.length, j + 1) + 1;
    }
    return i + 2;
  }

  function errorReport(resp, mode) {
    var out = ["Reporte de diagnóstico", "Modo: " + (mode || "-")];
    if (!resp) return out.concat(["Sin resultados."]).join("\n") + "\n";
    out.push("Éxito: " + (resp.success ? "sí" : "no"), "Fase final: " + phaseLabel(resp.phase));
    var list = [].concat(resp.errors || [], []);
    var warn = resp.warnings || [];
    out.push("", "Errores (" + list.length + "):");
    list.forEach(function (e) { out.push("  " + fmtDiag(e)); });
    out.push("", "Advertencias (" + warn.length + "):");
    warn.forEach(function (e) { out.push("  " + fmtDiag(e)); });
    var exit = describeExit(resp);
    if (exit) out.push("", "Ejecución: " + exit.text);
    if (resp.stderr) out.push("", "stderr:", resp.stderr.replace(/\n$/, ""));
    return out.join("\n") + "\n";
  }

  function fmtDiag(e) {
    e = e || {};
    var pos = e.line != null ? " (línea " + e.line + (e.column != null ? ", columna " + e.column : "") + ")" : "";
    return "[" + phaseLabel(e.phase) + "] " + (e.message || "") + pos;
  }

  function formatMs(ms) {
    if (ms == null) return "-";
    return ms < 1000 ? ms + " ms" : (ms / 1000).toFixed(2) + " s";
  }


  /* Sugerencia breve segun el texto del diagnostico. */
  function suggestFor(d) {
    if (!d) return "";
    var m = String(d.message || "").toLowerCase();
    var rules = [
      [/tiempo l/, "Reduzca el trabajo del programa o suba el tiempo límite en Opciones."],
      [/no declarad|undeclared|not declared/, "Declare el identificador antes de usarlo o revise que el nombre esté bien escrito."],
      [/no se puede asignar|incompatible|convert|cannot assign/, "Los tipos no coinciden: ajuste el tipo de la variable o convierta el valor de forma explícita."],
      [/const/, "El valor es const: quite la modificación o declare el objeto sin const."],
      [/se esperaba|esperado|expected|unexpected|inesperad/, "Revise la sintaxis cerca de esa posición: puede faltar o sobrar un símbolo como ; ) o }."],
      [/constructor/, "Compruebe que exista un constructor con esos parámetros."],
      [/argument|par.metro/, "Compare el número y los tipos de argumentos con la declaración de la función."]
    ];
    for (var i = 0; i < rules.length; i++) if (rules[i][0].test(m)) return rules[i][1];
    if (d.phase === "lex") return "Hay un carácter o literal que el analizador léxico no reconoce.";
    return "";
  }

  /* Empareja las filas de un diff para mostrarlas lado a lado.
   * Devuelve filas {l, r, kind} o {gap}. kind: eq, chg, del, add. */
  function sideBySide(rows) {
    var out = [], dels = [], adds = [];
    function flush() {
      var n = Math.max(dels.length, adds.length);
      for (var i = 0; i < n; i++) {
        var l = i < dels.length ? dels[i] : null, r = i < adds.length ? adds[i] : null;
        out.push({ l: l, r: r, kind: l !== null && r !== null ? "chg" : l !== null ? "del" : "add" });
      }
      dels = []; adds = [];
    }
    (rows || []).forEach(function (row) {
      if (row.op === "-") dels.push(row.text);
      else if (row.op === "+") adds.push(row.text);
      else {
        flush();
        if (row.op === "~") out.push({ gap: row.text });
        else out.push({ l: row.text, r: row.text, kind: "eq" });
      }
    });
    flush();
    return out;
  }

  /* Reaccion de la mascota a una respuesta de la API.
   * ctx: { hadErrors, firstSuccess }. Devuelve { estado, festejo }. */
  function mascotReaction(resp, ctx) {
    if (!resp) return null;
    ctx = ctx || {};
    var errs = (resp.errors || []).length, warns = (resp.warnings || []).length;
    var exit = describeExit(resp);
    if (exit && (exit.kind === "timeout" || exit.kind === "signal")) return { estado: "asustada", festejo: false };
    if (resp.stdout_truncated || resp.stderr_truncated || resp.truncated) return { estado: "asustada", festejo: false };
    if (errs >= 5) return { estado: "enojada", festejo: false };
    if (errs > 0 || resp.success === false) return { estado: "error", festejo: false };
    if (warns > 0) return { estado: "sospecha", festejo: false };
    if (exit && exit.kind === "nonzero") return { estado: "sospecha", festejo: false };
    return { estado: "feliz", festejo: !!(ctx.hadErrors || ctx.firstSuccess) };
  }

  return {
    PHASES: PHASES, astCategory: astCategory, initialCollapsed: initialCollapsed, revealPath: revealPath, layoutTree: layoutTree, suggestFor: suggestFor, sideBySide: sideBySide, mascotReaction: mascotReaction, escapeHtml: escapeHtml, phaseLabel: phaseLabel, formatCode: formatCode,
    tokenizeAsmLine: tokenizeAsmLine, extractStrings: extractStrings, diffLines: diffLines,
    summarizeDiff: summarizeDiff, normalizeNode: normalizeNode, searchTree: searchTree,
    countNodes: countNodes, attrChips: attrChips, filterTokens: filterTokens,
    truncateLines: truncateLines, countLines: countLines, phaseStatuses: phaseStatuses,
    describeExit: describeExit, tokenEndColumn: tokenEndColumn, errorReport: errorReport,
    formatMs: formatMs
  };
});
