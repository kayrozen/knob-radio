/* ═══════════════════════════════════════════════════════════════
   Préset — Installation logic (ESP web installer).
   Functional core is UNCHANGED from the original CarRadio installer;
   user-facing strings now route through T() for FR/EN, and dynamic
   labels re-render on the 'preset:langchange' event.
═══════════════════════════════════════════════════════════════ */

/* ----------------------------- i18n ----------------------------- */
function lang() { return document.documentElement.getAttribute('data-lang') === 'en' ? 'en' : 'fr'; }
const I18N = {
  fr: {
    searching: 'Recherche…',
    searchFailed: 'Échec de la recherche : ',
    noResults: 'Aucun résultat',
    loadingSugg: 'Chargement des suggestions…',
    showingNear: (flag, cc) => 'Stations populaires en ' + flag + ' ' + cc,
    showingCurated: 'Stations internationales sélectionnées',
    suggNear: 'Suggestions près de toi',
    curated: 'Stations internationales sélectionnées',
    needBoth: 'Entre un nom et une URL.',
    noFeed: 'Ce podcast n\u2019a pas de flux RSS public \u2014 impossible \u00e0 ajouter.',
    badUrl: 'L\u2019URL doit commencer par http:// ou https://',
    namePlaceholder: 'preset',
    searchPlaceholder: 'Tape pour chercher une station…',
    sending: 'Envoi des données…',
    provisioned: '\u2713 Préset configuré avec succès\u202F!',
    rejected: 'L\u2019appareil a refusé la configuration : ',
    timedOut: 'Délai dépassé en attendant l\u2019appareil.',
    flashDone: 'Flash terminé — envoi de la configuration…',
    serialErr: 'Erreur série : ',
    flashFailed: 'Échec du flash. Vérifie la connexion et réessaie.',
    invalidName: 'Nom d\u2019appareil invalide. Vérifie l\u2019étape 1.',
    emptyPl: 'La liste est vide. Ajoute au moins une station à l\u2019étape 2.',
    fixBefore: ' (à corriger avant la fin du flash)',
    schedule: 'Horaire', onStart: 'au démarrage',
    manualOnly: 'Manuel seulement', onDemand: 'À la demande', autoSchedule: 'Horaire automatique',
    kindLive: 'En direct', kindPod: 'Podcast',
    from: 'Début', to: 'Fin',
    days: ['Lun', 'Mar', 'Mer', 'Jeu', 'Ven', 'Sam', 'Dim'],
    everyDay: 'Tous les jours', weekdays: 'Lun–Ven', weekend: 'Sam–Dim', noDays: 'Aucun jour',
    liveUrlHint: 'URL du flux audio direct (MP3, AAC, OGG).',
    podUrlHint: 'Adresse du flux RSS — Préset joue le dernier épisode.',
    livePlaceholder: 'http://stream.example.com/radio',
    podPlaceholder: 'https://exemple.com/podcast.xml',
    renameAria: 'Nom du préset', schedAria: 'Régler l’horaire', removeAria: 'Retirer',
    searchRadioPh: 'Cherche une radio en direct…', searchPodPh: 'Cherche un podcast…',
    searchLabelRadio: 'Chercher une station', searchLabelPod: 'Chercher un podcast',
    modeRadio: 'Radio en direct', modePod: 'Podcast',
    suggPodNear: 'Podcasts populaires', podCurated: 'Podcasts à découvrir',
    quick: 'Raccourcis',
    qCommute: 'Retour', qLunch: 'Midi', qMorning: 'Matin', qWeekend: 'Week-end',
    suggFrench: 'Radios francophones', radioFrench: 'Une sélection francophone', podFrench: 'Podcasts francophones populaires'
  },
  en: {
    searching: 'Searching…',
    searchFailed: 'Search failed: ',
    noResults: 'No results',
    loadingSugg: 'Loading suggestions…',
    showingNear: (flag, cc) => 'Popular stations in ' + flag + ' ' + cc,
    showingCurated: 'Curated international stations',
    suggNear: 'Suggested stations near you',
    curated: 'Curated international stations',
    needBoth: 'Please enter both a name and a URL.',
    noFeed: 'This podcast has no public RSS feed \u2014 it can\u2019t be added.',
    badUrl: 'URL must start with http:// or https://',
    namePlaceholder: 'preset',
    searchPlaceholder: 'Type to search radio stations…',
    sending: 'Sending provisioning data…',
    provisioned: '\u2713 Device provisioned successfully!',
    rejected: 'Device rejected provisioning: ',
    timedOut: 'Timed out waiting for device acknowledgement.',
    flashDone: 'Flash complete — sending provisioning…',
    serialErr: 'Serial error: ',
    flashFailed: 'Flash failed. Check the connection and try again.',
    invalidName: 'Invalid device name. Check Step 1.',
    emptyPl: 'Playlist is empty. Add at least one station in Step 2.',
    fixBefore: ' (fix before flash completes)',
    schedule: 'Schedule', onStart: 'on startup',
    manualOnly: 'Manual only', onDemand: 'On demand', autoSchedule: 'Auto schedule',
    kindLive: 'Live', kindPod: 'Podcast',
    from: 'From', to: 'To',
    days: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
    everyDay: 'Every day', weekdays: 'Mon–Fri', weekend: 'Sat–Sun', noDays: 'No days',
    liveUrlHint: 'Direct audio stream URL (MP3, AAC, OGG).',
    podUrlHint: 'RSS feed address — Préset plays the latest episode.',
    livePlaceholder: 'http://stream.example.com/radio',
    podPlaceholder: 'https://example.com/podcast.xml',
    renameAria: 'Preset name', schedAria: 'Set schedule', removeAria: 'Remove',
    searchRadioPh: 'Search live radio…', searchPodPh: 'Search podcasts…',
    searchLabelRadio: 'Search stations', searchLabelPod: 'Search podcasts',
    modeRadio: 'Live radio', modePod: 'Podcast',
    suggPodNear: 'Popular podcasts', podCurated: 'Podcasts to explore',
    quick: 'Shortcuts',
    qCommute: 'Return', qLunch: 'Lunch', qMorning: 'Morning', qWeekend: 'Weekend',
    suggFrench: 'French-language radio', radioFrench: 'A French-language pick', podFrench: 'Popular French podcasts'
  }
};
function T(key) { return I18N[lang()][key]; }

/* ═══════════════════════════════════════════════════════════════
   Constants & state
═══════════════════════════════════════════════════════════════ */
const DEVICE_NAME_RE = /^[a-z0-9][a-z0-9-]{0,22}[a-z0-9]$/;
const PLAYLIST_MAX   = 5;
const RB_SERVERS_URL = 'https://all.api.radio-browser.info/json/servers';

let rbServer   = null;
let playlist   = [];
let userCC     = null;          // ISO country code from Nominatim reverse-geocode
let searchMode = 'radio';       // 'radio' | 'podcast'
let geoDone    = false;         // true once initial geo/suggestion load has run

/* Quick schedule shortcuts (days = Mon..Sun) */
const QUICK_PRESETS = {
  commute: { days: [1,1,1,1,1,0,0], start: '15:30', end: '18:00' },
  lunch:   { days: [1,1,1,1,1,0,0], start: '12:00', end: '13:00' },
  morning: { days: [1,1,1,1,1,1,1], start: '06:30', end: '08:30' },
  weekend: { days: [0,0,0,0,0,1,1], start: '09:00', end: '11:00' },
};
const QUICK_ORDER = ['commute', 'lunch', 'morning', 'weekend'];
const QUICK_KEY = { commute: 'qCommute', lunch: 'qLunch', morning: 'qMorning', weekend: 'qWeekend' };
let dragSrcIdx = null;

// dynamic-label bookkeeping so we can re-translate on language switch
let geoState   = null;   // { kind:'loading'|'near'|'curated', flag, cc }
let suggLabelEl = null;   // injected suggestions <label>
let suggLabelKind = null; // 'near' | 'curated'

/* ═══════════════════════════════════════════════════════════════
   radio-browser.info server discovery
═══════════════════════════════════════════════════════════════ */
async function getRBServer() {
  if (rbServer) return rbServer;
  const cached = sessionStorage.getItem('rb_server');
  if (cached) { rbServer = cached; return rbServer; }
  try {
    const res = await fetch(RB_SERVERS_URL);
    const list = await res.json();
    for (let i = list.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      [list[i], list[j]] = [list[j], list[i]];
    }
    for (const srv of list) {
      const base = 'https://' + srv.name;
      try {
        const test = await fetch(base + '/json/config');
        if (test.ok) {
          rbServer = base;
          sessionStorage.setItem('rb_server', base);
          return rbServer;
        }
      } catch (_) {}
    }
  } catch (e) {
    console.warn('RB server discovery failed:', e);
  }
  rbServer = 'https://de1.api.radio-browser.info';
  return rbServer;
}

/* ═══════════════════════════════════════════════════════════════
   Geolocation → country code
═══════════════════════════════════════════════════════════════ */
const COUNTRY_BOXES = [
  [41.3, 51.1,  -5.1,   9.6, 'FR'],
  [49.9, 60.8,  -8.2,   2.0, 'GB'],
  [47.3, 55.1,   5.9,  15.0, 'DE'],
  [36.0, 47.1,   6.6,  18.5, 'IT'],
  [35.9, 44.0,  -9.5,   4.3, 'ES'],
  [24.4, 49.4, -125.0, -66.9, 'US'],
  [41.7, 83.1,  -141.0, -52.6, 'CA'],
  [-43.6, -10.0, 113.0, 153.6, 'AU'],
  [-34.8, -19.8, -73.6, -28.8, 'BR'],
  [29.5,  53.6,  34.3,   73.0, 'IN'],
  [29.4,  45.6,  25.1,   45.1, 'SA'],
  [-34.8, -22.4,  16.4,  32.9, 'ZA'],
  [35.2,  45.6,  25.8,   45.1, 'TR'],
  [49.0,  70.1,  19.1,   68.9, 'PL'],
  [37.0,  52.0,  14.1,   24.1, 'UA'],
  [45.5,  70.1,   4.4,   31.0, 'SE'],
  [54.6,  70.1,   4.1,   31.1, 'NO'],
  [54.6,  57.8,   8.1,   15.2, 'DK'],
  [59.8,  70.1,  20.6,   31.6, 'FI'],
  [46.4,  55.8,  16.8,   40.5, 'RO'],
  [47.7,  51.1,  12.1,   22.9, 'HU'],
  [48.5,  51.1,  12.1,   22.6, 'CZ'],
  [34.0,  42.3, 122.9,  153.9, 'JP'],
  [22.0,  41.5, 113.5,  134.8, 'CN'],
  [33.1,  38.6, 124.6,  132.0, 'KR'],
  [1.3,   71.3,  92.1,  141.0, 'ID'],
  [14.6,  20.5, -117.1,  -86.7, 'MX'],
  [19.3,  22.0,  -74.5,  -72.0, 'HT'],
  [40.3,  47.8,  36.0,   46.0, 'AZ'],
];
function latLngToCountry(lat, lng) {
  for (const [minLat, maxLat, minLng, maxLng, code] of COUNTRY_BOXES) {
    if (lat >= minLat && lat <= maxLat && lng >= minLng && lng <= maxLng) return code;
  }
  return null;
}

/* ═══════════════════════════════════════════════════════════════
   Fallback curated stations
═══════════════════════════════════════════════════════════════ */
/* Curated fallbacks — Francophone on the FR page, international on EN.
   Used offline or when radio-browser / Apple charts are unreachable. */
const FALLBACK_STATIONS_FR = [
  { name: 'FIP',            url: 'https://icecast.radiofrance.fr/fip-hifi.aac',          codec: 'AAC', bitrate: 192, stationuuid: null, source: 'custom_url', country: '🇫🇷' },
  { name: 'France Inter',   url: 'https://icecast.radiofrance.fr/franceinter-hifi.aac',  codec: 'AAC', bitrate: 192, stationuuid: null, source: 'custom_url', country: '🇫🇷' },
  { name: 'France Culture', url: 'https://icecast.radiofrance.fr/franceculture-hifi.aac', codec: 'AAC', bitrate: 192, stationuuid: null, source: 'custom_url', country: '🇫🇷' },
  { name: 'France Musique', url: 'https://icecast.radiofrance.fr/francemusique-hifi.aac', codec: 'AAC', bitrate: 192, stationuuid: null, source: 'custom_url', country: '🇫🇷' },
  { name: 'Ici Musique',    url: 'https://rcavliveaudio.akamaized.net/hls/live/2006980/M-6A_MTL/master.m3u8', codec: 'AAC', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇨🇦' },
  { name: 'CKOI 96.9',      url: 'https://playerservices.streamtheworld.com/api/livestream-redirect/CKOIFMAAC.aac', codec: 'AAC', bitrate: 96, stationuuid: null, source: 'custom_url', country: '🇨🇦' },
  { name: 'Couleur 3',      url: 'https://stream.srg-ssr.ch/m/couleur3/mp3_128',         codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇨🇭' },
  { name: 'Radio Nova',     url: 'https://broadcast.infomaniak.ch/radionova-high.mp3',   codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇫🇷' },
];
const FALLBACK_STATIONS_INTL = [
  { name: 'FIP',                  url: 'https://icecast.radiofrance.fr/fip-hifi.aac',          codec: 'AAC', bitrate: 192, stationuuid: null, source: 'custom_url', country: '🇫🇷' },
  { name: 'BBC Radio 6',          url: 'http://stream.live.vc.bbcmedia.co.uk/bbc_6music',      codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇬🇧' },
  { name: 'Radio Paradise Mellow',url: 'http://stream.radioparadise.com/mellow-192',           codec: 'MP3', bitrate: 192, stationuuid: null, source: 'custom_url', country: '🇺🇸' },
  { name: 'KEXP',                 url: 'http://live-mp3-128.kexp.org/',                        codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇺🇸' },
  { name: 'NTS Radio 1',          url: 'http://stream-relay-geo.ntslive.net/stream',           codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇬🇧' },
  { name: 'SomaFM Groove Salad',  url: 'http://ice2.somafm.com/groovesalad-128-mp3',           codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇺🇸' },
  { name: 'Radio Swiss Classic',  url: 'http://stream.srg-ssr.ch/rsc_de/mp3_128.m3u',          codec: 'MP3', bitrate: 128, stationuuid: null, source: 'custom_url', country: '🇨🇭' },
];
function curatedStations() { return lang() === 'fr' ? FALLBACK_STATIONS_FR : FALLBACK_STATIONS_INTL; }

const FALLBACK_PODCASTS_FR = [
  { name: 'Affaires sensibles',   url: 'https://radiofrance-podcast.net/podcast09/rss_13957.xml', author: 'France Inter' },
  { name: 'Les Pieds sur terre',  url: 'https://radiofrance-podcast.net/podcast09/rss_10193.xml', author: 'France Culture' },
  { name: 'Le Masque et la Plume',url: 'https://radiofrance-podcast.net/podcast09/rss_11136.xml', author: 'France Inter' },
  { name: 'Transfert',            url: 'https://feeds.audiomeans.fr/feed/4f6a3e4e-1f7a-4c5e-9d2a-transfert.xml', author: 'Slate.fr' },
];
const FALLBACK_PODCASTS_INTL = [
  { name: 'The Daily',        url: 'https://feeds.simplecast.com/54nAGcIl',  author: 'The New York Times' },
  { name: 'Radiolab',         url: 'https://feeds.simplecast.com/EmVW7VGp',  author: 'WNYC Studios' },
  { name: '99% Invisible',    url: 'https://feeds.simplecast.com/BqbsxVfO',  author: 'Roman Mars' },
  { name: 'This American Life',url: 'http://feeds.thisamericanlife.org/talpodcast', author: 'WBEZ Chicago' },
];
function curatedPodcasts() { return lang() === 'fr' ? FALLBACK_PODCASTS_FR : FALLBACK_PODCASTS_INTL; }

/* Countries whose Apple podcast charts are reliably French-charting.
   (CA is excluded — its charts are English-dominated; FR charts are used instead.) */
const FRENCH_CHART_CC = ['FR', 'BE', 'CH', 'MC', 'LU'];

/* ═══════════════════════════════════════════════════════════════
   Country flag from ISO code
═══════════════════════════════════════════════════════════════ */
function countryFlag(cc) {
  if (!cc || cc.length !== 2) return '🌐';
  const offset = 0x1F1E6 - 65;
  return String.fromCodePoint(cc.toUpperCase().charCodeAt(0) + offset)
       + String.fromCodePoint(cc.toUpperCase().charCodeAt(1) + offset);
}

/* ═══════════════════════════════════════════════════════════════
   Schedule helpers
═══════════════════════════════════════════════════════════════ */
function defaultSchedule() {
  return { manual: true, days: [true, true, true, true, true, false, false], start: '07:00', end: '09:00' };
}
function summarizeDays(sch, lng) {
  const D = I18N[lng], d = sch.days, n = d.filter(Boolean).length;
  if (n === 0) return D.noDays;
  if (n === 7) return D.everyDay;
  if (d[0] && d[1] && d[2] && d[3] && d[4] && !d[5] && !d[6]) return D.weekdays;
  if (!d[0] && !d[1] && !d[2] && !d[3] && !d[4] && d[5] && d[6]) return D.weekend;
  return d.map((on, i) => on ? D.days[i] : null).filter(Boolean).join(' ');
}
function summarize(sch, lng) {
  if (sch.manual) return I18N[lng].onDemand;
  const days = summarizeDays(sch, lng);
  if (sch.days.filter(Boolean).length === 0) return days;
  return days + ' · ' + sch.start + '\u2013' + sch.end;
}

/* ═══════════════════════════════════════════════════════════════
   Playlist rendering (rich preset cards w/ per-preset schedule)
═══════════════════════════════════════════════════════════════ */
function renderPlaylist() {
  const ul = document.getElementById('station-list');
  ul.innerHTML = '';
  playlist.forEach((s, i) => {
    if (!s.schedule) s.schedule = defaultSchedule();
    if (!s.kind) s.kind = s.source === 'podcast' ? 'podcast' : 'live';
    const li = document.createElement('li');
    li.className = 'preset-card';
    li.draggable = true;
    li.dataset.idx = i;
    const flag = s.kind === 'podcast' ? '' : (s.country || '');
    const artUrl = s.artwork && /^https?:\/\//i.test(s.artwork) ? s.artwork : '';
    const fallbackGlyph = s.kind === 'podcast' ? '◉' : (s.country || '🌐');
    const artHtml = artUrl
      ? '<span class="pc-art"><img src="' + escHtml(artUrl) + '" alt="" loading="lazy" decoding="async" /></span>'
      : '<span class="pc-art pc-art--fallback">' + fallbackGlyph + '</span>';
    const kindLabel = s.kind === 'podcast' ? T('kindPod') : T('kindLive');
    const meta = s.kind === 'podcast'
      ? [kindLabel, s.author].filter(Boolean).join(' · ')
      : [kindLabel, s.codec, s.bitrate ? s.bitrate + ' kbps' : null].filter(Boolean).join(' · ');
    const sch = s.schedule;
    const chips = [0, 1, 2, 3, 4, 5, 6].map(d =>
      '<label class="day"><input type="checkbox" data-day="' + d + '"' + (sch.days[d] ? ' checked' : '') +
      ' /><span class="d">' + T('days')[d] + '</span></label>').join('');
    const quicks = QUICK_ORDER.map(k =>
      '<button type="button" class="quick" data-q="' + k + '">' + T(QUICK_KEY[k]) + '</button>').join('');
    li.innerHTML =
      '<div class="pc-top">' +
        '<span class="drag-handle" aria-hidden="true">&#9776;</span>' +
        '<span class="pc-slot">' + (i + 1) + '</span>' +
        artHtml +
        '<div class="pc-id">' +
          '<input class="pc-label" type="text" value="' + escHtml(s.name) + '" aria-label="' + T('renameAria') + '" spellcheck="false" autocomplete="off" />' +
          '<div class="pc-meta">' + (flag ? flag + ' ' : '') + escHtml(meta) + '</div>' +
        '</div>' +
        '<button class="station-remove" type="button" aria-label="' + T('removeAria') + '" title="' + T('removeAria') + '">&#10005;</button>' +
      '</div>' +
      '<div class="pc-sched' + (sch.manual ? ' is-manual' : '') + '">' +
        '<button class="sched-toggle" type="button" aria-expanded="false" aria-label="' + T('schedAria') + '">' +
          '<span class="st-clock" aria-hidden="true">&#9201;</span>' +
          '<span class="st-summary"></span>' +
          '<span class="st-caret" aria-hidden="true">&#9662;</span>' +
        '</button>' +
        '<div class="sched-panel" hidden>' +
          '<label class="switch">' +
            '<input type="checkbox" class="manual-input"' + (sch.manual ? '' : ' checked') + ' />' +
            '<span class="track"><span class="knob"></span></span>' +
            '<span class="switch-txt">' + T('autoSchedule') + '</span>' +
          '</label>' +
          '<div class="sched-body">' +
            '<div class="quick-row">' +
              '<span class="quick-lab">' + T('quick') + '</span>' + quicks +
            '</div>' +
            '<div class="days">' + chips + '</div>' +
            '<div class="time-range">' +
              '<div class="time-cell"><span class="t-lab">' + T('from') + '</span><input type="time" class="t-start" value="' + sch.start + '" /></div>' +
              '<span class="time-arrow" aria-hidden="true">&#8594;</span>' +
              '<div class="time-cell"><span class="t-lab">' + T('to') + '</span><input type="time" class="t-end" value="' + sch.end + '" /></div>' +
            '</div>' +
          '</div>' +
        '</div>' +
      '</div>';

    const summaryEl = li.querySelector('.st-summary');
    const schedWrap = li.querySelector('.pc-sched');
    const refresh = () => {
      summaryEl.textContent = summarize(sch, lang());
      schedWrap.classList.toggle('is-manual', sch.manual);
    };
    refresh();

    const artImg = li.querySelector('.pc-art img');
    if (artImg) artImg.addEventListener('error', () => {
      const sp = document.createElement('span');
      sp.className = 'pc-art pc-art--fallback';
      sp.textContent = fallbackGlyph;
      artImg.closest('.pc-art').replaceWith(sp);
    });
    li.querySelector('.pc-label').addEventListener('input', e => { s.name = e.target.value; });
    li.querySelector('.station-remove').addEventListener('click', () => removeStation(i));
    const toggle = li.querySelector('.sched-toggle');
    const panel = li.querySelector('.sched-panel');
    toggle.addEventListener('click', () => {
      const open = panel.hidden;
      panel.hidden = !open;
      toggle.setAttribute('aria-expanded', String(open));
      toggle.classList.toggle('open', open);
    });
    li.querySelector('.manual-input').addEventListener('change', e => { sch.manual = !e.target.checked; refresh(); });
    li.querySelectorAll('.day input').forEach(inp =>
      inp.addEventListener('change', e => { sch.days[+e.target.dataset.day] = e.target.checked; refresh(); }));
    li.querySelector('.t-start').addEventListener('input', e => { sch.start = e.target.value; refresh(); });
    li.querySelector('.t-end').addEventListener('input', e => { sch.end = e.target.value; refresh(); });
    li.querySelectorAll('.quick').forEach(qb =>
      qb.addEventListener('click', () => {
        const p = QUICK_PRESETS[qb.dataset.q];
        sch.manual = false;
        sch.days = p.days.map(Boolean);
        sch.start = p.start; sch.end = p.end;
        li.querySelector('.manual-input').checked = true;
        li.querySelectorAll('.day input').forEach((inp, d) => { inp.checked = sch.days[d]; });
        li.querySelector('.t-start').value = sch.start;
        li.querySelector('.t-end').value = sch.end;
        if (panel.hidden) toggle.click();
        refresh();
      }));

    li.addEventListener('dragstart', onDragStart);
    li.addEventListener('dragover',  onDragOver);
    li.addEventListener('drop',      onDrop);
    li.addEventListener('dragend',   onDragEnd);
    ul.appendChild(li);
  });
  const counter = document.getElementById('pl-counter');
  counter.textContent = playlist.length + ' / ' + PLAYLIST_MAX;
  counter.classList.toggle('full', playlist.length >= PLAYLIST_MAX);
  document.getElementById('search-input').disabled = playlist.length >= PLAYLIST_MAX;
  const empty = document.getElementById('pl-empty');
  if (empty) empty.style.display = playlist.length ? 'none' : 'block';
}
function escHtml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
function removeStation(i) { playlist.splice(i, 1); renderPlaylist(); }

/* ═══════════════════════════════════════════════════════════════
   Drag-to-reorder
═══════════════════════════════════════════════════════════════ */
function onDragStart(e) {
  if (e.target.closest('input, button, .sched-panel')) { e.preventDefault(); return; }
  dragSrcIdx = +e.currentTarget.dataset.idx; e.dataTransfer.effectAllowed = 'move';
}
function onDragOver(e) {
  e.preventDefault();
  e.dataTransfer.dropEffect = 'move';
  document.querySelectorAll('#station-list li').forEach(el => el.classList.remove('drag-over'));
  e.currentTarget.classList.add('drag-over');
}
function onDrop(e) {
  e.preventDefault();
  const toIdx = +e.currentTarget.dataset.idx;
  if (dragSrcIdx !== null && dragSrcIdx !== toIdx) {
    const moved = playlist.splice(dragSrcIdx, 1)[0];
    playlist.splice(toIdx, 0, moved);
    renderPlaylist();
  }
}
function onDragEnd() {
  document.querySelectorAll('#station-list li').forEach(el => el.classList.remove('drag-over'));
  dragSrcIdx = null;
}

/* ═══════════════════════════════════════════════════════════════
   Add station
═══════════════════════════════════════════════════════════════ */
function addStation(s) {
  if (playlist.length >= PLAYLIST_MAX) return;
  if (playlist.some(p => p.url === s.url)) return;
  if (!s.kind) s.kind = s.source === 'podcast' ? 'podcast' : 'live';
  if (!s.schedule) s.schedule = defaultSchedule();
  playlist.push(s);
  renderPlaylist();
  if (s.stationuuid && rbServer) {
    fetch(rbServer + '/json/url/' + s.stationuuid).catch(() => {});
  }
}
function addCustomStation() {
  const name = document.getElementById('custom-name').value.trim();
  const url  = document.getElementById('custom-url').value.trim();
  const kind = (document.querySelector('input[name="custom-kind"]:checked') || {}).value || 'live';
  if (!name || !url) { alert(T('needBoth')); return; }
  if (!/^https?:\/\//.test(url)) { alert(T('badUrl')); return; }
  addStation({
    name, url, stationuuid: null, codec: null, bitrate: null,
    source: kind === 'podcast' ? 'podcast' : 'custom_url', kind, country: ''
  });
  document.getElementById('custom-name').value = '';
  document.getElementById('custom-url').value  = '';
  document.getElementById('custom-details').removeAttribute('open');
}
function updateCustomKind() {
  const kind = (document.querySelector('input[name="custom-kind"]:checked') || {}).value || 'live';
  const urlEl = document.getElementById('custom-url');
  const hintEl = document.getElementById('custom-url-hint');
  if (urlEl) urlEl.placeholder = kind === 'podcast' ? T('podPlaceholder') : T('livePlaceholder');
  if (hintEl) hintEl.textContent = kind === 'podcast' ? T('podUrlHint') : T('liveUrlHint');
}

/* ═══════════════════════════════════════════════════════════════
   Search
═══════════════════════════════════════════════════════════════ */
let searchTimer = null;
document.getElementById('search-input').addEventListener('input', function () {
  clearTimeout(searchTimer);
  const q = this.value.trim();
  if (!q) { hideSearchResults(); return; }
  searchTimer = setTimeout(() => (searchMode === 'podcast' ? doPodcastSearch(q) : doSearch(q)), 300);
});
document.addEventListener('click', function (e) {
  if (!document.getElementById('search-wrapper').contains(e.target)) hideSearchResults();
});
function hideSearchResults() {
  document.getElementById('search-results').hidden = true;
  document.getElementById('search-results').innerHTML = '';
}
async function doSearch(q) {
  const resultsEl = document.getElementById('search-results');
  resultsEl.innerHTML = '<div class="result-item"><span class="spinner"></span> ' + T('searching') + '</div>';
  resultsEl.hidden = false;
  try {
    const server = await getRBServer();
    const params = new URLSearchParams({ name: q, order: 'clickcount', reverse: 'true', limit: '20', hidebroken: 'true' });
    const res = await fetch(server + '/json/stations/search?' + params);
    const stations = await res.json();
    renderSearchResults(stations);
  } catch (e) {
    resultsEl.innerHTML = '<div class="result-item" style="color:var(--warn)">' + T('searchFailed') + escHtml(String(e)) + '</div>';
  }
}
function renderSearchResults(stations) {
  const el = document.getElementById('search-results');
  if (!stations.length) {
    el.innerHTML = '<div class="result-item" style="color:var(--fg-3)">' + T('noResults') + '</div>';
    return;
  }
  el.innerHTML = '';
  stations.slice(0, 20).forEach(s => {
    const url = s.url_resolved || s.url;
    if (!url) return;
    const flag = countryFlag(s.countrycode);
    const fav = s.favicon && /^https?:\/\//i.test(s.favicon) ? s.favicon : '';
    const meta = [s.codec, s.bitrate ? s.bitrate + ' kbps' : null].filter(Boolean).join(' · ');
    const div = document.createElement('div');
    div.className = 'result-item';
    div.innerHTML =
      (fav ? '<img class="result-art rb" src="' + escHtml(fav) + '" alt="" loading="lazy" />'
           : '<span class="result-flag">' + flag + '</span>') +
      '<div class="result-info">' +
        '<div class="result-name">' + escHtml(s.name) + '</div>' +
        (meta ? '<div class="result-meta">' + escHtml(meta) + '</div>' : '') +
      '</div>' +
      '<button class="result-add" disabled>+</button>';
    const fimg = div.querySelector('img.result-art');
    if (fimg) fimg.addEventListener('error', () => {
      const sp = document.createElement('span'); sp.className = 'result-flag'; sp.textContent = flag; fimg.replaceWith(sp);
    });
    const btn = div.querySelector('.result-add');
    btn.disabled = (playlist.length >= PLAYLIST_MAX);
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      addStation({
        name: s.name, url: url, stationuuid: s.stationuuid || null,
        codec: s.codec || null, bitrate: s.bitrate ? parseInt(s.bitrate) : null,
        source: 'radio_browser', country: flag, artwork: fav,
      });
      hideSearchResults();
      document.getElementById('search-input').value = '';
    });
    el.appendChild(div);
  });
}

/* ═══════════════════════════════════════════════════════════════
   Podcast Index API — resolve an Apple/iTunes id straight to its RSS feed.
   Docs: https://podcastindex-org.github.io/docs-api/#get-/podcasts/byitunesid
   Get a free key/secret at https://api.podcastindex.org/. NOTE: both values
   ship in this client and are visible to anyone, so use a key dedicated to
   this app. Leave them blank to skip Podcast Index and use the Apple lookup.
═══════════════════════════════════════════════════════════════ */
const PI_API_KEY    = '';   // ← your Podcast Index API key
const PI_API_SECRET = '';   // ← your Podcast Index API secret

async function piAuthHeaders() {
  const authDate = Math.floor(Date.now() / 1000).toString();
  const buf = await crypto.subtle.digest('SHA-1',
    new TextEncoder().encode(PI_API_KEY + PI_API_SECRET + authDate));
  const sig = Array.from(new Uint8Array(buf)).map(b => b.toString(16).padStart(2, '0')).join('');
  return { 'X-Auth-Date': authDate, 'X-Auth-Key': PI_API_KEY, 'Authorization': sig };
}

/* GET /podcasts/byitunesid → { feedUrl, artistName } or null */
async function piFeedByItunesId(id) {
  if (!PI_API_KEY || !PI_API_SECRET || !(window.crypto && window.crypto.subtle)) return null;
  try {
    const headers = await piAuthHeaders();
    const r = await fetch('https://api.podcastindex.org/api/1.0/podcasts/byitunesid?id=' +
                          encodeURIComponent(id), { headers });
    if (!r.ok) return null;
    const j = await r.json();
    const f = j && j.feed;
    // a hit returns feed as an object; a miss returns an empty array — guard for it.
    if (f && !Array.isArray(f) && f.url) return { feedUrl: f.url, artistName: f.author || '' };
  } catch (_) {}
  return null;
}

/* ═══════════════════════════════════════════════════════════════
   Podcast search (Apple Podcasts directory) + add
═══════════════════════════════════════════════════════════════ */
async function doPodcastSearch(q) {
  const el = document.getElementById('search-results');
  el.innerHTML = '<div class="result-item"><span class="spinner"></span> ' + T('searching') + '</div>';
  el.hidden = false;
  try {
    const params = new URLSearchParams({ media: 'podcast', entity: 'podcast', limit: '20', term: q });
    if (userCC) params.set('country', userCC);
    else if (lang() === 'fr') params.set('country', 'FR');
    const res = await fetch('https://itunes.apple.com/search?' + params);
    const j = await res.json();
    renderPodcastResults((j.results || []).filter(p => p.feedUrl));
  } catch (e) {
    el.innerHTML = '<div class="result-item" style="color:var(--warn)">' + T('searchFailed') + escHtml(String(e)) + '</div>';
  }
}
function renderPodcastResults(list) {
  const el = document.getElementById('search-results');
  if (!list.length) {
    el.innerHTML = '<div class="result-item" style="color:var(--fg-3)">' + T('noResults') + '</div>';
    return;
  }
  el.innerHTML = '';
  list.slice(0, 20).forEach(p => {
    const art = p.artworkUrl60 || p.artworkUrl100 || '';
    const div = document.createElement('div');
    div.className = 'result-item';
    div.innerHTML =
      (art ? '<img class="result-art" src="' + art + '" alt="" />' : '<span class="result-flag">◉</span>') +
      '<div class="result-info">' +
        '<div class="result-name">' + escHtml(p.collectionName || '') + '</div>' +
        (p.artistName ? '<div class="result-meta">' + escHtml(p.artistName) + '</div>' : '') +
      '</div>' +
      '<button class="result-add">+</button>';
    const btn = div.querySelector('.result-add');
    btn.disabled = (playlist.length >= PLAYLIST_MAX);
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      addPodcast({ name: p.collectionName, url: p.feedUrl, author: p.artistName, artwork: art });
      hideSearchResults();
      document.getElementById('search-input').value = '';
    });
    el.appendChild(div);
  });
}
function addPodcast(p) {
  if (!p.url) return;
  addStation({
    name: p.name, url: p.url, stationuuid: null, codec: null, bitrate: null,
    source: 'podcast', kind: 'podcast', country: '', author: p.author || '', artwork: p.artwork || '',
  });
}
/* resolve a feed URL from an Apple chart id, then add */
async function addPodcastById(id, name, art, btn, cc) {
  if (playlist.length >= PLAYLIST_MAX) return;
  if (btn) btn.disabled = true;
  // Charts are store-specific: a plain lookup hits the default (US) store and can
  // fail for a podcast that only charts elsewhere. Scope the lookup to the chart's
  // store, and fall back to a name search if the id lookup still comes up empty.
  const store = (cc || userCC || (lang() === 'fr' ? 'FR' : 'US')).toUpperCase();
  let feed = null, author = '';
  const grab = (res) => { if (res && res.feedUrl) { feed = res.feedUrl; author = res.artistName || author; } };

  // 1) Podcast Index — resolve the iTunes id directly to its RSS feed.
  grab(await piFeedByItunesId(id));

  // 2) Fallback: Apple lookup, scoped to the chart's store.
  if (!feed) {
    try {
      const r = await fetch('https://itunes.apple.com/lookup?id=' + id + '&entity=podcast&country=' + store);
      const j = await r.json();
      grab(j.results && j.results[0]);
    } catch (_) {}
  }
  // 3) Last resort: name search.
  if (!feed) {
    try {
      const params = new URLSearchParams({ media: 'podcast', entity: 'podcast', limit: '1', term: name, country: store });
      const r = await fetch('https://itunes.apple.com/search?' + params);
      const j = await r.json();
      grab((j.results || []).find(p => p.feedUrl));
    } catch (_) {}
  }
  if (btn) btn.disabled = (playlist.length >= PLAYLIST_MAX);
  if (!feed) { alert(T('noFeed')); return; }
  addPodcast({ name: name, url: feed, author: author, artwork: art });
}

/* search-mode toggle (radio / podcast) */
function setSearchMode(mode) {
  searchMode = mode === 'podcast' ? 'podcast' : 'radio';
  document.getElementById('sm-radio').toggleAttribute('checked', searchMode === 'radio');
  document.getElementById('sm-pod').toggleAttribute('checked', searchMode === 'podcast');
  const inp = document.getElementById('search-input');
  inp.value = '';
  inp.placeholder = searchMode === 'podcast' ? T('searchPodPh') : T('searchRadioPh');
  const lbl = document.getElementById('search-label');
  if (lbl) lbl.textContent = searchMode === 'podcast' ? T('searchLabelPod') : T('searchLabelRadio');
  hideSearchResults();
  setGeoStatus();
  renderSuggestions();
}
document.querySelectorAll('input[name="search-mode"]').forEach(r =>
  r.addEventListener('change', () => setSearchMode(r.value)));

/* ═══════════════════════════════════════════════════════════════
   Geolocation → country (Nominatim reverse-geocode) → suggestions
═══════════════════════════════════════════════════════════════ */
async function reverseGeocode(lat, lng) {
  try {
    const url = 'https://nominatim.openstreetmap.org/reverse?lat=' + lat + '&lon=' + lng +
                '&format=jsonv2&zoom=3&accept-language=en';
    const r = await fetch(url, { headers: { 'Accept': 'application/json' } });
    if (!r.ok) return null;
    const j = await r.json();
    const cc = j && j.address && j.address.country_code;
    return cc ? cc.toUpperCase() : null;
  } catch (_) { return null; }
}

function setGeoStatus() {
  const el = document.getElementById('geo-status');
  if (!geoState) { el.textContent = ''; return; }
  if (geoState.kind === 'loading') { el.innerHTML = '<span class="spinner"></span> ' + T('loadingSugg'); return; }
  if (geoState.kind === 'near') {
    el.textContent = searchMode === 'podcast'
      ? T('suggPodNear') + ' · ' + geoState.flag + ' ' + geoState.cc
      : (lang() === 'fr' ? T('suggFrench') + ' · ' + geoState.flag + ' ' + geoState.cc
                         : T('showingNear')(geoState.flag, geoState.cc));
  } else {
    if (lang() === 'fr') el.textContent = searchMode === 'podcast' ? T('podFrench') : T('suggFrench');
    else el.textContent = searchMode === 'podcast' ? T('podCurated') : T('showingCurated');
  }
}

async function loadLocalSuggestions() {
  geoState = { kind: 'loading' };
  setGeoStatus();
  let cc = null;
  if ('geolocation' in navigator) {
    try {
      const pos = await new Promise((res, rej) =>
        navigator.geolocation.getCurrentPosition(res, rej, { timeout: 7000 }));
      cc = await reverseGeocode(pos.coords.latitude, pos.coords.longitude);
      if (!cc) cc = latLngToCountry(pos.coords.latitude, pos.coords.longitude);
    } catch (_) {}
  }
  userCC = cc;
  geoState = cc ? { kind: 'near', flag: countryFlag(cc), cc: cc } : { kind: 'curated' };
  setGeoStatus();
  renderSuggestions();
  geoDone = true;
}

/* clear + (re)render the suggestion chips for the active search mode */
function clearSuggestions() {
  document.querySelectorAll('.sugg-block').forEach(n => n.remove());
  suggLabelEl = null; suggLabelKind = null;
}
function renderSuggestions() {
  clearSuggestions();
  const fr = lang() === 'fr';
  if (searchMode === 'podcast') {
    // FR page keeps geo (Canadian) charts; France charts only as a no-geo default
    if (userCC) showTopPodcasts(userCC, 'podNear');
    else if (fr) showTopPodcasts('FR', 'podFrench');
    else showFallbackPodcasts();
  } else {
    if (fr) showFrenchStations(userCC);                // French + geo (e.g. French-Canadian)
    else if (userCC) showGeoStations(userCC);
    else showFallbackStations();
  }
}
function suggWrapper(labelText, kind) {
  const wrapper = document.createElement('div');
  wrapper.className = 'sugg-block';
  wrapper.style.marginTop = '16px';
  const label = document.createElement('label');
  label.textContent = labelText;
  suggLabelEl = label; suggLabelKind = kind;
  wrapper.appendChild(label);
  const chipRow = document.createElement('div');
  chipRow.className = 'chip-row';
  wrapper.appendChild(chipRow);
  return { wrapper, chipRow };
}

/* build a station suggestion chip showing its radio-browser logo (favicon),
   falling back to the country flag when there's no usable logo */
function stationChip(s) {
  const url = s.url_resolved || s.url;
  if (!url) return null;
  const flag = countryFlag(s.countrycode);
  const fav = s.favicon && /^https?:\/\//i.test(s.favicon) ? s.favicon : '';
  const btn = document.createElement('button');
  btn.className = 'btn btn-secondary chip';
  btn.innerHTML = (fav
      ? '<img class="chip-art" src="' + escHtml(fav) + '" alt="" loading="lazy" />'
      : '<span class="chip-flag">' + flag + '</span>') +
    '<span>' + escHtml(s.name) + '</span>';
  const img = btn.querySelector('img');
  if (img) img.addEventListener('error', () => {
    const sp = document.createElement('span');
    sp.className = 'chip-flag'; sp.textContent = flag;
    img.replaceWith(sp);
  });
  btn.addEventListener('click', () => addStation({
    name: s.name, url: url, stationuuid: s.stationuuid || null,
    codec: s.codec || null, bitrate: s.bitrate ? parseInt(s.bitrate) : null,
    source: 'radio_browser', country: flag, artwork: fav,
  }));
  return btn;
}

async function showGeoStations(cc) {
  try {
    const server = await getRBServer();
    const params = new URLSearchParams({ countrycode: cc, order: 'clickcount', reverse: 'true', limit: '10', hidebroken: 'true' });
    const res = await fetch(server + '/json/stations/search?' + params);
    const stations = await res.json();
    if (!stations.length) { showFallbackStations(); return; }
    const { wrapper, chipRow } = suggWrapper(T('suggNear'), 'near');
    stations.slice(0, 8).forEach(s => {
      const btn = stationChip(s);
      if (btn) chipRow.appendChild(btn);
    });
    document.getElementById('search-wrapper').before(wrapper);
  } catch (e) {
    showFallbackStations();
  }
}
function showFallbackStations() {
  const { wrapper, chipRow } = suggWrapper(T('curated'), 'curated');
  curatedStations().forEach(s => {
    const btn = document.createElement('button');
    btn.className = 'btn btn-secondary chip';
    btn.textContent = (s.country || '') + ' ' + s.name;
    btn.addEventListener('click', () => addStation({ ...s }));
    chipRow.appendChild(btn);
  });
  document.getElementById('search-wrapper').before(wrapper);
}

/* French-language radio. With a country code, prefers French stations IN that
   country (e.g. French-Canadian), then tops up with French stations elsewhere. */
async function showFrenchStations(cc) {
  try {
    const server = await getRBServer();
    const base = { language: 'french', order: 'clickcount', reverse: 'true', hidebroken: 'true' };
    let stations = [];
    if (cc) {
      const local = await fetch(server + '/json/stations/search?' + new URLSearchParams({ ...base, countrycode: cc, limit: '8' }));
      stations = await local.json();
    }
    const seen = new Set(stations.map(s => s.stationuuid));
    if (stations.length < 8) {
      const more = await fetch(server + '/json/stations/search?' + new URLSearchParams({ ...base, limit: '14' }));
      (await more.json()).forEach(s => { if (!seen.has(s.stationuuid)) { stations.push(s); seen.add(s.stationuuid); } });
    }
    if (!stations.length) { showFallbackStations(); return; }
    const { wrapper, chipRow } = suggWrapper(T('suggFrench'), 'french');
    stations.slice(0, 8).forEach(s => {
      const btn = stationChip(s);
      if (btn) chipRow.appendChild(btn);
    });
    document.getElementById('search-wrapper').before(wrapper);
  } catch (e) {
    showFallbackStations();
  }
}

/* podcast geo suggestions — Apple top-podcasts charts for the country */
async function showTopPodcasts(cc, labelKind) {
  try {
    const res = await fetch('https://itunes.apple.com/' + cc.toLowerCase() + '/rss/toppodcasts/limit=10/json');
    const j = await res.json();
    const entries = (j.feed && j.feed.entry) || [];
    if (!entries.length) { showFallbackPodcasts(); return; }
    const kind = labelKind || 'podNear';
    const labelTxt = kind === 'podFrench' ? T('podFrench') : T('suggPodNear');
    const { wrapper, chipRow } = suggWrapper(labelTxt, kind);
    entries.slice(0, 8).forEach(e => {
      const name = e['im:name'] && e['im:name'].label;
      const id = e.id && e.id.attributes && e.id.attributes['im:id'];
      const art = (e['im:image'] && e['im:image'][0] && e['im:image'][0].label) || '';
      if (!name || !id) return;
      const btn = document.createElement('button');
      btn.className = 'btn btn-secondary chip';
      btn.innerHTML = (art ? '<img class="chip-art" src="' + art + '" alt="" />' : '') + '<span>' + escHtml(name) + '</span>';
      btn.addEventListener('click', () => addPodcastById(id, name, art, btn, cc));
      chipRow.appendChild(btn);
    });
    document.getElementById('search-wrapper').before(wrapper);
  } catch (e) {
    showFallbackPodcasts();
  }
}
function showFallbackPodcasts() {
  const { wrapper, chipRow } = suggWrapper(T('podCurated'), 'podCurated');
  curatedPodcasts().forEach(p => {
    const btn = document.createElement('button');
    btn.className = 'btn btn-secondary chip';
    btn.textContent = p.name;
    btn.addEventListener('click', () => addPodcast({ name: p.name, url: p.url, author: p.author, artwork: '' }));
    chipRow.appendChild(btn);
  });
  document.getElementById('search-wrapper').before(wrapper);
}

/* ═══════════════════════════════════════════════════════════════
   Device name validation
═══════════════════════════════════════════════════════════════ */
document.getElementById('device-name').addEventListener('input', function () {
  const valid = DEVICE_NAME_RE.test(this.value.trim());
  this.classList.toggle('error', this.value.length > 0 && !valid);
  document.getElementById('name-hint').classList.toggle('err', this.value.length > 0 && !valid);
});

/* ═══════════════════════════════════════════════════════════════
   ESP Web Tools serial handoff
═══════════════════════════════════════════════════════════════ */
/* The config object both transports share: the USB installer wraps it as a
   `PROVISION:<json>\n` serial line; the LAN editor POSTs it to /api/presets. */
function buildConfigObject() {
  const name = document.getElementById('device-name').value.trim();
  if (!DEVICE_NAME_RE.test(name)) throw new Error(T('invalidName'));
  if (playlist.length === 0) throw new Error(T('emptyPl'));
  const pl = playlist.map(s => ({
    name: s.name, url: s.url, stationuuid: s.stationuuid || null,
    codec: s.codec || null, bitrate: s.bitrate || null,
    source: s.source || 'custom_url', kind: s.kind || 'live',
    schedule: s.schedule && !s.schedule.manual
      ? { mode: 'auto', days: s.schedule.days.map(b => b ? 1 : 0), start: s.schedule.start, end: s.schedule.end }
      : { mode: 'manual' },
  }));
  let timezone = '';
  try { timezone = Intl.DateTimeFormat().resolvedOptions().timeZone || ''; } catch (_) {}
  return { provision_version: 1, device_name: name, timezone: timezone, playlist: pl };
}
function buildProvisionPayload() {
  return 'PROVISION:' + JSON.stringify(buildConfigObject()) + '\n';
}
async function sendProvisioning(port) {
  const statusEl = document.getElementById('install-status');
  statusEl.textContent = T('sending');
  statusEl.className = 'status-msg';
  const payload = buildProvisionPayload();
  const writer = port.writable.getWriter();
  await writer.write(new TextEncoder().encode(payload));
  writer.releaseLock();
  const reader = port.readable.getReader();
  let response = '';
  // One timeout that cancels the read; never race two reader.read() calls on the
  // same reader (that throws "Already reading").
  const timeoutId = setTimeout(() => { reader.cancel().catch(() => {}); }, 10000);
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value) {
        response += new TextDecoder().decode(value);
        if (response.includes('PROVISION:OK')) {
          statusEl.textContent = T('provisioned');
          statusEl.className = 'status-msg';
          return;
        }
        // Only act once the whole line has arrived (serial chunks arbitrarily);
        // the capture group drops the PROVISION:ERR: protocol prefix for the UI.
        var errMatch = response.match(/PROVISION:ERR:([^\n]*)\n/);
        if (errMatch) {
          throw new Error(T('rejected') + errMatch[1].trim());
        }
      }
    }
    throw new Error(T('timedOut'));   // reader.cancel() (timeout) ends the loop
  } finally {
    clearTimeout(timeoutId);
    reader.releaseLock();
  }
}
const installBtn = document.getElementById('install-btn');
const statusEl   = document.getElementById('install-status');
installBtn.addEventListener('state-changed', async (e) => {
  const state = e.detail?.state;
  if (!state) return;
  if (state === 'finished') {
    statusEl.textContent = T('flashDone');
    statusEl.className = 'status-msg';
    try { buildProvisionPayload(); } catch (err) {
      statusEl.textContent = '\u26A0 ' + err.message;
      statusEl.className = 'status-msg err';
      return;
    }
    let port;
    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });
      await sendProvisioning(port);
    } catch (err) {
      statusEl.textContent = T('serialErr') + err.message;
      statusEl.className = 'status-msg err';
    } finally {
      if (port && port.readable) { try { await port.close(); } catch (_) {} }
    }
  } else if (state === 'error') {
    statusEl.textContent = T('flashFailed');
    statusEl.className = 'status-msg err';
  } else if (state === 'initializing' || state === 'flashing') {
    try {
      buildProvisionPayload();
      statusEl.textContent = '';
      statusEl.className = 'status-msg';
    } catch (err) {
      statusEl.textContent = '\u26A0 ' + err.message + T('fixBefore');
      statusEl.className = 'status-msg err';
    }
  }
});

/* ═══════════════════════════════════════════════════════════════
   Language switch → re-translate dynamic strings
═══════════════════════════════════════════════════════════════ */
function applyLangStrings() {
  document.getElementById('device-name').placeholder = T('namePlaceholder');
  const inp = document.getElementById('search-input');
  inp.placeholder = searchMode === 'podcast' ? T('searchPodPh') : T('searchRadioPh');
  const lbl = document.getElementById('search-label');
  if (lbl) lbl.textContent = searchMode === 'podcast' ? T('searchLabelPod') : T('searchLabelRadio');
  setGeoStatus();
  if (suggLabelEl && suggLabelKind) {
    const map = { near: 'suggNear', curated: 'curated', podNear: 'suggPodNear', podCurated: 'podCurated', french: 'suggFrench', podFrench: 'podFrench' };
    suggLabelEl.textContent = T(map[suggLabelKind] || 'curated');
  }
  // suggestions are language-specific (francophone on FR) — refresh after a lang switch
  if (geoDone) renderSuggestions();
  updateCustomKind();
  renderPlaylist();
}
window.addEventListener('preset:langchange', applyLangStrings);
document.querySelectorAll('input[name="custom-kind"]').forEach(r => r.addEventListener('change', updateCustomKind));

/* ═══════════════════════════════════════════════════════════════
   Init
═══════════════════════════════════════════════════════════════ */
/* prefill a unique-ish default name: base "preset" + short random suffix, so a
   user with more than one device avoids a name collision (clearable back to plain). */
function randomNameSuffix() { return Math.random().toString(36).slice(2, 6); }
(function () {
  const el = document.getElementById('device-name');
  if (el && !el.value) el.value = 'preset-' + randomNameSuffix();
})();

applyLangStrings();
renderPlaylist();
loadLocalSuggestions();

/* ═══════════════════════════════════════════════════════════════
   Device mode — same page, served from the knob over the LAN (<name>.local).
   When GET /api/presets answers, this isn't the browser installer but the
   on-device editor: prefill the current name + presets, hide the USB-flashing
   UI, and save by POSTing the same config object back to /api/presets.
═══════════════════════════════════════════════════════════════ */
function enterDeviceMode(cfg) {
  // Prefill the device name and the five presets from the live config.
  const nameEl = document.getElementById('device-name');
  if (nameEl && cfg.device_name) nameEl.value = cfg.device_name;
  playlist = (cfg.playlist || []).slice(0, PLAYLIST_MAX).map(p => {
    const sc = p.schedule || {};
    let schedule;
    if (sc.mode === 'auto') {
      const days = (sc.days || []).map(b => !!b);
      while (days.length < 7) days.push(false);
      schedule = { manual: false, days: days, start: sc.start || '07:00', end: sc.end || '09:00' };
    } else {
      schedule = defaultSchedule();
    }
    return {
      name: p.name || '', url: p.url,
      kind: p.kind || 'live',
      source: p.kind === 'podcast' ? 'podcast' : 'custom_url',
      schedule: schedule,
    };
  });
  renderPlaylist();

  // Hide everything specific to first-time USB flashing.
  ['install-card', 'bridge-card'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.style.display = 'none';
  });
  const legend = document.querySelector('.led-legend');
  if (legend) legend.style.display = 'none';

  // A save card in place of the flashing step.
  const step2 = document.getElementById('step2-card');
  if (step2 && !document.getElementById('save-card')) {
    const card = document.createElement('div');
    card.className = 'card';
    card.id = 'save-card';
    card.innerHTML =
      '<div class="step-label"><span data-fr>Sur ton réseau</span><span data-en>On your network</span></div>' +
      '<h2><span data-fr>Enregistrer sur l’appareil</span><span data-en>Save to the device</span></h2>' +
      '<p><span data-fr>Tes présets sont envoyés directement au Préset par wifi. Il redémarre et joue la nouvelle sélection — aucun câble.</span>' +
      '<span data-en>Your presets go straight to the Préset over wifi. It restarts and plays the new selection — no cable.</span></p>' +
      '<button class="btn" id="save-device-btn"><span data-fr>Enregistrer les présets</span><span data-en>Save presets</span></button>' +
      '<div class="status-msg" id="save-status"></div>';
    step2.parentNode.insertBefore(card, step2.nextSibling);
    document.getElementById('save-device-btn').addEventListener('click', saveToDevice);
  }
}

function saveToDevice() {
  const statusEl = document.getElementById('save-status');
  const btn = document.getElementById('save-device-btn');
  let cfg;
  try {
    cfg = buildConfigObject();
  } catch (err) {
    statusEl.textContent = '⚠ ' + err.message;
    statusEl.className = 'status-msg err';
    return;
  }
  statusEl.textContent = T('sending');
  statusEl.className = 'status-msg';
  if (btn) btn.disabled = true;
  fetch('/api/presets', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(cfg),
  }).then(r => {
    if (!r.ok) throw new Error('rejected');
    statusEl.innerHTML = '<span data-fr>Enregistré — le Préset redémarre sur ta nouvelle sélection.</span>' +
      '<span data-en>Saved — the Préset is restarting on your new selection.</span>';
    statusEl.className = 'status-msg';
  }).catch(() => {
    statusEl.textContent = T('flashFailed');
    statusEl.className = 'status-msg err';
    if (btn) btn.disabled = false;
  });
}

(function probeDevice() {
  if (!/^https?:$/.test(location.protocol)) return;   // file:// preview — skip
  fetch('/api/presets', { cache: 'no-store' })
    .then(r => (r.ok ? r.json() : Promise.reject()))
    .then(cfg => { if (cfg && Array.isArray(cfg.playlist)) enterDeviceMode(cfg); })
    .catch(() => { /* served by the public installer (no device API) — leave as-is */ });
})();
