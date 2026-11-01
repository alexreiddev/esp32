const CATS = {
  all: "All",
  hardware: "Hardware & IoT",
  client: "Client & business",
  apps: "Apps & tools",
  games: "Games & stories",
  pitch: "Pitches",
  demos: "Fix demos",
};
const STATUS = {
  live: ["Live", "--live"],
  building: ["Building", "--building"],
  shipped: ["Shipped", "--shipped"],
  idea: ["Planned", "--idea"],
};
const ORDER = { building: 0, live: 1, shipped: 2, idea: 3 };
const P = [...window.PROJECTS].sort(
  (a, b) => ORDER[a.status] - ORDER[b.status] || b.updated.localeCompare(a.updated)
);
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
let cat = "all", q = "";
try { cat = new URLSearchParams(location.search).get("c") || "all"; if (!CATS[cat]) cat = "all"; } catch (e) {}

const count = (fn) => P.filter(fn).length;
document.getElementById("stats").innerHTML = [
  [P.length, "projects"],
  [count((p) => p.status === "live"), "live now"],
  [count((p) => p.status === "building"), "in progress"],
  [new Set(P.map((p) => p.category)).size, "areas"],
].map(([n, l]) => `<div class="stat"><b>${n}</b><span>${l}</span></div>`).join("");

function chips() {
  document.getElementById("chips").innerHTML = Object.entries(CATS).map(([k, l]) => {
    const n = k === "all" ? P.length : count((p) => p.category === k);
    return n ? `<button class="chip" data-c="${k}" aria-pressed="${cat === k}">${l}<span class="n">${n}</span></button>` : "";
  }).join("");
}

function render() {
  const t = q.trim().toLowerCase();
  const list = P.filter((p) =>
    (cat === "all" || p.category === cat) &&
    (!t || [p.name, p.info, ...(p.tags || [])].join(" ").toLowerCase().includes(t))
  );
  document.getElementById("grid").innerHTML = list.map((p) => {
    const [sl, sv] = STATUS[p.status] || STATUS.shipped;
    const links = (p.links || []).length
      ? p.links.filter((l) => /^https:\/\//.test(l.url)).map((l) => `<a href="${esc(l.url)}" target="_blank" rel="noopener noreferrer">${esc(l.label)} ↗</a>`).join("")
      : `<span class="none">Private · ask for a demo</span>`;
    return `<article class="card">
      <div class="top"><span class="cat">${esc(CATS[p.category] || p.category)}</span>
        <span class="status ${p.status}" style="--s:var(${sv})">${sl}</span></div>
      <h2>${esc(p.name)}</h2>
      <p>${esc(p.info)}</p>
      <div class="tags">${(p.tags || []).map((x) => `<span class="tag">${esc(x)}</span>`).join("")}</div>
      <div class="links">${links}</div>
    </article>`;
  }).join("");
  document.getElementById("empty").style.display = list.length ? "none" : "block";
}

document.getElementById("chips").addEventListener("click", (e) => {
  const b = e.target.closest("[data-c]");
  if (!b) return;
  cat = b.dataset.c;
  try { history.replaceState(null, "", cat === "all" ? location.pathname : "?c=" + cat); } catch (e) {}
  chips(); render();
});
document.getElementById("q").addEventListener("input", (e) => { q = e.target.value; render(); });
document.getElementById("yr").textContent = new Date().getFullYear();
chips(); render();
