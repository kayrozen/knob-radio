/* =========================================================================
   Préset — captive-portal logic (talks to the device).
   WiFi page  : GET /api/scan       -> networks; POST /api/wifi {ssid,pass}
   Bluetooth  : GET /api/bt/scan    -> speakers; POST /api/bt   {mac}
   Shared scan/select/submit/success flow mirrors the design. preset.js handles
   the FR/EN toggle.
   ========================================================================= */
(function () {
  'use strict';

  var page = document.querySelector('.portal-page');
  if (!page) return;
  var kind = page.getAttribute('data-portal');           // 'wifi' | 'bt'

  var scanBtn   = document.querySelector('[data-scan]');
  var scanMeta  = document.querySelector('[data-scan-meta]');
  var list      = document.querySelector('[data-list]');
  var pass      = document.querySelector('.pp-pass');
  var passInput = pass ? pass.querySelector('input') : null;
  var eye       = document.querySelector('.pp-eye');
  var submit    = document.querySelector('[data-submit]');
  var selected  = null;

  var ICON = {
    wifi: '<svg width="17" height="17" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><path d="M2 6a9 9 0 0 1 12 0"/><path d="M4.3 8.4a5.6 5.6 0 0 1 7.4 0"/><path d="M6.6 10.8a2.2 2.2 0 0 1 2.8 0"/><circle cx="8" cy="13" r="0.7" fill="currentColor" stroke="none"/></svg>',
    bt: '<svg width="15" height="17" viewBox="0 0 12 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"><path d="M3 4.5 9 11l-3 2.5V2.5L9 5 3 11.5"/></svg>',
    lock: '<svg width="11" height="11" viewBox="0 0 14 14" fill="none" stroke="currentColor" stroke-width="1.5"><rect x="3" y="6.2" width="8" height="5.5" rx="1"/><path d="M4.6 6.2V4.4a2.4 2.4 0 0 1 4.8 0v1.8"/></svg>'
  };

  function esc(s) {
    return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
    });
  }
  function bars(rssi) {                                   // dBm -> 1..4
    if (rssi >= -55) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    return 1;
  }
  function sigBars(lvl) {
    return '<span class="sig" data-level="' + lvl + '"><i></i><i></i><i></i><i></i></span>';
  }

  function rowHTML(d, i) {
    var lead = kind === 'wifi' ? ICON.wifi : ICON.bt;
    var locked = kind === 'wifi' && d.auth;
    var lock = locked ? '<span class="lock" aria-hidden="true">' + ICON.lock + '</span>' : '';
    var attrs = kind === 'wifi'
      ? ' data-ssid="' + esc(d.ssid) + '" data-lock="' + (locked ? 1 : 0) + '"'
      : ' data-mac="' + esc(d.mac) + '"';
    var nm   = kind === 'wifi' ? d.ssid : (d.name || d.mac);
    var meta = kind === 'wifi'
      ? (d.auth ? 'WPA' : 'Open') + ' · ' + d.rssi + ' dBm'
      : 'A2DP · ' + d.rssi + ' dBm';
    return '<button type="button" class="pp-row" data-i="' + i + '"' + attrs + '>' +
        '<span class="lead-ico" aria-hidden="true">' + lead + '</span>' +
        '<span class="info"><span class="nm">' + esc(nm) + lock + '</span>' +
          '<span class="meta">' + esc(meta) + '</span></span>' +
        '<span class="tail">' + sigBars(bars(d.rssi)) +
          '<span class="pp-radio" aria-hidden="true"></span></span>' +
      '</button>';
  }

  function setMeta(fr, en, done) {
    if (!scanMeta) return;
    scanMeta.classList.toggle('done', !!done);
    scanMeta.innerHTML = '<span class="pulse"></span><span data-fr>' + esc(fr) +
      '</span><span data-en>' + esc(en) + '</span>';
  }

  function scan() {
    selected = null;
    if (submit) submit.disabled = true;
    if (pass) pass.classList.remove('show');
    if (scanBtn) scanBtn.setAttribute('aria-busy', 'true');
    setMeta(kind === 'wifi' ? 'Recherche…' : 'Scan…', 'Searching…', false);
    list.innerHTML = '';
    for (var s = 0; s < (kind === 'wifi' ? 4 : 3); s++) {
      var sk = document.createElement('div'); sk.className = 'pp-skel'; list.appendChild(sk);
    }
    var url = kind === 'wifi' ? '/api/scan' : '/api/bt/scan';
    fetch(url).then(function (r) { return r.json(); }).then(function (j) {
      var data = (kind === 'wifi' ? j.networks : j.devices) || [];
      list.innerHTML = data.length ? data.map(rowHTML).join('')
        : '<div class="pp-empty"><span data-fr>Rien trouvé — Rescanner.</span>' +
          '<span data-en>Nothing found — Rescan.</span></div>';
      if (scanBtn) scanBtn.setAttribute('aria-busy', 'false');
      var unit = kind === 'wifi' ? ['réseaux', 'networks'] : ['enceintes', 'speakers'];
      setMeta(data.length + ' ' + unit[0], data.length + ' ' + unit[1], true);
    }).catch(function () {
      list.innerHTML = '<div class="pp-empty">—</div>';
      if (scanBtn) scanBtn.setAttribute('aria-busy', 'false');
      setMeta('Erreur', 'Error', true);
    });
  }

  if (list) list.addEventListener('click', function (e) {
    var row = e.target.closest('.pp-row');
    if (!row) return;
    list.querySelectorAll('.pp-row').forEach(function (r) { r.classList.remove('is-active'); });
    row.classList.add('is-active');
    selected = row;
    if (kind === 'wifi' && pass) {
      var locked = row.getAttribute('data-lock') === '1';
      pass.classList.toggle('show', locked);
      if (locked) setTimeout(function () { passInput && passInput.focus(); }, 120);
      else if (passInput) passInput.value = '';
    }
    if (submit) submit.disabled = false;
  });

  if (eye && passInput) eye.addEventListener('click', function () {
    var on = eye.classList.toggle('is-on');
    passInput.type = on ? 'text' : 'password';
  });

  function finish(name) {
    document.querySelectorAll('[data-picked]').forEach(function (el) { el.textContent = name; });
    page.classList.add('is-done');
    var st = document.querySelector('.pp-status'); if (st) st.classList.add('is-connected');
    var shell = document.querySelector('.portal-shell'); if (shell) shell.scrollTop = 0;
    window.scrollTo(0, 0);
  }

  if (submit) submit.addEventListener('click', function () {
    if (!selected) { void submit.offsetWidth; submit.classList.add('shake'); return; }
    submit.setAttribute('aria-busy', 'true');
    submit.disabled = true;
    var name, body, url;
    if (kind === 'wifi') {
      name = selected.getAttribute('data-ssid');
      url = '/api/wifi';
      body = 'ssid=' + encodeURIComponent(name) +
             '&pass=' + encodeURIComponent(passInput ? passInput.value : '');
    } else {
      name = selected.querySelector('.nm').textContent.trim();
      url = '/api/bt';
      body = 'mac=' + encodeURIComponent(selected.getAttribute('data-mac'));
    }
    fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: body })
      .then(function (r) {
        if (!r.ok) throw new Error('rejected');
        finish(name);
      })
      .catch(function () {
        submit.setAttribute('aria-busy', 'false');
        submit.disabled = false;
        void submit.offsetWidth; submit.classList.add('shake');
      });
  });

  document.querySelectorAll('[data-restart]').forEach(function (el) {
    el.addEventListener('click', function (e) {
      e.preventDefault();
      page.classList.remove('is-done');
      var st = document.querySelector('.pp-status'); if (st) st.classList.remove('is-connected');
      if (submit) submit.removeAttribute('aria-busy');
      scan();
    });
  });

  scan();
})();
