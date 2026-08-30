#pragma once

#include <string_view>

namespace cyka::aim {

inline constexpr std::string_view kTtdViewerCss = R"css(
:root { --bg:#121212; --fg:#eee; --muted:#9aa; --accent:#6cf; }
* { box-sizing:border-box; }
html,body { margin:0; background:var(--bg); color:var(--fg); font:14px/1.4 system-ui,sans-serif; }
body { padding:1.25rem 1.5rem 4rem; }
h1 { font-size:1.4rem; margin:0 0 .5rem; }
h2 { font-size:1.1rem; margin:0 0 .35rem; }
.lead,.card p { color:var(--muted); max-width:70rem; }
.card { margin:2rem 0 2.5rem; }
.toolbar { display:flex; flex-wrap:wrap; gap:.5rem 1rem; align-items:center; margin:.6rem 0 .8rem; }
.toolbar button,.ov-bar button {
  background:#2a2a2a; color:var(--fg); border:1px solid #444; border-radius:6px;
  padding:.35rem .7rem; cursor:pointer; font:inherit;
}
.toolbar button:hover,.ov-bar button:hover { background:#3a3a3a; }
.toolbar label { color:var(--muted); display:flex; align-items:center; gap:.35rem; }
.toolbar input,.ov-bar input {
  width:4.5rem; background:#1a1a1a; color:var(--fg); border:1px solid #444;
  border-radius:4px; padding:.25rem .4rem; font:inherit;
}
.row { display:flex; flex-wrap:wrap; gap:8px; }
.thumb {
  display:flex; flex-direction:column; gap:4px; width:240px; padding:0; margin:0;
  background:transparent; border:2px solid transparent; border-radius:4px;
  color:inherit; cursor:pointer; text-align:left; font:inherit;
}
.thumb:hover,.thumb.on { border-color:var(--accent); }
.thumb img { width:240px; height:auto; background:#111; image-rendering:pixelated; display:block; }
.cap { font-size:11px; color:#ccc; }
.ov {
  display:none; position:fixed; inset:0; z-index:20; background:#000;
  flex-direction:column;
}
.ov.open { display:flex; }
.ov-stage {
  flex:1 1 0; min-height:0; width:100%;
  display:flex; align-items:center; justify-content:center;
}
.ov-stage img {
  width:100%; height:100%;
  object-fit:contain; object-position:center;
  image-rendering:pixelated; image-rendering:crisp-edges;
  background:#000;
}
.ov-bar {
  flex:none; width:100%; display:flex; flex-wrap:wrap; gap:.6rem 1rem; align-items:center;
  justify-content:center; padding:.65rem 1rem; background:#111; color:#eee; font:13px/1.3 system-ui,sans-serif;
}
.ov-bar .meta { min-width:12rem; text-align:center; }
)css";

inline constexpr std::string_view kTtdViewerJs = R"js(
(function () {
  const ov = document.getElementById("ttd-ov");
  const ovImg = document.getElementById("ttd-ov-img");
  const ovMeta = document.getElementById("ttd-ov-meta");
  const ovSpeed = document.getElementById("ttd-ov-speed");
  const ovPlay = document.getElementById("ttd-ov-play");
  let strip = [];
  let idx = 0;
  let timer = null;
  let cardSpeed = null;

  function thumbs(card) {
    return Array.prototype.slice.call(card.querySelectorAll(".thumb"));
  }
  function speedSec() {
    const src = ov.classList.contains("open") ? ovSpeed : cardSpeed;
    const v = parseFloat(src && src.value);
    return Number.isFinite(v) && v > 0 ? v : 0.1;
  }
  function cap(th) {
    return th ? th.getAttribute("data-cap") || "" : "";
  }
  function show() {
    if (!strip.length) return;
    const th = strip[idx];
    const img = th.querySelector("img");
    ovImg.src = img.getAttribute("src");
    ovMeta.textContent = (idx + 1) + "/" + strip.length + "  " + cap(th);
    strip.forEach(function (t, j) { t.classList.toggle("on", j === idx); });
  }
  function stop() {
    if (timer !== null) {
      clearInterval(timer);
      timer = null;
    }
    ovPlay.textContent = "Play";
    document.querySelectorAll(".card-play").forEach(function (b) {
      b.textContent = "Play";
    });
  }
  function play() {
    if (timer !== null) {
      stop();
      return;
    }
    ovPlay.textContent = "Pause";
    if (cardSpeed) {
      const b = cardSpeed.closest(".card").querySelector(".card-play");
      if (b) b.textContent = "Pause";
    }
    timer = setInterval(function () {
      if (idx >= strip.length - 1) {
        stop();
        return;
      }
      idx += 1;
      show();
    }, speedSec() * 1000);
  }
  function openAt(card, i) {
    strip = thumbs(card);
    idx = Math.max(0, Math.min(i, strip.length - 1));
    cardSpeed = card.querySelector(".card-speed");
    if (cardSpeed) ovSpeed.value = cardSpeed.value || "0.1";
    ov.classList.add("open");
    show();
  }
  function close() {
    stop();
    ov.classList.remove("open");
  }
  function step(d) {
    if (!strip.length) return;
    idx = Math.max(0, Math.min(strip.length - 1, idx + d));
    show();
  }

  document.querySelectorAll(".card").forEach(function (card) {
    thumbs(card).forEach(function (th, i) {
      th.addEventListener("click", function () { openAt(card, i); });
    });
    const playBtn = card.querySelector(".card-play");
    const spd = card.querySelector(".card-speed");
    if (playBtn) {
      playBtn.addEventListener("click", function () {
        if (timer !== null && cardSpeed === spd) {
          stop();
          return;
        }
        openAt(card, 0);
        play();
      });
    }
  });
  ovPlay.addEventListener("click", play);
  document.getElementById("ttd-ov-prev").addEventListener("click", function () { step(-1); });
  document.getElementById("ttd-ov-next").addEventListener("click", function () { step(1); });
  document.getElementById("ttd-ov-close").addEventListener("click", close);
  ov.addEventListener("click", function (e) {
    if (e.target === ov || e.target.id === "ttd-ov-stage") close();
  });
  document.addEventListener("keydown", function (e) {
    if (!ov.classList.contains("open")) return;
    if (e.key === "Escape") close();
    else if (e.key === "ArrowLeft" || e.key === "ArrowUp") { e.preventDefault(); step(-1); }
    else if (e.key === "ArrowRight" || e.key === "ArrowDown") { e.preventDefault(); step(1); }
    else if (e.key === " ") { e.preventDefault(); play(); }
  });
})();
)js";

} // namespace cyka::aim
