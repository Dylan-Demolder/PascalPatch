// PascalPatch desktop app. Plain JS on Pascal UI; every action is an /api call to app/server.py.
'use strict';

const $ = (sel, root = document) => root.querySelector(sel);
function h(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else if (k === 'class') e.className = v;
    else if (k === 'html') e.innerHTML = v;
    else e.setAttribute(k, v === true ? '' : v);
  }
  for (const k of kids.flat()) if (k != null && k !== false) e.append(k instanceof Node ? k : document.createTextNode(String(k)));
  return e;
}
const api = {
  async get(p) { const r = await fetch(p); const j = await r.json(); if (!r.ok) throw new Error(j.error || r.statusText); return j; },
  async post(p, body = {}) {
    const r = await fetch(p, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
    const j = await r.json(); if (!r.ok) throw new Error(j.error || r.statusText); return j;
  },
};
function toast(msg, kind = '') {
  const t = h('div', { class: `pp-toast ${kind ? 'pp-toast--' + kind : ''}` }, msg);
  $('#toasts').append(t); setTimeout(() => t.remove(), kind === 'danger' ? 9000 : 4500);
}
async function run(fn, ok) {
  try { const r = await fn(); if (ok) toast(ok); return r; } catch (e) { toast(e.message, 'danger'); throw e; }
}
function tip(text) { $('#tip').textContent = text; }
const DEFAULT_TIP = 'Choose a profile and press Play.';
document.addEventListener('mouseover', (e) => { const t = e.target.closest('[data-tip]'); tip(t ? t.dataset.tip : DEFAULT_TIP); });

const PORTS = ['1', '2', '3', '4'];
const S = { status: null, profiles: [], selected: null, jobs: [] };
try { S.selected = localStorage.getItem('pp.profile'); } catch (e) { /* storage may be off */ }

function frame(tabs, ...body) {
  return h('section', { class: 'pp-frame has-tab' },
    h('div', { class: 'pp-frame-tab' }, tabs.map((t, i) => h('span', { class: i ? 'is-dim' : '' }, t))), ...body);
}
function pageHead(title, sub, ...actions) {
  return h('div', { class: 'pp-page-head' },
    h('div', {}, h('h1', { class: 'pp-h1' }, title), sub ? h('p', { class: 'pp-muted', style: 'margin:4px 0 0' }, sub) : null),
    h('span', { class: 'pp-spacer' }), ...actions);
}
function empty(title, text, action) { return h('div', { class: 'pp-empty' }, h('div', { class: 'pp-h3' }, title), h('p', { style: 'margin:0' }, text), action); }
function compatTag(c) {
  return h('span', { class: `pp-tag ${c === 'online-safe' ? 'pp-tag--ok' : c === 'invalid' ? 'pp-tag--danger' : ''}`,
    'data-tip': c === 'online-safe' ? 'Nothing here changes gameplay.' : 'Changes gameplay: PascalPatch keeps this profile offline.' }, c);
}
function fighterTags(p) {
  return h('div', { class: 'fighters' }, p.characters.slice(0, 8).map((c, i) =>
    h('span', { class: `pp-tag pp-port pp-port--${PORTS[i % 4]}`, 'data-tip': `${c.name}: ${c.install === 'new' ? 'new fighter' : 'replaces ' + c.slot}` }, c.name)),
    p.characters.length > 8 ? h('span', { class: 'pp-tag' }, `+${p.characters.length - 8}`) : null);
}

// ---------------------------------------------------------------- Play
async function pagePlay(root) {
  S.profiles = await api.get('/api/profiles');
  if (!S.profiles.find((p) => p.id === S.selected)) S.selected = S.profiles[0]?.id || null;
  const st = S.status || {};
  const pick = h('div', { class: 'profile-pick' });
  const drawPick = () => {
    pick.replaceChildren(...S.profiles.map((p) => h('div', {
      class: `pp-card is-interactive ${p.id === S.selected ? 'is-selected' : ''}`, tabindex: 0, role: 'button',
      'data-tip': p.description || `${p.name}: ${p.characters.length} fighters, ${p.mode} mode.`,
      onclick: () => { S.selected = p.id; try { localStorage.setItem('pp.profile', p.id); } catch (e) {} drawPick(); },
      onkeydown: (e) => { if (e.key === 'Enter' || e.key === ' ') e.currentTarget.click(); },
    }, h('div', { class: 'pp-card-head' }, h('div', { class: 'pp-card-title' }, p.name), h('span', { class: 'pp-spacer' }), compatTag(p.compatibility)),
      h('div', { class: 'pp-small pp-muted' }, `${p.id} · ${p.characters.length} fighter${p.characters.length === 1 ? '' : 's'} · ${p.built ? 'built ' + p.built.replace('T', ' ') : 'not built yet'}`),
      p.characters.length ? fighterTags(p) : null)));
  };
  drawPick();
  const play = h('button', { class: 'pp-btn pp-btn--primary play-btn', 'data-tip': 'Build this profile if it changed, then start Melee Unlocked with PascalPatch (offline, no Slippi).',
    disabled: !S.selected || !!st.problems?.length, onclick: async () => {
      const j = await run(() => api.post('/api/launch', { id: S.selected }), 'Starting the game…'); watchJob(j.id);
    } }, 'Play');
  const build = h('button', { class: 'pp-btn', disabled: !S.selected, 'data-tip': 'Build the profile\'s game files without starting the game.',
    onclick: async () => { const j = await run(() => api.post('/api/build', { id: S.selected }), 'Building…'); watchJob(j.id); } }, 'Build');
  const checks = h('div', { class: 'checks' },
    [[!!st.port, 'Melee Unlocked', st.port ? `${st.melee_unlocked ? 'v' + st.melee_unlocked + ' · ' : ''}${st.port}` : 'melee_port.exe not found'],
     [!!st.launcher, 'PascalPatch runtime', st.launcher ? 'built' : 'not built'],
     [true, 'Offline guard', 'network refused in game, Slippi login hidden'],
     [st.plugins > 0, 'Plugins', `${st.plugins || 0} available`]].map(([ok, name, text]) =>
      h('div', { class: `check ${ok ? 'ok' : 'bad'}` }, h('b', {}, ok ? '✓' : '✕'), h('div', {}, h('strong', {}, name), h('div', { class: 'pp-dim' }, text)))));
  root.append(
    h('div', { class: 'hero' },
      frame(['Play', 'Offline'],
        S.profiles.length ? h('div', { class: 'pp-stack' }, pick, h('div', { class: 'play-row' }, play, build,
          h('span', { class: 'pp-small pp-muted' }, 'In game, press ', h('span', { class: 'pp-kbd' }, 'F2'), ' for the PascalPatch overlay.')))
          : empty('No profiles yet', 'A profile says which characters and plugins go into your game.', h('a', { class: 'pp-btn pp-btn--primary', href: '#profiles' }, 'New profile'))),
      frame(['Status'], checks, st.problems?.length ? h('div', { class: 'pp-notice pp-notice--warn', style: 'margin-top:12px' }, h('div', {}, st.problems.join(' '))) : null)));
}

// ---------------------------------------------------------------- Profiles
async function pageProfiles(root) {
  S.profiles = await api.get('/api/profiles');
  const list = h('div', { class: 'pp-list' });
  const detail = h('div', { class: 'pp-stack' });
  let current = S.profiles.find((p) => p.id === S.selected) || S.profiles[0];
  const drawList = () => list.replaceChildren(...S.profiles.map((p) => h('div', {
    class: 'pp-list-row is-interactive', style: p === current ? 'background:var(--pp-raised);box-shadow:inset 3px 0 var(--pp-accent)' : '',
    onclick: () => { current = p; S.selected = p.id; drawList(); drawDetail(); },
  }, h('div', { class: 'pp-grow' }, h('strong', {}, p.name), h('div', { class: 'pp-small pp-dim' }, p.id)), compatTag(p.compatibility))));
  const drawDetail = async () => {
    if (!current) { detail.replaceChildren(empty('No profiles', 'Create one to start.')); return; }
    const p = current;
    const chars = p.characters.length ? h('table', { class: 'pp-table' },
      h('thead', {}, h('tr', {}, h('th', {}, 'Fighter'), h('th', {}, 'Built on'), h('th', {}, 'Added as'))),
      h('tbody', {}, p.characters.map((c, i) => h('tr', {}, h('td', {}, h('span', { class: `pp-tag pp-port pp-port--${PORTS[i % 4]}` }, c.name)), h('td', {}, c.slot), h('td', {}, c.install === 'new' ? 'new fighter' : 'replacement')))))
      : h('p', { class: 'pp-muted' }, 'No custom fighters. Make some in Character Studio and build its roster into this profile.');
    const mods = h('div', { class: 'pp-list' }, h('div', { class: 'pp-list-row pp-muted' }, 'Loading mods…'));
    detail.replaceChildren(
      frame([p.name, p.id],
        h('div', { class: 'pp-stack' },
          h('div', { class: 'pp-inline' }, compatTag(p.compatibility), h('span', { class: 'pp-tag' }, p.mode), p.unlock_all ? h('span', { class: 'pp-tag pp-tag--accent' }, 'unlock all') : null,
            h('span', { class: 'pp-small pp-dim' }, p.built ? `built ${p.built.replace('T', ' ')}` : 'not built')),
          p.description ? h('p', { class: 'pp-muted', style: 'margin:0' }, p.description) : null,
          h('div', { class: 'play-row' },
            h('button', { class: 'pp-btn pp-btn--primary', onclick: async () => { const j = await run(() => api.post('/api/launch', { id: p.id }), 'Starting the game…'); watchJob(j.id); } }, 'Play'),
            h('button', { class: 'pp-btn', onclick: async () => { const j = await run(() => api.post('/api/build', { id: p.id }), 'Building…'); watchJob(j.id); } }, 'Build'),
            h('span', { class: 'pp-spacer' }),
            h('button', { class: 'pp-btn pp-btn--ghost pp-btn--sm', 'data-tip': 'Delete this profile (its builds stay on disk).', onclick: () => confirmDialog(`Delete ${p.name}?`, 'The profile file is removed. Built games stay in the data folder.', 'Delete', async () => {
              await run(() => api.post('/api/profile/delete', { id: p.id }), 'Profile deleted'); route(); }) }, 'Delete')))),
      frame(['Fighters'], chars),
      frame(['Mods'], mods));
    try {
      const m = await api.get('/api/profile-mods?id=' + encodeURIComponent(p.id));
      mods.replaceChildren(...(m.catalog.length ? m.catalog.map((e) => {
        const on = m.enabled.includes(e.id);
        const sw = h('input', { type: 'checkbox', checked: on, disabled: e.status === 'planned', onchange: async (ev) => {
          try { await api.post('/api/profile-mod', { profile: p.id, mod: e.id, enabled: ev.target.checked }); toast(`${e.name} ${ev.target.checked ? 'on' : 'off'}`); }
          catch (err) { ev.target.checked = !ev.target.checked; toast(err.message, 'danger'); } } });
        return h('div', { class: 'pp-list-row', 'data-tip': e.description || e.name },
          h('div', { class: 'pp-grow' }, h('strong', {}, e.name), h('div', { class: 'pp-small pp-dim' }, `${e.category} · v${e.version}${e.status === 'planned' ? ' · planned' : ''}`)),
          e.online_safe ? null : h('span', { class: 'pp-tag' }, 'offline'),
          h('label', { class: 'pp-switch' }, sw, h('span', { class: 'pp-switch-track' })));
      }) : [h('div', { class: 'pp-list-row pp-muted' }, 'No mods in the catalog.')]));
    } catch (e) { mods.replaceChildren(h('div', { class: 'pp-list-row pp-muted' }, e.message)); }
  };
  root.append(pageHead('Profiles', 'A profile is one version of your game: its fighters, mods and plugins.',
    h('button', { class: 'pp-btn pp-btn--primary', onclick: newProfileDialog }, 'New profile')),
    h('div', { class: 'split' }, list, detail));
  drawList(); drawDetail();
}
function newProfileDialog() {
  const id = h('input', { class: 'pp-input', placeholder: 'my-profile', pattern: '[a-z0-9][a-z0-9._-]+' });
  const name = h('input', { class: 'pp-input', placeholder: 'My profile' });
  const iso = h('input', { class: 'pp-input', placeholder: 'C:\\path\\to\\melee.iso' });
  openDialog('New profile', [
    h('div', { class: 'pp-field' }, h('label', {}, 'Name'), name),
    h('div', { class: 'pp-field' }, h('label', {}, 'ID'), id, h('span', { class: 'pp-hint' }, 'Lowercase letters, digits and dashes.')),
    h('div', { class: 'pp-field' }, h('label', {}, 'Your Melee 1.02 ISO'), iso, h('span', { class: 'pp-hint' }, 'Your own NTSC 1.02 disc image. PascalPatch never changes it.'))],
    'Create', async () => { await run(() => api.post('/api/profile/create', { id: id.value.trim(), name: name.value.trim(), iso: iso.value.trim() }), 'Profile created'); route(); });
  name.addEventListener('input', () => { if (!id.dataset.touched) id.value = name.value.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, ''); });
  id.addEventListener('input', () => { id.dataset.touched = 1; });
}

// ---------------------------------------------------------------- Installed plugins
async function pagePlugins(root) {
  const data = await api.get('/api/plugins');
  const file = h('input', { type: 'file', accept: '.zip', hidden: true, onchange: async (e) => {
    const f = e.target.files[0]; if (!f) return;
    const b64 = await new Promise((res) => { const r = new FileReader(); r.onload = () => res(r.result.split(',')[1]); r.readAsDataURL(f); });
    await run(() => api.post('/api/plugins/upload', { data: b64 }), `${f.name} installed (from a file, not verified)`); route(); } });
  const row = (p, builtin) => h('div', { class: 'pp-list-row', 'data-tip': p.summary || p.name },
    h('div', { class: `plugin-icon ${builtin ? 'is-builtin' : ''}` }, (p.name || p.id)[0].toUpperCase()),
    h('div', { class: 'pp-grow' }, h('strong', {}, p.name || p.id), ' ', h('span', { class: 'pp-dim pp-small' }, `v${p.version}`),
      h('div', { class: 'pp-small pp-muted' }, p.summary)),
    builtin ? h('span', { class: 'pp-tag', 'data-tip': 'Built from this PascalPatch checkout; profiles turn it on when they need it.' }, 'built-in')
      : h('span', { class: `pp-tag ${p.source === 'file' ? '' : 'pp-tag--info'}`, 'data-tip': p.source === 'file' ? 'Installed from a file: not checked against the plugin site.' : 'Installed from the plugin site: signature and hash checked.' }, p.source === 'file' ? 'local' : 'verified'),
    builtin ? null : h('button', { class: 'pp-btn pp-btn--ghost pp-btn--sm', disabled: !p.settings_schema.length, onclick: () => settingsDialog(p) }, 'Settings'),
    builtin ? null : h('label', { class: 'pp-switch', 'data-tip': 'Load this plugin the next time you press Play.' },
      h('input', { type: 'checkbox', checked: p.enabled, onchange: async (e) => { await run(() => api.post('/api/plugins/enable', { id: p.id, enabled: e.target.checked }), `${p.name} ${e.target.checked ? 'on' : 'off'}`); } }),
      h('span', { class: 'pp-switch-track' })));
  root.append(pageHead('Installed plugins', 'Plugins run inside the game. Changes apply the next time you press Play; in game, F2 opens their settings.',
    h('a', { class: 'pp-btn pp-btn--primary', href: '#browse' }, 'Browse plugins'),
    h('button', { class: 'pp-btn', 'data-tip': 'Install a plugin ZIP you built yourself.', onclick: () => file.click() }, 'Install from file'), file),
    data.installed.length ? h('div', { class: 'pp-list' }, data.installed.map((p) => row(p, false)))
      : empty('No downloaded plugins', 'Find some on the plugin site.', h('a', { class: 'pp-btn pp-btn--primary', href: '#browse' }, 'Browse plugins')),
    h('h2', { class: 'pp-h2' }, 'Built in'),
    data.builtin.length ? h('div', { class: 'pp-list' }, data.builtin.map((p) => row(p, true))) : h('p', { class: 'pp-muted' }, 'The runtime and first-party plugins are not built yet.'));
}
function settingsDialog(p) {
  const values = { ...p.settings };
  let group = '';
  const fields = p.settings_schema.flatMap((s) => {
    const heading = s.group && s.group !== group ? h('div', { class: 'pp-field-group' }, s.group) : null;
    group = s.group || '';
    const label = s.label || s.key; let input;
    if (s.type === 'bool') input = h('label', { class: 'pp-switch' }, h('input', { type: 'checkbox', checked: !!values[s.key], onchange: (e) => { values[s.key] = e.target.checked; } }), h('span', { class: 'pp-switch-track' }), h('span', {}, label));
    else if (s.type === 'choice') input = h('select', { class: 'pp-select', onchange: (e) => { values[s.key] = e.target.value; } },
      s.options.map((o) => { const v = typeof o === 'object' ? o.value : o; return h('option', { value: v, selected: v === values[s.key] }, typeof o === 'object' ? o.label : o); }));
    else if (s.type === 'int' || s.type === 'float') input = h('input', { class: 'pp-input', type: 'number', min: s.min, max: s.max, step: s.step || (s.type === 'int' ? 1 : 0.05), value: values[s.key],
      oninput: (e) => { values[s.key] = s.type === 'int' ? parseInt(e.target.value, 10) : parseFloat(e.target.value); } });
    else input = h('input', { class: 'pp-input', value: values[s.key] ?? '', oninput: (e) => { values[s.key] = e.target.value; } });
    const field = h('div', { class: 'pp-field' }, s.type === 'bool' ? null : h('label', {}, label), input, s.help ? h('span', { class: 'pp-hint' }, s.help) : null);
    return heading ? [heading, field] : [field];
  });
  openDialog(`${p.name} settings`, fields, 'Save', async () => { await run(() => api.post('/api/plugins/settings', { id: p.id, values }), 'Settings saved'); route(); });
}

// ---------------------------------------------------------------- Browse
async function pageBrowse(root, focus) {   // focus: a plugin id from #browse/<id> (the plugin site links here)
  let data = await api.get('/api/store');
  if (focus && !data.plugins.some((p) => p.id === focus)) { try { data = await api.post('/api/store/refresh'); } catch (e) {} }
  const grid = h('div', { class: 'pp-cards' });
  const search = h('input', { class: 'pp-input', type: 'search', placeholder: 'Search plugins', oninput: () => draw() });
  const tagSel = h('select', { class: 'pp-select', style: 'max-width:180px', onchange: () => draw() });
  const note = h('div', {});
  const refresh = h('button', { class: 'pp-btn', 'data-tip': 'Download the plugin list from the plugin site (checked against PascalPatch\'s signing key).', onclick: async () => {
    refresh.disabled = true;
    try { data = await run(() => api.post('/api/store/refresh'), 'Plugin list updated'); draw(); } finally { refresh.disabled = false; } } }, 'Refresh');
  function draw() {
    const tags = [...new Set(data.plugins.flatMap((p) => p.tags))].sort();
    const cur = tagSel.value;
    tagSel.replaceChildren(h('option', { value: '' }, 'All tags'), ...tags.map((t) => h('option', { value: t, selected: t === cur }, t)));
    const q = search.value.trim().toLowerCase();
    const shown = data.plugins.filter((p) => (!q || `${p.name} ${p.summary} ${p.author} ${p.tags.join(' ')}`.toLowerCase().includes(q)) && (!tagSel.value || p.tags.includes(tagSel.value)));
    note.replaceChildren(data.fetched ? h('span', { class: 'pp-small pp-dim' }, `From ${data.site} · updated ${new Date(data.fetched * 1000).toLocaleString()}`) : null);
    if (!data.plugins.length) {
      grid.replaceChildren(empty('No plugin list yet', data.fetched ? 'The plugin site has no plugins yet.' : 'Press Refresh to download the list from the plugin site.', refresh.cloneNode(true)));
      grid.querySelector('button')?.addEventListener('click', () => refresh.click());
      return;
    }
    grid.replaceChildren(...shown.map((p) => h('div', { class: 'pp-card', 'data-tip': p.summary },
      h('div', { class: 'pp-card-head' }, h('div', { class: 'plugin-icon' }, p.name[0].toUpperCase()),
        h('div', { class: 'pp-grow', style: 'min-width:0' }, h('div', { class: 'pp-card-title' }, p.name), h('div', { class: 'pp-small pp-dim' }, `by ${p.author} · v${p.version}`))),
      h('p', { class: 'pp-muted pp-small', style: 'margin:0' }, p.summary),
      h('div', { class: 'pp-inline' }, p.tags.map((t) => h('span', { class: 'pp-tag' }, t)), p.compatibility === 'online-safe' ? null : h('span', { class: 'pp-tag', 'data-tip': 'Changes gameplay: only runs offline.' }, 'offline')),
      h('div', { class: 'pp-card-foot' },
        p.installed && !p.update ? h('span', { class: 'pp-tag pp-tag--ok' }, 'installed')
          : h('button', { class: 'pp-btn pp-btn--primary pp-btn--sm', onclick: async (e) => { e.target.disabled = true;
            try { data = await run(() => api.post('/api/store/install', { id: p.id }), `${p.name} ${p.update ? 'updated' : 'installed'}`); draw(); refreshStatus(); } catch (err) { e.target.disabled = false; } } }, p.update ? `Update to ${p.version}` : 'Install'),
        h('span', { class: 'pp-spacer' }),
        h('button', { class: 'pp-btn pp-btn--ghost pp-btn--sm', onclick: () => pluginInfo(p) }, 'Details')))));
    if (!shown.length) grid.replaceChildren(empty('Nothing matches', 'Try another search or tag.'));
  }
  root.append(pageHead('Browse plugins', 'Every download is checked against PascalPatch\'s signing key before it is installed.', refresh),
    h('div', { class: 'toolbar' }, search, tagSel, h('span', { class: 'pp-spacer' }), note), grid);
  const wanted = focus && data.plugins.find((p) => p.id === focus);
  if (wanted) search.value = wanted.name;
  draw();
  if (wanted) setTimeout(() => pluginInfo(wanted), 0);
}
function pluginInfo(p) {
  openDialog(p.name, [
    h('div', { class: 'pp-inline' }, h('span', { class: 'pp-tag pp-tag--accent' }, `v${p.version}`), h('span', { class: 'pp-tag' }, p.license), ...p.tags.map((t) => h('span', { class: 'pp-tag' }, t))),
    h('p', { class: 'desc', style: 'margin:0' }, p.description || p.summary),
    p.changelog?.length ? h('div', {}, h('div', { class: 'pp-label' }, 'Changes'), h('ul', { class: 'pp-small pp-muted' }, p.changelog.slice(0, 6).map((c) => h('li', {}, typeof c === 'string' ? c : `${c.version}: ${c.notes}`)))) : null,
    p.homepage ? h('a', { href: p.homepage, target: '_blank', rel: 'noopener' }, 'Plugin page') : null], null);
}

// ---------------------------------------------------------------- Activity
async function pageActivity(root) {
  const [jobs, logs] = await Promise.all([api.get('/api/jobs'), api.get('/api/logs')]);
  const view = h('pre', { class: 'logview' }, 'Pick a job or a log.');
  const show = async (loader) => { view.textContent = 'Loading…'; try { view.textContent = await loader() || '(empty)'; view.scrollTop = view.scrollHeight; } catch (e) { view.textContent = e.message; } };
  root.append(pageHead('Activity', 'Builds and launches from this session, and the logs the game wrote.'),
    h('div', { class: 'split' },
      h('div', { class: 'pp-stack' },
        h('div', { class: 'pp-label' }, 'This session'),
        jobs.length ? h('div', { class: 'pp-list' }, jobs.map((j) => h('div', { class: 'pp-list-row is-interactive', onclick: () => show(async () => (await api.get('/api/jobs/' + j.id)).lines.join('\n')) },
          h('div', { class: 'pp-grow' }, h('strong', {}, j.title), h('div', { class: 'pp-small pp-dim' }, j.tail || '…')),
          h('span', { class: `pp-tag ${j.running ? 'pp-tag--info' : j.exit === 0 ? 'pp-tag--ok' : 'pp-tag--danger'}` }, j.running ? 'running' : j.exit === 0 ? 'done' : `exit ${j.exit}`))))
          : h('p', { class: 'pp-muted pp-small' }, 'Nothing yet.'),
        h('div', { class: 'pp-label' }, 'Game logs'),
        logs.length ? h('div', { class: 'pp-list' }, logs.slice(0, 25).map((l) => h('div', { class: 'pp-list-row is-interactive', onclick: () => show(async () => (await api.get(`/api/log?profile=${encodeURIComponent(l.profile)}&name=${encodeURIComponent(l.name)}`)).text) },
          h('div', { class: 'pp-grow' }, h('strong', {}, l.profile), h('div', { class: 'pp-small pp-dim' }, `${l.name} · ${l.modified.replace('T', ' ')}`)))))
          : h('p', { class: 'pp-muted pp-small' }, 'No logs yet.')),
      view));
}

// ---------------------------------------------------------------- Settings
async function pageSettings(root) {
  const s = await api.get('/api/settings');
  const f = (key, label, hint, attrs = {}) => {
    const input = h('input', { class: 'pp-input', value: Array.isArray(s[key]) ? s[key].join(' ') : (s[key] || ''), ...attrs });
    input.dataset.key = key;
    return h('div', { class: 'pp-field' }, h('label', {}, label), input, hint ? h('span', { class: 'pp-hint' }, hint) : null);
  };
  const theme = h('select', { class: 'pp-select' }, ['dark', 'light'].map((t) => h('option', { value: t, selected: s.theme === t }, t === 'dark' ? 'Dark (Melee)' : 'Light')));
  const form = h('div', { class: 'pp-stack' },
    frame(['Game'], h('div', { class: 'pp-row-form' },
      f('port', 'melee_port.exe', 'Your Melee Unlocked build. PascalPatch starts it unmodified.', { placeholder: 'C:\\...\\melee_port.exe' }),
      f('port_cwd', 'Game folder', 'Where the game keeps its settings and saves (defaults to the exe\'s folder).'),
      f('port_args', 'Extra game options', 'Passed to melee_port.exe, separated by spaces.', { placeholder: '--fps unlocked' }))),
    frame(['Plugins'], h('div', { class: 'pp-row-form' }, f('site', 'Plugin site', 'Where Browse downloads the plugin list. Downloads are checked against PascalPatch\'s signing key whatever the site.'))),
    frame(['Character Studio'], h('div', { class: 'pp-row-form' }, f('studio_repo', 'Character Studio folder', 'The MeleeCharacterStudio folder, for the Character Studio button.'))),
    frame(['Look'], h('div', { class: 'pp-row-form' }, h('div', { class: 'pp-field' }, h('label', {}, 'Theme'), theme))),
    h('div', { class: 'play-row' }, h('button', { class: 'pp-btn pp-btn--primary', onclick: async () => {
      const body = { theme: theme.value };
      form.querySelectorAll('input[data-key]').forEach((i) => { body[i.dataset.key] = i.dataset.key === 'port_args' ? i.value.split(/\s+/).filter(Boolean) : i.value.trim(); });
      await run(() => api.post('/api/settings', body), 'Settings saved'); applyTheme(body.theme); refreshStatus();
    } }, 'Save'), h('span', { class: 'pp-small pp-dim' }, `Profiles: ${S.status?.root} · Data: ${S.status?.data}`)));
  root.append(pageHead('Settings'), form);
}

// ---------------------------------------------------------------- dialogs
function openDialog(title, body, okText, onOk) {
  const d = $('#dialog');
  const ok = okText ? h('button', { class: 'pp-btn pp-btn--primary', onclick: async () => { ok.disabled = true; try { await onOk(); d.close(); } catch (e) { ok.disabled = false; } } }, okText) : null;
  d.replaceChildren(h('div', { class: 'pp-dialog-head' }, h('div', { class: 'pp-h2' }, title)), h('div', { class: 'pp-dialog-body' }, body),
    h('div', { class: 'pp-dialog-foot' }, h('button', { class: 'pp-btn pp-btn--ghost', onclick: () => d.close() }, okText ? 'Cancel' : 'Close'), ok));
  d.showModal();
}
function confirmDialog(title, text, okText, onOk) { openDialog(title, [h('p', { style: 'margin:0' }, text)], okText, onOk); }

// ---------------------------------------------------------------- jobs, status, routing
function watchJob(id) {
  const tick = async () => {
    try {
      const j = await api.get('/api/jobs/' + id);
      tip(`${j.title}: ${j.lines.at(-1) || '…'}`);
      if (j.running) { setTimeout(tick, 700); return; }
      toast(j.exit === 0 ? `${j.title}: done` : `${j.title} failed (exit ${j.exit}). See Activity.`, j.exit === 0 ? '' : 'danger');
      refreshStatus(); if (['#play', '#profiles', '#activity'].includes(location.hash.split('/')[0] || '#play')) route();
    } catch (e) { /* the job list is per session */ }
  };
  tick(); refreshStatus();
}
async function refreshStatus() {
  try {
    S.status = await api.get('/api/status');
    const ready = S.status.launcher && S.status.port;
    const pill = $('#runtime-pill');
    pill.className = `pp-tag ${ready ? 'pp-tag--ok' : 'pp-tag--danger'}`;
    pill.textContent = ready ? `Ready${S.status.melee_unlocked ? ' · Melee Unlocked ' + S.status.melee_unlocked : ''}` : 'Setup needed';
    pill.dataset.tip = ready ? 'The game and the PascalPatch runtime were found.' : S.status.problems.join(' ');
    $('#n-profiles').textContent = S.status.profiles || '';
    $('#n-plugins').textContent = S.status.plugins || '';
    $('#n-jobs').textContent = S.status.running.length || '';
    $('#footer-version').textContent = `PascalPatch ${S.status.version}`;
  } catch (e) { /* server gone */ }
}
function applyTheme(t) { document.documentElement.dataset.theme = t === 'light' ? 'light' : 'dark'; }
const PAGES = { '#play': pagePlay, '#profiles': pageProfiles, '#plugins': pagePlugins, '#browse': pageBrowse, '#activity': pageActivity, '#settings': pageSettings };
async function route() {
  const [base, arg] = location.hash.split('/');
  const hash = PAGES[base] ? base : '#play';
  document.querySelectorAll('.pp-nav-item').forEach((a) => {
    if (a.getAttribute('href') === hash) a.setAttribute('aria-current', 'page'); else a.removeAttribute('aria-current');
  });
  const root = $('#page'); const fresh = h('div', { class: 'pp-page', id: 'page' });
  try { await PAGES[hash](fresh, arg && decodeURIComponent(arg)); } catch (e) { fresh.append(h('div', { class: 'pp-notice pp-notice--danger' }, e.message)); }
  root.replaceWith(fresh);
}
window.addEventListener('hashchange', route);
$('#open-studio').addEventListener('click', () => run(() => api.post('/api/studio'), 'Opening Character Studio…'));
(async () => {
  try { applyTheme((await api.get('/api/settings')).theme); } catch (e) {}
  await refreshStatus(); route(); setInterval(refreshStatus, 5000);
})();
