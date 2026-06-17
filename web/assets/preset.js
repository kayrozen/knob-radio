/* Préset — language toggle + restrained scroll reveal */
(function () {
  'use strict';

  // ---- Language toggle (FR / EN) ----
  var STORE = 'preset-lang';
  var root = document.documentElement;
  var buttons = Array.prototype.slice.call(document.querySelectorAll('[data-setlang]'));

  function setLang(lang) {
    if (lang !== 'fr' && lang !== 'en') lang = 'fr';
    root.setAttribute('data-lang', lang);
    root.setAttribute('lang', lang);
    buttons.forEach(function (b) {
      b.setAttribute('aria-pressed', String(b.getAttribute('data-setlang') === lang));
    });
    try { localStorage.setItem(STORE, lang); } catch (e) {}
    try { window.dispatchEvent(new CustomEvent('preset:langchange', { detail: { lang: lang } })); } catch (e) {}
  }

  buttons.forEach(function (b) {
    b.addEventListener('click', function () { setLang(b.getAttribute('data-setlang')); });
  });

  var saved;
  try { saved = localStorage.getItem(STORE); } catch (e) {}
  // FR-priority audience (Québec); default FR unless previously chosen.
  setLang(saved || 'fr');
})();
