/* Aplica el tema guardado antes de pintar para evitar parpadeo. */
(function () {
  try {
    var raw = localStorage.getItem("mcpp.v1");
    var theme = raw ? JSON.parse(raw).theme : "auto";
    if (theme === "light" || theme === "dark") document.documentElement.setAttribute("data-theme", theme);
  } catch (e) { /* sin almacenamiento */ }
})();
