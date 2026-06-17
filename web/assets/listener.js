/* Préset — listener page: five-preset buttons.
   On every reload, fill the five slots with a fresh random mix of stations
   and podcasts (visual only). Separate pools per language, since the media
   are language-specific. Names swap in on hover; type shows beneath. */
(function () {
  'use strict';

  // French-speaking stations & podcasts (FR side).
  var POOL_FR = [
    { name: 'ICI Musique',    type: 'station' },
    { name: 'CKOI 96,9',      type: 'station' },
    { name: 'Rythme FM',      type: 'station' },
    { name: '98,5 FM',        type: 'station' },
    { name: 'ICI Première',   type: 'station' },
    { name: 'Énergie 94,3',   type: 'station' },
    { name: 'Rouge FM',       type: 'station' },
    { name: 'WKND 99,5',      type: 'station' },
    { name: 'ICI Classique',  type: 'station' },
    { name: 'Sans Filtre',         type: 'podcast' },
    { name: 'Distorsion',          type: 'podcast' },
    { name: 'La Soirée est encore jeune', type: 'podcast' },
    { name: 'Mortel',              type: 'podcast' },
    { name: 'Les Pires Moments de l\u2019Histoire', type: 'podcast' },
    { name: 'Programme Double',    type: 'podcast' },
    { name: 'Trafic',              type: 'podcast' },
    { name: 'Le Sportnographe',    type: 'podcast' }
  ];

  // English-speaking stations & podcasts (EN side).
  var POOL_EN = [
    { name: 'CBC Radio One',  type: 'station' },
    { name: 'CBC Music',      type: 'station' },
    { name: 'Virgin Radio',   type: 'station' },
    { name: 'CHOM 97.7',      type: 'station' },
    { name: 'CJAD 800',       type: 'station' },
    { name: 'TSN 690',        type: 'station' },
    { name: 'The Beat 92.5',  type: 'station' },
    { name: 'Indie88',        type: 'station' },
    { name: 'Q107',           type: 'station' },
    { name: 'Front Burner',        type: 'podcast' },
    { name: 'The Current',         type: 'podcast' },
    { name: 'Canadaland',          type: 'podcast' },
    { name: 'Under the Influence', type: 'podcast' },
    { name: '99% Invisible',       type: 'podcast' },
    { name: 'Stuff You Should Know',type: 'podcast' },
    { name: 'This American Life',  type: 'podcast' },
    { name: 'The Daily',           type: 'podcast' }
  ];

  var TYPE_LABEL = {
    station: { fr: 'Station', en: 'Station' },
    podcast: { fr: 'Balado',  en: 'Podcast' }
  };

  function shuffle(arr) {
    var a = arr.slice();
    for (var i = a.length - 1; i > 0; i--) {
      var j = Math.floor(Math.random() * (i + 1));
      var t = a[i]; a[i] = a[j]; a[j] = t;
    }
    return a;
  }

  // Pick 5 with a guaranteed mix: at least one station and one podcast.
  function pickFive(pool) {
    var stations = shuffle(pool.filter(function (p) { return p.type === 'station'; }));
    var podcasts = shuffle(pool.filter(function (p) { return p.type === 'podcast'; }));
    var pick = [stations.shift(), podcasts.shift()];           // seed the mix
    var rest = shuffle(stations.concat(podcasts));
    while (pick.length < 5 && rest.length) { pick.push(rest.shift()); }
    return shuffle(pick).filter(Boolean);                      // randomize order
  }

  function currentLang() {
    return document.documentElement.getAttribute('data-lang') === 'en' ? 'en' : 'fr';
  }

  function render(btn, item, idx) {
    var nm = btn.querySelector('.nm');
    if (!nm || !item) return;
    var label = TYPE_LABEL[item.type] || TYPE_LABEL.station;
    nm.innerHTML =
      '<span class="nm-name">' + item.name + '</span>' +
      '<span class="nm-type"><span data-fr>' + label.fr + '</span>' +
      '<span data-en>' + label.en + '</span></span>';
    var pn = btn.querySelector('.pn');
    if (pn) pn.textContent = String(idx + 1);
  }

  var btns = Array.prototype.slice.call(document.querySelectorAll('.preset-btn'));

  // Pre-pick a stable set per language on load; swap on language toggle.
  var chosen = { fr: pickFive(POOL_FR), en: pickFive(POOL_EN) };

  function renderSet(lang) {
    var set = chosen[lang] || chosen.fr;
    btns.forEach(function (btn, i) { render(btn, set[i], i); });
  }

  renderSet(currentLang());

  // Re-render presets when the language changes.
  new MutationObserver(function () { renderSet(currentLang()); })
    .observe(document.documentElement, { attributes: true, attributeFilter: ['data-lang'] });

  // Click selects (visual only).
  btns.forEach(function (btn) {
    btn.addEventListener('click', function () {
      btns.forEach(function (b) { b.classList.remove('active'); });
      btn.classList.add('active');
    });
  });
})();
