// PascalPatch plugin site: lists what catalog.json describes and index.json (signed) offers.
// The browser never decides what gets installed; the PascalPatch app verifies the index itself.
"use strict";
(() => {
  const $ = (s) => document.querySelector(s);
  const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  // A README's Markdown, the small part plugins use: paragraphs, "- " lists (continued by indented
  // lines), **bold** and `code`. Everything is escaped first, so a README can never inject markup.
  const inline = (t) => esc(t).replace(/`([^`]+)`/g, "<code>$1</code>").replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>");
  const readme = (text) => String(text || "").split(/\n\s*\n/).map((block) => {
    const lines = block.split("\n");
    if (/^\s*[-*] /.test(lines[0])) {
      const items = [];
      for (const l of lines) {
        if (/^\s*[-*] /.test(l)) items.push(l.replace(/^\s*[-*] /, ""));
        else if (items.length) items[items.length - 1] += " " + l.trim();
      }
      return `<ul>${items.map((i) => `<li>${inline(i)}</li>`).join("")}</ul>`;
    }
    return `<p>${inline(lines.map((l) => l.trim()).join(" "))}</p>`;
  }).join("");
  const verKey = (v) => String(v).split(/[.-]/).map((p) => (/^\d+$/.test(p) ? p.padStart(8, "0") : "~" + p)).join(".");
  let plugins = [], tag = "all";

  async function load() {
    try {
      const [cat, idx] = await Promise.all([fetch("catalog.json").then((r) => r.json()), fetch("index.json").then((r) => r.json())]);
      const entries = {};   // newest signed version per id
      for (const e of idx.entries || []) if (!entries[e.id] || verKey(e.version) > verKey(entries[e.id].version)) entries[e.id] = e;
      plugins = (cat.plugins || []).filter((c) => entries[c.id]).map((c) => ({ ...c, entry: entries[c.id] }));
      $("#generated").textContent = cat.generated ? new Date(cat.generated * 1000).toLocaleDateString() : "—";
      tags();
      render();
    } catch (err) {
      $("#notice").innerHTML = `<div class="pp-notice pp-notice--danger">Could not load the plugin list (${esc(err.message)}).</div>`;
    }
  }

  function tags() {
    const all = ["all", ...new Set(plugins.flatMap((p) => p.tags || []))];
    $("#tags").innerHTML = all.map((t) => `<button class="pp-tab" role="tab" aria-selected="${t === tag}" data-tag="${esc(t)}">${esc(t === "all" ? "All" : t)}</button>`).join("");
  }

  function render() {
    const q = $("#q").value.trim().toLowerCase();
    const shown = plugins.filter((p) => (tag === "all" || (p.tags || []).includes(tag)) &&
      (!q || [p.name, p.summary, p.author, ...(p.tags || [])].join(" ").toLowerCase().includes(q)));
    $("#grid").innerHTML = shown.length ? shown.map(card).join("") :
      `<div class="pp-empty">${plugins.length ? "No plugin matches that search." : "No plugins published yet."}</div>`;
  }

  function icon(p) {
    return p.icon ? `<span class="plugin-icon"><img src="${esc(p.icon)}" alt=""></span>` : `<span class="plugin-icon">${esc((p.name || p.id)[0])}</span>`;
  }

  function card(p) {
    return `<article class="pp-card is-interactive plugin-card" tabindex="0" data-id="${esc(p.id)}">
      <div class="pp-card-head">${icon(p)}<div><h3 class="pp-card-title">${esc(p.name)}</h3>
        <div class="meta">v${esc(p.entry.version)} · ${esc(p.author || p.entry.maintainer)}</div></div></div>
      <p class="plugin-summary">${esc(p.summary)}</p>
      <div class="pp-card-foot">${(p.tags || []).map((t) => `<span class="pp-tag">${esc(t)}</span>`).join("")}
        ${p.entry.compatibility === "offline-only" ? `<span class="pp-tag pp-tag--info">offline</span>` : ""}</div>
    </article>`;
  }

  function settingRow(s) {
    const range = s.type === "int" || s.type === "float" ? ` (${s.min ?? "…"} to ${s.max ?? "…"})` : "";
    const opts = s.type === "choice" ? `: ${(s.options || []).map((o) => esc(o.label ?? o)).join(", ")}` : "";
    return `<li><b>${esc(s.label || s.key)}</b> <span class="pp-dim">${esc(s.type)}${range}${opts}</span></li>`;
  }

  function details(id) {
    const p = plugins.find((x) => x.id === id);
    if (!p) return;
    const e = p.entry;
    $("#d-title").textContent = p.name;
    $("#d-body").innerHTML = `
      <p class="plugin-summary">${esc(p.summary)}</p>
      ${p.description ? `<div class="readme">${readme(p.description)}</div>` : ""}
      ${(p.settings || []).length ? `<div><h4 class="pp-h3">In the F2 window</h4><ul>${p.settings.map(settingRow).join("")}</ul></div>` : ""}
      <dl class="detail-grid">
        <dt>Version</dt><dd>${esc(e.version)}</dd>
        <dt>Author</dt><dd>${esc(p.author || e.maintainer)}</dd>
        <dt>License</dt><dd>${esc(e.license)}</dd>
        <dt>Needs</dt><dd>${p.min_runtime ? `PascalPatch ${esc(p.min_runtime)} or newer` : `PascalPatch runtime ABI ${esc(p.abi ?? 1)}`}</dd>
        <dt>SHA-256</dt><dd class="pp-mono pp-small">${esc(e.sha256)}</dd>
        ${(p.changelog || []).length ? `<dt>Changes</dt><dd>${p.changelog.map((c) => `<div><b>${esc(c.version)}</b> ${esc(c.notes)}</div>`).join("")}</dd>` : ""}
      </dl>
      <div class="pp-notice">Install it from the <b>Browse</b> page of the PascalPatch app, which checks the signature and the hash.</div>`;
    $("#d-foot").innerHTML = `
      <a class="pp-btn pp-btn--ghost" href="${esc(e.source)}">Download ZIP</a>
      <a class="pp-btn pp-btn--primary" href="http://127.0.0.1:8790/#browse/${encodeURIComponent(p.id)}">Open in PascalPatch</a>`;
    $("#details").showModal();
  }

  $("#q").addEventListener("input", render);
  $("#tags").addEventListener("click", (ev) => {
    const b = ev.target.closest("[data-tag]");
    if (!b) return;
    tag = b.dataset.tag; tags(); render();
  });
  $("#grid").addEventListener("click", (ev) => { const c = ev.target.closest("[data-id]"); if (c) details(c.dataset.id); });
  $("#grid").addEventListener("keydown", (ev) => {
    const c = ev.target.closest("[data-id]");
    if (c && (ev.key === "Enter" || ev.key === " ")) { ev.preventDefault(); details(c.dataset.id); }
  });
  $("#d-close").addEventListener("click", () => $("#details").close());
  load();
})();
