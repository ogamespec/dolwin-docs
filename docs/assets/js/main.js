/* ============================================================
   GameCube Hardware Specifications — shared behavior
   - Theme toggle (light/dark) with persistence
   - GameCube-style cube logo injection
   ============================================================ */
(function () {
  "use strict";

  /* ---- Theme ---- */
  var KEY = "gc-docs-theme";
  var root = document.documentElement;

  function apply(theme) {
    root.setAttribute("data-theme", theme);
    var toggles = document.querySelectorAll(".theme-switch");
    toggles.forEach(function (t) { t.checked = theme === "dark"; });
  }

  function current() {
    // Allow an explicit override via ?theme=light|dark
    try {
      var qp = new URLSearchParams(window.location.search).get("theme");
      if (qp === "light" || qp === "dark") return qp;
    } catch (e) {}
    var saved = null;
    try { saved = localStorage.getItem(KEY); } catch (e) {}
    if (saved === "light" || saved === "dark") return saved;
    if (window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
      return "dark";
    }
    return "light";
  }

  function initTheme() {
    apply(current());
    document.querySelectorAll(".theme-switch").forEach(function (t) {
      t.addEventListener("change", function () {
        var theme = t.checked ? "dark" : "light";
        apply(theme);
        try { localStorage.setItem(KEY, theme); } catch (e) {}
      });
    });
  }

  /* ---- GameCube-style cube logo ---- */
  function cubeSVG() {
    return [
      '<svg viewBox="0 0 100 92" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">',
      '<defs>',
      '<linearGradient id="gc-top" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#9a8bff"/><stop offset="1" stop-color="#7b6bff"/></linearGradient>',
      '<linearGradient id="gc-left" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#6b5ad0"/><stop offset="1" stop-color="#4b44a9"/></linearGradient>',
      '<linearGradient id="gc-right" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4b44a9"/><stop offset="1" stop-color="#332c7a"/></linearGradient>',
      "</defs>",
      '<polygon points="50,2 97,26 50,50 3,26" fill="url(#gc-top)"/>',
      '<polygon points="3,26 50,50 50,90 3,66" fill="url(#gc-left)"/>',
      '<polygon points="50,50 97,26 97,66 50,90" fill="url(#gc-right)"/>',
      '<polyline points="3,26 50,50 97,26" fill="none" stroke="#ffffff" stroke-opacity="0.35" stroke-width="1.5"/>',
      "</svg>"
    ].join("");
  }

  function injectLogo() {
    var svg = cubeSVG();
    document.querySelectorAll(".cube, .big-cube").forEach(function (el) {
      el.innerHTML = svg;
    });
    // icon slot containers
    document.querySelectorAll(".card-icon").forEach(function (el) {
      if (!el.textContent.trim()) {
        var glyph = el.getAttribute("data-glyph") || "◈";
        el.textContent = glyph;
      }
    });
  }

  /* ---- Boot ---- */
  function boot() {
    initTheme();
    injectLogo();
    // mark active nav link by location
    var path = window.location.pathname.split("/").pop() || "index.html";
    if (path === "") path = "index.html";
    document.querySelectorAll(".app-nav a").forEach(function (a) {
      var href = a.getAttribute("href");
      if (href === path) a.classList.add("active");
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
