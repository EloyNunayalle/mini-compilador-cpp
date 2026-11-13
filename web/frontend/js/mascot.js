/* Mascota Compi: personaje propio creado con bible-strong-avatar-lab (AGPL-3.0), definido en
 * web/mascota/compi.avatar.json y empaquetado en vendor/mascota.js (global Compi).
 * Aqui solo se gestionan estados y festejo. */
(function () {
  "use strict";

  var ESTADOS = ["reposo", "curiosa", "atenta", "sospecha", "enojada", "asustada", "cargando", "feliz", "error"];
  var SPARK_COLORS = ["#f97316", "#facc15", "#fb923c", "#4ade80", "#fde68a"];

  /* Crea una mascota dentro de host. Devuelve { poner, estado, destruir } o null. */
  function create(host, estado) {
    var cur = ESTADOS.indexOf(estado) >= 0 ? estado : "reposo";
    if (!window.Compi) return null;
    var m;
    try {
      m = window.Compi.crearMascota(host, { size: "100%", estado: cur, ariaLabel: "Compi, la mascota del compilador" });
    } catch (e) {
      return null;
    }
    return {
      poner: function (e) {
        if (ESTADOS.indexOf(e) < 0) return;
        cur = e;
        try { m.poner(e); } catch (err) { /* estado desconocido */ }
      },
      estado: function () { return cur; },
      destruir: function () {
        try { m.destruir(); } catch (err) { /* ya destruida */ }
        while (host.firstChild) host.removeChild(host.firstChild);
      }
    };
  }

  var host = null, dock = null, inst = null, enabled = true, current = "reposo";
  var backTimer = null, typingTimer = null, busy = false, focused = false;

  function reduced() {
    return !!(window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches);
  }

  function set(e) {
    current = e;
    if (inst) inst.poner(e);
  }

  function scheduleBack(ms) {
    clearTimeout(backTimer);
    backTimer = setTimeout(function () { if (!busy) set(focused ? "curiosa" : "reposo"); }, ms);
  }

  function celebrate() {
    if (!dock || reduced() || !enabled) return;
    var n = 14;
    for (var i = 0; i < n; i++) {
      var sp = document.createElement("span");
      sp.className = "spark";
      var ang = (Math.PI * 2 * i) / n + (i % 2 ? 0.2 : -0.2);
      var dist = 34 + (i % 3) * 14;
      sp.style.setProperty("--dx", Math.round(Math.cos(ang) * dist * 1.5) + "px");
      sp.style.setProperty("--dy", Math.round(Math.sin(ang) * dist - 10) + "px");
      sp.style.setProperty("--c", SPARK_COLORS[i % SPARK_COLORS.length]);
      sp.addEventListener("animationend", function (ev) { if (ev.target.parentNode) ev.target.parentNode.removeChild(ev.target); });
      dock.appendChild(sp);
    }
  }

  function applyEnabled() {
    if (dock) dock.classList.toggle("off", !enabled);
    if (enabled && !inst && host) inst = create(host, current);
    if (!enabled && inst) { inst.destruir(); inst = null; }
  }

  window.MCMascot = {
    create: create,
    init: function (opts) {
      host = opts.host; dock = opts.dock;
      enabled = opts.enabled !== false;
      applyEnabled();
    },
    enabled: function () { return enabled; },
    setEnabled: function (on) { enabled = !!on; applyEnabled(); },
    state: function () { return current; },
    /* Muestra un estado; con festejo lanza chispas. */
    show: function (estado, o) {
      o = o || {};
      busy = estado === "cargando";
      clearTimeout(typingTimer);
      set(estado);
      if (o.festejo) celebrate();
      if (!busy && estado !== "error" && estado !== "enojada") scheduleBack(o.hold || 7000);
    },
    typing: function () {
      if (busy) return;
      clearTimeout(backTimer);
      set("atenta");
      clearTimeout(typingTimer);
      typingTimer = setTimeout(function () { if (!busy) set(focused ? "curiosa" : "reposo"); }, 1400);
    },
    focus: function (on) {
      focused = !!on;
      if (busy) return;
      if (on) { if (current === "reposo") set("curiosa"); }
      else setTimeout(function () { if (!busy && !focused && (current === "curiosa" || current === "atenta")) set("reposo"); }, 300);
    },
    done: function () { busy = false; }
  };
})();
