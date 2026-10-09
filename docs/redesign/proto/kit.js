/* Snapper prototype kit behaviour. Plain JS, no libraries.
   One small manager per concern, all reached through window.Kit.
   Controls are wired by event delegation on document, so markup added
   later by a screen works with no init call. */
(function () {
  "use strict";

  const kDragThreshold = 4;
  const kSnapCatch = 6;
  const kChipOffset = 14;
  const kToastMs = 4000;
  const kWhyMs = 2400;
  const kMaxToasts = 4;
  const kMathMaxLen = 64;
  const kMotionTable = {
    press: 80, hover: 100, pop: 150, move: 200, max: 300,
    flash: 100, puff: 120, shake: 200
  };
  const kMotionScale = { full: 1, calm: 0.65, off: 0 };

  function store(key, value) {
    try {
      if (value === undefined) return window.localStorage.getItem(key);
      window.localStorage.setItem(key, value);
    } catch (err) {
      return null;
    }
    return null;
  }

  function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }

  function emit(el, name, detail) {
    el.dispatchEvent(new CustomEvent(name, { bubbles: true, detail: detail }));
  }

  /* ---------- Motion: Full, Calm, Off ---------- */
  const Motion = {
    mode: "full",
    init() {
      const saved = store("snapper.motion");
      const reduce = window.matchMedia &&
        window.matchMedia("(prefers-reduced-motion: reduce)").matches;
      const has_saved = saved === "full" || saved === "calm" || saved === "off";
      this.set(has_saved ? saved : (reduce ? "calm" : "full"), false);
    },
    set(mode, remember) {
      const is_known = Object.prototype.hasOwnProperty.call(kMotionScale, mode);
      if (!is_known) return;
      this.mode = mode;
      document.documentElement.dataset.motion = mode;
      if (remember !== false) store("snapper.motion", mode);
      document.querySelectorAll("[data-kit-motion]").forEach((b) => {
        b.setAttribute("aria-pressed", String(b.dataset.kitMotion === mode));
      });
      emit(document, "kit:motion", { mode: mode });
    },
    ms(name) {
      const base = kMotionTable[name] !== undefined ? kMotionTable[name] : name;
      return Math.round(Number(base) * kMotionScale[this.mode]);
    },
    get on() { return this.mode !== "off"; }
  };

  /* Restart a CSS animation class and call done when it ends. */
  function play(el, cls, ms, done) {
    if (!el) return;
    el.classList.remove(cls);
    void el.offsetWidth;
    const wait = Motion.ms(ms);
    if (wait <= 0) {
      if (done) done();
      return;
    }
    el.classList.add(cls);
    window.setTimeout(() => {
      el.classList.remove(cls);
      if (done) done();
    }, wait + 20);
  }

  /* ---------- Sound: tiny WebAudio ticks, off by default ---------- */
  const Sound = {
    enabled: false,
    ctx: null,
    init() { this.set(store("snapper.sound") === "on", false); },
    set(on, remember) {
      this.enabled = Boolean(on);
      if (remember !== false) store("snapper.sound", this.enabled ? "on" : "off");
      document.querySelectorAll("[data-kit-sound]").forEach((b) => {
        b.setAttribute("aria-pressed", String(this.enabled));
      });
      if (this.enabled) this.blip(880, 0.03, "square", 0.03);
    },
    blip(freq, secs, type, vol) {
      if (!this.enabled) return;
      const Ctx = window.AudioContext || window.webkitAudioContext;
      if (!Ctx) return;
      try {
        if (!this.ctx) this.ctx = new Ctx();
        const t = this.ctx.currentTime;
        const osc = this.ctx.createOscillator();
        const gain = this.ctx.createGain();
        osc.type = type;
        osc.frequency.setValueAtTime(freq, t);
        gain.gain.setValueAtTime(vol, t);
        gain.gain.exponentialRampToValueAtTime(0.0001, t + secs);
        osc.connect(gain).connect(this.ctx.destination);
        osc.start(t);
        osc.stop(t + secs + 0.02);
      } catch (err) {
        this.enabled = false;
      }
    },
    click() { this.blip(520, 0.025, "square", 0.025); },
    tick() { this.blip(1600, 0.015, "square", 0.02); },
    pop() { this.blip(740, 0.06, "sine", 0.05); },
    error() { this.blip(140, 0.09, "sawtooth", 0.03); }
  };

  /* ---------- Small motion helpers ---------- */
  function pop(el, from) {
    if (!el) return;
    el.style.setProperty("--pop-from", String(from || 0.85));
    play(el, "k-pop", "pop");
  }
  function puff(el, remove, done) {
    if (!el) return;
    play(el, "k-puff", "puff", () => {
      const should_remove = remove !== false;
      if (should_remove) el.remove();
      else el.hidden = true;
      if (done) done();
    });
  }
  function flash(el) { play(el, "k-flash", "flash"); }
  function bounce(el, dx, dy) {
    if (!el) return;
    const len = Math.hypot(dx || 0, dy || 0) || 1;
    el.style.setProperty("--bx", ((dx || 0) / len * 2).toFixed(2) + "px");
    el.style.setProperty("--by", ((dy || 0) / len * 2).toFixed(2) + "px");
    const has_bounce = Motion.mode === "full";
    if (has_bounce) play(el, "k-bounce", "pop");
  }
  function ring(container, x, y) {
    if (!container || !Motion.on) return;
    const r = document.createElement("div");
    r.className = "k-ring";
    r.style.left = x + "px";
    r.style.top = y + "px";
    container.appendChild(r);
    window.setTimeout(() => r.remove(), 300);
  }

  /* ---------- Tooltip and why bubble ---------- */
  const Tip = {
    el: null,
    timer: 0,
    warm_until: 0,
    target: null,
    ensure() {
      if (this.el) return this.el;
      this.el = document.createElement("div");
      this.el.className = "tip";
      this.el.setAttribute("role", "tooltip");
      this.el.hidden = true;
      document.body.appendChild(this.el);
      return this.el;
    },
    textFor(t) {
      const is_off = t.getAttribute("aria-disabled") === "true";
      if (is_off && t.dataset.why) return { text: t.dataset.why, why: true };
      return { text: t.dataset.tip || "", why: false };
    },
    over(t) {
      if (t === this.target) return;
      this.hide();
      const info = this.textFor(t);
      if (!info.text) return;
      this.target = t;
      const warm = performance.now() < this.warm_until;
      this.timer = window.setTimeout(() => this.show(t, info), warm ? 0 : 450);
    },
    show(t, info) {
      const tip = this.ensure();
      tip.textContent = info.text;
      if (t.dataset.key) {
        const k = document.createElement("span");
        k.className = "tip-key";
        k.textContent = t.dataset.key;
        tip.appendChild(k);
      }
      tip.classList.toggle("is-why", info.why);
      tip.hidden = false;
      placeNear(tip, t.getBoundingClientRect());
    },
    hide() {
      window.clearTimeout(this.timer);
      if (this.el && !this.el.hidden) {
        this.el.hidden = true;
        this.warm_until = performance.now() + 600;
      }
      this.target = null;
    }
  };

  /* Put a floating box under a rect, flipped and clamped to the view. */
  function placeNear(box, r) {
    const bw = box.offsetWidth;
    const bh = box.offsetHeight;
    const vw = document.documentElement.clientWidth;
    const vh = document.documentElement.clientHeight;
    let x = r.left + r.width / 2 - bw / 2;
    let y = r.bottom + 6;
    const below_fits = y + bh <= vh - 4;
    if (!below_fits) y = r.top - bh - 6;
    x = clamp(x, 4, Math.max(4, vw - bw - 4));
    box.style.left = Math.round(x) + "px";
    box.style.top = Math.round(Math.max(4, y)) + "px";
  }

  const Why = {
    live: null,
    announce(text) {
      if (!this.live) {
        this.live = document.createElement("div");
        this.live.className = "sr-only";
        this.live.setAttribute("aria-live", "polite");
        document.body.appendChild(this.live);
      }
      this.live.textContent = text;
    }
  };

  /* Blocked action: shake 4 px for 200 ms and say why next to it. */
  function shake(el, why) {
    if (!el) return;
    Tip.hide();
    Sound.error();
    if (Motion.on) play(el, "k-shake", "shake");
    else play(el, "k-flash-bad", 300);
    const text = why || el.dataset.why || "";
    if (!text) return;
    Why.announce(text);
    const old = el._kitWhy;
    if (old) old.remove();
    const b = document.createElement("div");
    b.className = "why";
    b.textContent = text;
    document.body.appendChild(b);
    placeNear(b, el.getBoundingClientRect());
    el._kitWhy = b;
    window.setTimeout(() => {
      const is_current = el._kitWhy === b;
      if (is_current) el._kitWhy = null;
      b.remove();
    }, kWhyMs);
  }

  /* ---------- Number chip ---------- */
  const Chip = {
    el: null,
    show(text, x, y, snapped) {
      if (!this.el) {
        this.el = document.createElement("div");
        this.el.className = "numchip";
        this.el.hidden = true;
        document.body.appendChild(this.el);
      }
      const was_hidden = this.el.hidden;
      this.el.textContent = text;
      this.el.classList.toggle("is-snap", Boolean(snapped));
      this.el.hidden = false;
      if (was_hidden) play(this.el, "k-pop", "pop");
      this.move(x, y);
    },
    move(x, y) {
      if (!this.el || this.el.hidden) return;
      const vw = document.documentElement.clientWidth;
      const vh = document.documentElement.clientHeight;
      const w = this.el.offsetWidth;
      const h = this.el.offsetHeight;
      let cx = x + kChipOffset;
      let cy = y + kChipOffset;
      const right_fits = cx + w <= vw - 4;
      if (!right_fits) cx = x - kChipOffset - w;
      const below_fits = cy + h <= vh - 4;
      if (!below_fits) cy = y - kChipOffset - h;
      this.el.style.left = Math.round(cx) + "px";
      this.el.style.top = Math.round(cy) + "px";
    },
    hide() { if (this.el) this.el.hidden = true; }
  };

  /* ---------- Toasts with Undo ---------- */
  const Toasts = {
    box: null,
    ensure() {
      if (this.box) return this.box;
      this.box = document.createElement("div");
      this.box.className = "toasts";
      this.box.setAttribute("aria-live", "polite");
      document.body.appendChild(this.box);
      return this.box;
    },
    show(msg, undoFn, opts) {
      const o = opts || {};
      const box = this.ensure();
      const t = document.createElement("div");
      t.className = "toast" + (o.kind ? " is-" + o.kind : "");
      t.setAttribute("role", o.kind === "error" ? "alert" : "status");
      const m = document.createElement("span");
      m.className = "toast-msg";
      m.textContent = msg;
      t.appendChild(m);
      const has_undo = typeof undoFn === "function";
      if (has_undo) {
        const u = document.createElement("button");
        u.className = "btn small";
        u.textContent = "Undo";
        u.addEventListener("click", () => {
          undoFn();
          m.textContent = "Undone.";
          u.remove();
          life.reset(1200);
        });
        t.appendChild(u);
      }
      const x = document.createElement("button");
      x.className = "btn small toast-x";
      x.setAttribute("aria-label", "Close");
      x.textContent = "x";
      x.addEventListener("click", () => life.close());
      t.appendChild(x);
      const bar = document.createElement("div");
      bar.className = "toast-time";
      t.appendChild(bar);
      box.appendChild(t);
      while (box.children.length > kMaxToasts) box.firstElementChild.remove();
      Sound.pop();
      const life = this.lifetime(t, bar, o.ms || kToastMs);
      return life;
    },
    lifetime(t, bar, ms) {
      let left = ms;
      let started = performance.now();
      let timer = 0;
      const close = () => {
        window.clearTimeout(timer);
        play(t, "k-leave", "puff", () => t.remove());
      };
      const run = () => {
        started = performance.now();
        timer = window.setTimeout(close, left);
        bar.style.animation = "k-shrink " + left + "ms linear forwards";
      };
      const pause = () => {
        window.clearTimeout(timer);
        left = Math.max(400, left - (performance.now() - started));
        bar.style.animation = "none";
        bar.style.scale = String(left / ms) + " 1";
      };
      t.addEventListener("pointerenter", pause);
      t.addEventListener("pointerleave", run);
      run();
      return {
        close: close,
        reset(newMs) {
          window.clearTimeout(timer);
          left = newMs;
          ms = newMs;
          bar.style.scale = "";
          run();
        }
      };
    }
  };
  function toast(msg, undoFn, opts) { return Toasts.show(msg, undoFn, opts); }

  /* ---------- Drag helper: 4 px threshold, ghost, chip, Escape ---------- */
  const Drag = {
    active: null,
    cancelActive() {
      const a = this.active;
      if (!a) return;
      a.finish(true);
    }
  };

  function makeGhost(el) {
    const g = el.cloneNode(true);
    g.removeAttribute("id");
    g.classList.add("ghost");
    g.classList.remove("is-hover", "is-picked", "is-dragging");
    g.setAttribute("aria-hidden", "true");
    const is_abs = getComputedStyle(el).position === "absolute";
    if (is_abs) {
      el.parentNode.insertBefore(g, el);
    } else {
      const r = el.getBoundingClientRect();
      g.style.position = "fixed";
      g.style.left = r.left + "px";
      g.style.top = r.top + "px";
      g.style.width = r.width + "px";
      g.style.margin = "0";
      document.body.appendChild(g);
    }
    return g;
  }

  /* opts: threshold, ghost (bool), chip(state) -> text, onStart, onMove,
     onEnd, onCancel, onClick, onDown. state has x0,y0,x,y,dx,dy,mx,my,
     shift, alt, ctrl, event. Returns a function that unwires it. */
  function drag(el, opts) {
    const o = opts || {};
    const threshold = o.threshold === undefined ? kDragThreshold : o.threshold;
    const down = (e) => {
      const is_main = e.button === 0;
      if (!is_main || Drag.active) return;
      if (o.filter && !o.filter(e)) return;
      const s = { x0: e.clientX, y0: e.clientY, x: e.clientX, y: e.clientY,
        dx: 0, dy: 0, mx: 0, my: 0, started: false, ghost: null, event: e,
        target: e.target, el: el };
      if (o.onDown) o.onDown(s);
      try { el.setPointerCapture(e.pointerId); } catch (err) { /* ignore */ }
      const move = (ev) => {
        s.mx = ev.clientX - s.x;
        s.my = ev.clientY - s.y;
        s.x = ev.clientX; s.y = ev.clientY;
        s.dx = s.x - s.x0; s.dy = s.y - s.y0;
        s.shift = ev.shiftKey; s.alt = ev.altKey; s.ctrl = ev.ctrlKey || ev.metaKey;
        s.event = ev;
        const past_threshold = Math.hypot(s.dx, s.dy) >= threshold;
        if (!s.started && past_threshold) {
          s.started = true;
          document.body.classList.add("kit-dragging");
          el.classList.add("is-dragging");
          Tip.hide();
          if (o.ghost) s.ghost = makeGhost(o.ghostOf ? o.ghostOf(s) : el);
          if (o.onStart) o.onStart(s);
        }
        if (!s.started) return;
        if (o.onMove) o.onMove(s);
        const text = o.chip ? o.chip(s) : "";
        if (text) Chip.show(text, s.x, s.y, s.snapped);
      };
      const finish = (cancelled) => {
        el.removeEventListener("pointermove", move);
        el.removeEventListener("pointerup", up);
        el.removeEventListener("pointercancel", cancel);
        window.removeEventListener("keydown", key, true);
        Drag.active = null;
        Chip.hide();
        document.body.classList.remove("kit-dragging");
        el.classList.remove("is-dragging");
        if (s.ghost) s.ghost.remove();
        if (!s.started && !cancelled) {
          if (o.onClick) o.onClick(s);
          return;
        }
        if (cancelled && s.started && o.onCancel) o.onCancel(s);
        if (!cancelled && o.onEnd) o.onEnd(s);
      };
      const up = () => finish(false);
      const cancel = () => finish(true);
      const key = (ev) => {
        const is_escape = ev.key === "Escape";
        if (!is_escape) return;
        ev.preventDefault();
        ev.stopPropagation();
        finish(true);
      };
      el.addEventListener("pointermove", move);
      el.addEventListener("pointerup", up);
      el.addEventListener("pointercancel", cancel);
      window.addEventListener("keydown", key, true);
      Drag.active = { finish: finish };
    };
    el.addEventListener("pointerdown", down);
    return () => el.removeEventListener("pointerdown", down);
  }

  /* ---------- Snapping: catch within 6 px, flash target, bounce ---------- */
  /* targets: numbers or { at, el }. Returns a stateful snapper so the
     flash and tick fire only when a snap is entered, not every move. */
  function snapper(opts) {
    const o = opts || {};
    const reach = o.catch === undefined ? kSnapCatch : o.catch;
    let last = null;
    return {
      test(value, targets) {
        let best = null;
        let best_d = reach + 0.001;
        const list = targets || [];
        for (let i = 0; i < list.length && i < 1000; i++) {
          const t = typeof list[i] === "number" ? { at: list[i] } : list[i];
          const d = Math.abs(t.at - value);
          const is_closer = d <= best_d;
          if (is_closer) { best = t; best_d = d; }
        }
        const entered = best !== null && (last === null || last.at !== best.at);
        if (entered) {
          if (best.el) flashTarget(best.el);
          Sound.tick();
        }
        last = best;
        return { value: best ? best.at : value, hit: best, entered: entered };
      },
      reset() { last = null; }
    };
  }
  function flashTarget(el) {
    el.classList.add("k-flash-on", "is-on");
    window.setTimeout(() => el.classList.remove("k-flash-on"), Math.max(100, Motion.ms("flash")));
    flash(el);
  }
  function snap(value, targets, reach) {
    return snapper({ catch: reach }).test(value, targets);
  }

  /* ---------- Hover outline and nearest handle for canvas items ---------- */
  function nearest(container, selector, x, y, radius) {
    const r = radius === undefined ? 12 : radius;
    let best = null;
    let best_d = r + 0.001;
    const all = container.querySelectorAll(selector);
    for (let i = 0; i < all.length && i < 2000; i++) {
      const b = all[i].getBoundingClientRect();
      const d = Math.hypot(b.left + b.width / 2 - x, b.top + b.height / 2 - y);
      const is_closer = d <= best_d;
      if (is_closer) { best = all[i]; best_d = d; }
    }
    return best;
  }
  function hoverOutline(container, opts) {
    const o = opts || {};
    const items = o.items || ".item";
    const handles = o.handles || ".handle";
    const grab = o.grab || 12;
    let current = null;
    const set = (el) => {
      if (el === current) return;
      if (current) current.classList.remove("is-hover");
      current = el;
      if (current) current.classList.add("is-hover");
      if (o.onChange) o.onChange(current);
    };
    const move = (e) => {
      if (Drag.active) return;
      const h = nearest(container, handles, e.clientX, e.clientY, grab);
      if (h) { set(h); return; }
      const under = document.elementFromPoint(e.clientX, e.clientY);
      const item = under ? under.closest(items) : null;
      const inside = item && container.contains(item) && !item.classList.contains("ghost");
      set(inside ? item : null);
    };
    container.addEventListener("pointermove", move);
    container.addEventListener("pointerleave", () => { if (!Drag.active) set(null); });
    return { get current() { return current; }, clear() { set(null); } };
  }

  /* ---------- Math in number fields ---------- */
  /* Shunting-yard, no eval. Leading + * / work on the current value:
     "*2" doubles, "+15" adds. A leading minus is a plain negative. */
  function evalMath(text, current) {
    let src = String(text || "").replace(/\s+/g, "").replace(/,/g, ".");
    if (!src || src.length > kMathMaxLen) return null;
    const is_relative = /^[+*/x]/.test(src) && typeof current === "number";
    if (is_relative) src = String(current) + src;
    src = src.replace(/x/g, "*");
    const tokens = src.match(/(\d+\.?\d*|\.\d+|[-+*/()])/g);
    if (!tokens || tokens.join("") !== src) return null;
    const prec = { "+": 1, "-": 1, "*": 2, "/": 2, "~": 3 };
    const out = [];
    const ops = [];
    let prev = null;
    for (let i = 0; i < tokens.length; i++) {
      let tk = tokens[i];
      const is_num = /^[\d.]/.test(tk);
      if (is_num) { out.push(parseFloat(tk)); prev = "n"; continue; }
      if (tk === "(") { ops.push(tk); prev = "("; continue; }
      if (tk === ")") {
        while (ops.length && ops[ops.length - 1] !== "(") out.push(ops.pop());
        if (!ops.length) return null;
        ops.pop();
        prev = "n";
        continue;
      }
      const is_unary = tk === "-" && (prev === null || prev === "(" || prev === "op");
      if (is_unary) tk = "~";
      while (ops.length && ops[ops.length - 1] !== "(" && tk !== "~" &&
             prec[ops[ops.length - 1]] >= prec[tk]) out.push(ops.pop());
      ops.push(tk);
      prev = "op";
    }
    while (ops.length) {
      const op = ops.pop();
      if (op === "(") return null;
      out.push(op);
    }
    const st = [];
    for (let i = 0; i < out.length; i++) {
      const tk = out[i];
      if (typeof tk === "number") { st.push(tk); continue; }
      if (tk === "~") {
        if (!st.length) return null;
        st.push(-st.pop());
        continue;
      }
      if (st.length < 2) return null;
      const b = st.pop();
      const a = st.pop();
      if (tk === "+") st.push(a + b);
      else if (tk === "-") st.push(a - b);
      else if (tk === "*") st.push(a * b);
      else st.push(b === 0 ? NaN : a / b);
    }
    const ok = st.length === 1 && Number.isFinite(st[0]);
    return ok ? st[0] : null;
  }

  /* ---------- Scrub fields (.num) ---------- */
  function numConfig(num) {
    const d = num.dataset;
    const step = parseFloat(d.step || "1");
    const decimals = d.decimals !== undefined ? parseInt(d.decimals, 10)
      : (String(step).split(".")[1] || "").length;
    return {
      min: d.min !== undefined ? parseFloat(d.min) : -Infinity,
      max: d.max !== undefined ? parseFloat(d.max) : Infinity,
      step: step,
      decimals: decimals,
      unit: d.unit || "",
      wrap: d.wrap !== undefined
    };
  }
  function fmt(v, decimals) {
    const s = v.toFixed(Math.max(0, decimals));
    return decimals > 0 ? s.replace(/\.?0+$/, "") : s;
  }
  function numValue(num) {
    const input = num.querySelector(".num-input");
    const v = parseFloat(num.dataset.value !== undefined ? num.dataset.value : input.value);
    return Number.isFinite(v) ? v : 0;
  }
  function numSet(num, v, live) {
    const c = numConfig(num);
    let out = v;
    const can_wrap = c.wrap && Number.isFinite(c.min) && Number.isFinite(c.max);
    if (can_wrap) {
      const span = c.max - c.min;
      out = ((out - c.min) % span + span) % span + c.min;
    } else {
      out = clamp(out, c.min, c.max);
    }
    const q = Math.pow(10, c.decimals);
    out = Math.round(out * q) / q;
    num.dataset.value = String(out);
    num.removeAttribute("data-mixed");
    const input = num.querySelector(".num-input");
    input.value = fmt(out, c.decimals);
    input.placeholder = "";
    emit(num, "kit:change", { value: out, live: Boolean(live) });
    return out;
  }
  function numCommit(num, from, to) {
    const same = from === to;
    if (same) return;
    const input = num.querySelector(".num-input");
    play(input, "k-bump", "pop");
    emit(num, "kit:commit", { from: from, to: to,
      name: num.dataset.name || (num.querySelector(".num-label") || {}).textContent || "value" });
  }

  const Scrub = {
    start(e, num) {
      const is_off = num.getAttribute("aria-disabled") === "true";
      if (is_off) return;
      const label = e.target.closest(".num-label");
      const c = numConfig(num);
      const from = numValue(num);
      let value = from;
      let started = false;
      let last_x = e.clientX;
      const x0 = e.clientX;
      try { label.setPointerCapture(e.pointerId); } catch (err) { /* ignore */ }
      num.classList.add("is-down");
      const chipText = (v) => fmt(v, c.decimals) + (c.unit ? " " + c.unit : "");
      const move = (ev) => {
        const passed = Math.abs(ev.clientX - x0) >= kDragThreshold;
        if (!started && passed) {
          started = true;
          last_x = x0;
          num.classList.add("is-scrubbing");
          document.body.classList.add("kit-scrubbing", "kit-dragging");
          Tip.hide();
        }
        if (!started) return;
        const fine = ev.altKey || ev.ctrlKey;
        const mult = ev.shiftKey ? 10 : (fine ? 0.1 : 1);
        value += (ev.clientX - last_x) * c.step * mult;
        last_x = ev.clientX;
        const shown = numSet(num, value, true);
        if (!c.wrap) value = clamp(value, c.min, c.max);
        else value = shown;
        Chip.show(chipText(shown), ev.clientX, ev.clientY);
      };
      const end = (cancelled) => {
        label.removeEventListener("pointermove", move);
        label.removeEventListener("pointerup", up);
        label.removeEventListener("pointercancel", cancel);
        window.removeEventListener("keydown", key, true);
        num.classList.remove("is-down", "is-scrubbing");
        document.body.classList.remove("kit-scrubbing", "kit-dragging");
        Chip.hide();
        Drag.active = null;
        if (!started) {
          if (!cancelled) num.querySelector(".num-input").focus();
          return;
        }
        if (cancelled) {
          numSet(num, from, false);
          return;
        }
        numCommit(num, from, numValue(num));
      };
      const up = () => end(false);
      const cancel = () => end(true);
      const key = (ev) => {
        const is_escape = ev.key === "Escape";
        if (!is_escape) return;
        ev.preventDefault();
        ev.stopPropagation();
        end(true);
      };
      label.addEventListener("pointermove", move);
      label.addEventListener("pointerup", up);
      label.addEventListener("pointercancel", cancel);
      window.addEventListener("keydown", key, true);
      Drag.active = { finish: end };
      e.preventDefault();
    },
    typed(num) {
      const input = num.querySelector(".num-input");
      const from = numValue(num);
      const raw = input.value.trim();
      const is_mixed_left = raw === "" && num.hasAttribute("data-mixed");
      if (is_mixed_left) return true;
      const v = evalMath(raw, from);
      if (v === null) {
        input.value = fmt(from, numConfig(num).decimals);
        num.classList.add("is-error");
        shake(input, "Type a number or a sum, like 12, 90/2 or *2.");
        window.setTimeout(() => num.classList.remove("is-error"), kWhyMs);
        return false;
      }
      const c = numConfig(num);
      const outside = !c.wrap && (v < c.min || v > c.max);
      const to = numSet(num, v, false);
      if (outside) shake(input, "Kept to " + fmt(c.min, c.decimals) + " .. " + fmt(c.max, c.decimals) + ".");
      numCommit(num, from, to);
      return true;
    },
    key(e, num) {
      const input = e.target;
      const c = numConfig(num);
      if (e.key === "Enter") {
        e.preventDefault();
        const ok = Scrub.typed(num);
        if (ok) input.select();
        return;
      }
      if (e.key === "Escape") {
        input.value = num.hasAttribute("data-mixed") ? "" : fmt(numValue(num), c.decimals);
        input.blur();
        return;
      }
      const is_arrow = e.key === "ArrowUp" || e.key === "ArrowDown";
      if (!is_arrow) return;
      e.preventDefault();
      const fine = e.altKey || e.ctrlKey;
      const mult = e.shiftKey ? 10 : (fine ? 0.1 : 1);
      const dir = e.key === "ArrowUp" ? 1 : -1;
      const from = numValue(num);
      numCommit(num, from, numSet(num, from + dir * c.step * mult, false));
      input.select();
    }
  };

  /* ---------- Slider ---------- */
  function sliderConfig(s) {
    const d = s.dataset;
    return {
      min: parseFloat(d.min || "0"),
      max: parseFloat(d.max || "100"),
      step: parseFloat(d.step || "1"),
      unit: d.unit || "",
      snaps: (d.snaps || "").split(",").filter(Boolean).map(Number)
    };
  }
  function sliderBuild(s) {
    if (s.querySelector(".slider-track")) return;
    const c = sliderConfig(s);
    const track = document.createElement("div");
    track.className = "slider-track";
    const fill = document.createElement("div");
    fill.className = "slider-fill";
    track.appendChild(fill);
    s.appendChild(track);
    c.snaps.forEach((v) => {
      const t = document.createElement("div");
      t.className = "slider-tick";
      t.dataset.at = String(v);
      s.appendChild(t);
    });
    const thumb = document.createElement("div");
    thumb.className = "slider-thumb";
    s.appendChild(thumb);
    s.setAttribute("role", "slider");
    if (!s.hasAttribute("tabindex")) s.tabIndex = 0;
    sliderSet(s, parseFloat(s.dataset.value || String(c.min)), false, true);
  }
  function sliderLayout(s) {
    const c = sliderConfig(s);
    const inset = 6;
    const w = s.clientWidth - inset * 2;
    return { c: c, inset: inset, w: w,
      px: (v) => inset + (v - c.min) / (c.max - c.min) * w,
      val: (px) => c.min + clamp((px - inset) / w, 0, 1) * (c.max - c.min) };
  }
  function sliderSet(s, v, live, quiet) {
    const L = sliderLayout(s);
    const q = Math.round(clamp(v, L.c.min, L.c.max) / L.c.step) * L.c.step;
    const val = parseFloat(q.toFixed(6));
    s.dataset.value = String(val);
    s.setAttribute("aria-valuemin", String(L.c.min));
    s.setAttribute("aria-valuemax", String(L.c.max));
    s.setAttribute("aria-valuenow", String(val));
    s.setAttribute("aria-valuetext", val + (L.c.unit ? " " + L.c.unit : ""));
    const x = L.px(val);
    s.querySelector(".slider-thumb").style.left = x + "px";
    s.querySelector(".slider-fill").style.width = (x - L.inset) + "px";
    s.querySelectorAll(".slider-tick").forEach((t) => {
      t.style.left = L.px(parseFloat(t.dataset.at)) + "px";
    });
    if (!quiet) emit(s, "kit:change", { value: val, live: Boolean(live) });
    return val;
  }
  function sliderDown(e, s) {
    const is_off = s.getAttribute("aria-disabled") === "true";
    if (is_off || e.button !== 0) return;
    const from = parseFloat(s.dataset.value);
    const on_thumb = Boolean(e.target.closest(".slider-thumb"));
    const sn = snapper();
    const ticks = Array.from(s.querySelectorAll(".slider-tick"));
    const rect = s.getBoundingClientRect();
    const L = sliderLayout(s);
    const grab_dx = on_thumb ? (e.clientX - rect.left) - L.px(from) : 0;
    let started = !on_thumb;
    let last_dx = 0;
    s.classList.add("is-down");
    s.focus();
    try { s.setPointerCapture(e.pointerId); } catch (err) { /* ignore */ }
    const at = (cx, free) => {
      const raw = L.val(cx - rect.left - grab_dx);
      if (free) { sn.reset(); return { value: raw, hit: null }; }
      const targets = ticks.map((t) => ({ at: L.px(parseFloat(t.dataset.at)), el: t }));
      const r = sn.test(L.px(raw), targets);
      return { value: r.hit ? L.val(r.value) : raw, hit: r.hit };
    };
    if (started) sliderSet(s, at(e.clientX, false).value, true);
    const move = (ev) => {
      const passed = Math.abs(ev.clientX - e.clientX) >= kDragThreshold;
      if (!started && passed) started = true;
      if (!started) return;
      last_dx = ev.movementX || last_dx;
      const r = at(ev.clientX, ev.altKey || ev.ctrlKey);
      const v = sliderSet(s, r.value, true);
      Chip.show(v + (L.c.unit ? " " + L.c.unit : ""), ev.clientX, ev.clientY, Boolean(r.hit));
    };
    const end = (cancelled) => {
      s.removeEventListener("pointermove", move);
      s.removeEventListener("pointerup", up);
      s.removeEventListener("pointercancel", cancel);
      window.removeEventListener("keydown", key, true);
      s.classList.remove("is-down");
      Chip.hide();
      Drag.active = null;
      if (cancelled) { sliderSet(s, from, false); return; }
      const to = parseFloat(s.dataset.value);
      bounce(s.querySelector(".slider-thumb"), last_dx, 0);
      const changed = to !== from;
      if (changed) emit(s, "kit:commit", { from: from, to: to, name: s.dataset.name || "slider" });
    };
    const up = () => end(false);
    const cancel = () => end(true);
    const key = (ev) => {
      const is_escape = ev.key === "Escape";
      if (!is_escape) return;
      ev.preventDefault();
      ev.stopPropagation();
      end(true);
    };
    s.addEventListener("pointermove", move);
    s.addEventListener("pointerup", up);
    s.addEventListener("pointercancel", cancel);
    window.addEventListener("keydown", key, true);
    Drag.active = { finish: end };
    e.preventDefault();
  }
  function sliderKey(e, s) {
    const dirs = { ArrowRight: 1, ArrowUp: 1, ArrowLeft: -1, ArrowDown: -1 };
    const dir = dirs[e.key];
    if (!dir) return;
    e.preventDefault();
    const c = sliderConfig(s);
    const from = parseFloat(s.dataset.value);
    const to = sliderSet(s, from + dir * c.step * (e.shiftKey ? 10 : 1), false);
    emit(s, "kit:commit", { from: from, to: to, name: s.dataset.name || "slider" });
  }

  /* ---------- Menus, menubar, context menu, combo ---------- */
  const Menus = {
    open_menu: null,
    opener: null,
    open(menu, opener, focusFirst) {
      if (this.open_menu === menu) return;
      this.close();
      menu.hidden = false;
      play(menu, "k-open", "pop");
      this.open_menu = menu;
      this.opener = opener || null;
      if (opener) {
        opener.setAttribute("aria-expanded", "true");
        this.anchor(menu, opener);
      }
      if (focusFirst) this.step(menu, 1);
    },
    /* Drop-downs are fixed to the opener so a scrolling box never
       clips them; they flip up and slide left to stay on screen. */
    anchor(menu, opener) {
      const r = opener.getBoundingClientRect();
      const vw = document.documentElement.clientWidth;
      const vh = document.documentElement.clientHeight;
      const is_combo = opener.classList.contains("combo-btn");
      menu.classList.add("is-fixed");
      menu.style.right = "auto";
      menu.style.minWidth = is_combo ? r.width + "px" : "";
      const w = menu.offsetWidth;
      const h = menu.offsetHeight;
      const fits_below = r.bottom + h <= vh - 4 || r.top - h < 4;
      menu.style.left = Math.round(clamp(r.left, 4, Math.max(4, vw - w - 4))) + "px";
      menu.style.top = Math.round(fits_below ? r.bottom : r.top - h) + "px";
      menu.style.transformOrigin = fits_below ? "top left" : "bottom left";
    },
    close() {
      const m = this.open_menu;
      if (!m) return;
      m.hidden = true;
      m.classList.remove("k-open");
      if (this.opener) this.opener.setAttribute("aria-expanded", "false");
      const is_ctx = m.classList.contains("is-ctx");
      if (is_ctx) m.remove();
      this.open_menu = null;
      this.opener = null;
    },
    step(menu, dir) {
      const items = Array.from(menu.querySelectorAll(".menu-item"));
      if (!items.length) return;
      const i = items.indexOf(document.activeElement);
      const next = i < 0 ? (dir > 0 ? 0 : items.length - 1)
        : (i + dir + items.length) % items.length;
      items[next].focus({ preventScroll: true });
    },
    choose(item) {
      const is_off = item.getAttribute("aria-disabled") === "true";
      if (is_off) { shake(item, item.dataset.why); return; }
      const menu = item.closest(".menu");
      const combo = menu.parentElement && menu.parentElement.classList.contains("combo")
        ? menu.parentElement : null;
      Sound.click();
      if (combo) { comboPick(combo, item); }
      if (item._kitRun) item._kitRun();
      emit(item, "kit:menu", { action: item.dataset.action || item.textContent.trim() });
      const opener = this.opener;
      this.close();
      if (opener) opener.focus();
    }
  };

  /* items: [{ label, key, why, run, sep, checked, head }] */
  function contextMenu(x, y, items) {
    const m = document.createElement("div");
    m.className = "menu is-fixed is-ctx";
    m.setAttribute("role", "menu");
    (items || []).forEach((it) => {
      if (it.sep) {
        const s = document.createElement("div");
        s.className = "menu-sep";
        m.appendChild(s);
        return;
      }
      if (it.head) {
        const h = document.createElement("div");
        h.className = "menu-head";
        h.textContent = it.head;
        m.appendChild(h);
        return;
      }
      const b = document.createElement("button");
      b.className = "menu-item";
      b.setAttribute("role", "menuitem");
      const l = document.createElement("span");
      l.textContent = it.label;
      b.appendChild(l);
      if (it.key) {
        const k = document.createElement("kbd");
        k.className = "menu-key";
        k.textContent = it.key;
        b.appendChild(k);
      }
      if (it.why) {
        b.setAttribute("aria-disabled", "true");
        b.dataset.why = it.why;
      }
      if (it.checked !== undefined) b.setAttribute("aria-checked", String(Boolean(it.checked)));
      if (it.action) b.dataset.action = it.action;
      b._kitRun = it.run || null;
      m.appendChild(b);
    });
    m.hidden = true;
    document.body.appendChild(m);
    m.style.left = "0px";
    m.style.top = "0px";
    Menus.open(m, null, false);
    const vw = document.documentElement.clientWidth;
    const vh = document.documentElement.clientHeight;
    const fits_right = x + m.offsetWidth <= vw - 4;
    const fits_below = y + m.offsetHeight <= vh - 4;
    m.style.left = Math.max(4, fits_right ? x : x - m.offsetWidth) + "px";
    m.style.top = Math.max(4, fits_below ? y : y - m.offsetHeight) + "px";
    m.style.transformOrigin = (fits_right ? "left " : "right ") + (fits_below ? "top" : "bottom");
    return m;
  }

  function comboPick(combo, item) {
    combo.querySelectorAll(".menu-item").forEach((i) => i.setAttribute("aria-checked", "false"));
    item.setAttribute("aria-checked", "true");
    const value = item.dataset.value || item.textContent.trim();
    const from = combo.dataset.value;
    combo.dataset.value = value;
    const label = combo.querySelector(".combo-value");
    label.textContent = item.querySelector("span") ? item.querySelector("span").textContent
      : item.textContent.trim();
    combo.classList.remove("is-error");
    play(label, "k-swap", "move");
    const changed = from !== value;
    if (changed) emit(combo, "kit:change", { value: value, from: from });
  }
  function comboStep(combo, dir) {
    const items = Array.from(combo.querySelectorAll(".menu-item:not([aria-disabled='true'])"));
    const i = items.findIndex((it) => it.getAttribute("aria-checked") === "true");
    const next = items[clamp(i + dir, 0, items.length - 1)];
    if (next && next !== items[i]) comboPick(combo, next);
  }

  /* ---------- Tabs ---------- */
  function tabsLayout(tabs) {
    let bar = tabs.querySelector(".tabs-bar");
    if (!bar) {
      bar = document.createElement("div");
      bar.className = "tabs-bar";
      tabs.appendChild(bar);
    }
    const on = tabs.querySelector('.tab[aria-selected="true"]');
    if (!on) { bar.style.width = "0"; return; }
    bar.style.left = (on.offsetLeft + 3) + "px";
    bar.style.width = (on.offsetWidth - 6) + "px";
  }
  function tabPick(tab) {
    const tabs = tab.closest(".tabs");
    const is_off = tab.getAttribute("aria-disabled") === "true";
    if (is_off) { shake(tab, tab.dataset.why); return; }
    tabs.querySelectorAll(".tab").forEach((t) => {
      const on = t === tab;
      t.setAttribute("aria-selected", String(on));
      t.tabIndex = on ? 0 : -1;
      const id = t.getAttribute("aria-controls");
      const panel = id ? document.getElementById(id) : null;
      if (!panel) return;
      panel.hidden = !on;
      if (on) play(panel, "k-enter", "move");
    });
    tabsLayout(tabs);
    emit(tab, "kit:tab", { tab: tab.dataset.tab || tab.textContent.trim() });
  }

  /* ---------- Dock and fold ---------- */
  function dockSet(dock, collapsed, remember) {
    dock.classList.toggle("is-collapsed", collapsed);
    const t = dock.querySelector(".dock-toggle");
    if (t) t.setAttribute("aria-expanded", String(!collapsed));
    const id = dock.dataset.dockId;
    if (id && remember !== false) store("snapper.dock." + id, collapsed ? "1" : "0");
  }

  /* ---------- Dialog (native dialog element) ---------- */
  const Dialog = {
    open(dlg) {
      if (!dlg || dlg.open) return;
      if (typeof dlg.showModal === "function") dlg.showModal();
      else dlg.setAttribute("open", "");
      play(dlg, "k-open", "pop");
      const def = dlg.querySelector("[autofocus], .btn.primary");
      if (def) def.focus();
    },
    close(dlg, value) {
      if (!dlg || !dlg.open) return;
      dlg.returnValue = value || "";
      play(dlg, "k-leave", "puff", () => {
        if (typeof dlg.close === "function") dlg.close(value || "");
        else dlg.removeAttribute("open");
      });
    }
  };

  /* ---------- Undo stack for demos ---------- */
  class UndoStack {
    constructor(limit) {
      this.limit = limit || 100;
      this.done = [];
      this.undone = [];
      this.listeners = [];
    }
    /* Run doFn now and remember how to take it back. */
    run(name, doFn, undoFn) {
      doFn();
      this.push(name, undoFn, doFn);
    }
    /* Record a step that already happened (a drag that just ended). */
    push(name, undoFn, redoFn) {
      this.done.push({ name: name, undo: undoFn, redo: redoFn });
      while (this.done.length > this.limit) this.done.shift();
      this.undone.length = 0;
      this.changed("push");
    }
    undo() {
      const step = this.done.pop();
      if (!step) return null;
      step.undo();
      this.undone.push(step);
      this.changed("undo");
      return step.name;
    }
    redo() {
      const step = this.undone.pop();
      if (!step) return null;
      step.redo();
      this.done.push(step);
      this.changed("redo");
      return step.name;
    }
    get undoName() { return this.done.length ? this.done[this.done.length - 1].name : ""; }
    get redoName() { return this.undone.length ? this.undone[this.undone.length - 1].name : ""; }
    onChange(fn) { this.listeners.push(fn); fn("init", this); }
    changed(kind) { this.listeners.forEach((fn) => fn(kind, this)); }
    bindButtons(undoBtn, redoBtn) {
      const paint = (btn, verb, name, why, animate) => {
        if (!btn) return;
        const text = name ? verb + " " + name : verb;
        const changed_text = btn.textContent !== text;
        btn.textContent = "";
        const span = document.createElement("span");
        span.textContent = text;
        btn.appendChild(span);
        if (changed_text && animate) play(span, "k-swap", "move");
        btn.setAttribute("aria-disabled", String(!name));
        btn.dataset.why = why;
      };
      if (undoBtn) undoBtn.addEventListener("click", () => this.undo());
      if (redoBtn) redoBtn.addEventListener("click", () => this.redo());
      this.onChange((kind, st) => {
        const animate = kind !== "init";
        paint(undoBtn, "Undo", st.undoName, "Nothing to undo yet.", animate);
        paint(redoBtn, "Redo", st.redoName, "Nothing to redo.", animate);
      });
    }
    bindKeys(target) {
      (target || document).addEventListener("keydown", (e) => {
        const typing = e.target.closest && e.target.closest("input, textarea");
        const mod = e.ctrlKey || e.metaKey;
        if (typing || !mod) return;
        const k = e.key.toLowerCase();
        const is_redo = k === "y" || (k === "z" && e.shiftKey);
        const is_undo = k === "z" && !e.shiftKey;
        if (is_undo) { e.preventDefault(); this.undo(); }
        if (is_redo) { e.preventDefault(); this.redo(); }
      });
    }
  }

  /* ---------- Press feedback: at least one frame of the down state ---------- */
  const kPressSel = ".btn, .tool, .tab, .seg > button, .win-btn, .toggle, .combo-btn";
  function pressDown(el) {
    el.classList.add("is-down");
    el._kitDownAt = performance.now();
    Sound.click();
  }
  function pressUp(el) {
    const held = performance.now() - (el._kitDownAt || 0);
    const wait = Math.max(0, 50 - held);
    window.setTimeout(() => {
      window.requestAnimationFrame(() => el.classList.remove("is-down"));
    }, wait);
  }

  /* ---------- Delegated wiring ---------- */
  function wire() {
    const doc = document;

    doc.addEventListener("pointerdown", (e) => {
      const t = e.target;
      const off = t.closest('[aria-disabled="true"]');
      if (off) {
        e.preventDefault();
        return;
      }
      const label = t.closest(".num-label");
      if (label && label.closest(".num")) { Scrub.start(e, label.closest(".num")); return; }
      const slider = t.closest(".slider");
      if (slider) { sliderDown(e, slider); return; }
      const press = t.closest(kPressSel);
      const is_main = e.button === 0;
      if (press && is_main) {
        pressDown(press);
        const up = () => {
          pressUp(press);
          window.removeEventListener("pointerup", up, true);
          window.removeEventListener("pointercancel", up, true);
        };
        window.addEventListener("pointerup", up, true);
        window.addEventListener("pointercancel", up, true);
      }
      const in_menu = t.closest(".menu, .menubar-item, .combo-btn");
      if (!in_menu) Menus.close();
      Tip.hide();
    }, true);

    /* A click on a control that cannot act: shake and say why. */
    doc.addEventListener("click", (e) => {
      const off = e.target.closest('[aria-disabled="true"]');
      if (!off) return;
      e.preventDefault();
      e.stopImmediatePropagation();
      shake(off, off.dataset.why);
    }, true);

    doc.addEventListener("click", (e) => {
      const t = e.target;
      const motion = t.closest("[data-kit-motion]");
      if (motion) { Motion.set(motion.dataset.kitMotion); return; }
      const sound = t.closest("[data-kit-sound]");
      if (sound) { Sound.set(!Sound.enabled); return; }
      const toggle = t.closest(".toggle");
      if (toggle) {
        const on = toggle.getAttribute("aria-pressed") !== "true";
        toggle.setAttribute("aria-pressed", String(on));
        emit(toggle, "kit:change", { value: on });
        return;
      }
      const seg = t.closest(".seg > button");
      if (seg && !seg.hasAttribute("data-kit-motion")) {
        seg.parentElement.querySelectorAll("button").forEach((b) => {
          b.setAttribute("aria-pressed", String(b === seg));
        });
        emit(seg, "kit:change", { value: seg.dataset.value || seg.textContent.trim() });
        return;
      }
      const tool = t.closest(".tool[aria-pressed]");
      if (tool) {
        const on = tool.getAttribute("aria-pressed") !== "true";
        const group = tool.dataset.group;
        if (group) {
          doc.querySelectorAll('.tool[data-group="' + group + '"]').forEach((b) => {
            b.setAttribute("aria-pressed", String(b === tool));
          });
        } else {
          tool.setAttribute("aria-pressed", String(on));
        }
        emit(tool, "kit:change", { value: on });
        return;
      }
      const tab = t.closest(".tab");
      if (tab) { tabPick(tab); return; }
      const dockT = t.closest(".dock-toggle");
      if (dockT) {
        const dock = dockT.closest(".dock");
        dockSet(dock, !dock.classList.contains("is-collapsed"));
        return;
      }
      const mbi = t.closest(".menubar-item");
      if (mbi) {
        const menu = mbi.parentElement.querySelector(".menu");
        const is_open = Menus.open_menu === menu;
        if (is_open) Menus.close();
        else Menus.open(menu, mbi, e.detail === 0);
        return;
      }
      const cb = t.closest(".combo-btn");
      if (cb) {
        const menu = cb.parentElement.querySelector(".menu");
        const is_open = Menus.open_menu === menu;
        if (is_open) { Menus.close(); return; }
        Menus.open(menu, cb, false);
        const cur = menu.querySelector('[aria-checked="true"]') || menu.querySelector(".menu-item");
        if (cur) cur.focus({ preventScroll: true });
        return;
      }
      const item = t.closest(".menu-item");
      if (item) { Menus.choose(item); return; }
      const opener = t.closest("[data-open-dialog]");
      if (opener) { Dialog.open(doc.getElementById(opener.dataset.openDialog)); return; }
      const closer = t.closest("dialog [data-close]");
      if (closer) {
        e.preventDefault();
        Dialog.close(closer.closest("dialog"), closer.dataset.close);
      }
    });

    doc.addEventListener("cancel", (e) => {
      const dlg = e.target;
      const is_kit = dlg.classList && dlg.classList.contains("dialog");
      if (!is_kit) return;
      e.preventDefault();
      Dialog.close(dlg, "cancel");
    }, true);

    /* Hover a menubar while one menu is open: switch to that menu. */
    doc.addEventListener("pointerover", (e) => {
      const t = e.target;
      const mbi = t.closest(".menubar-item");
      const in_bar_open = mbi && Menus.open_menu && Menus.opener &&
        Menus.opener.classList.contains("menubar-item");
      if (in_bar_open) Menus.open(mbi.parentElement.querySelector(".menu"), mbi, false);
      const item = t.closest(".menu-item");
      if (item) item.focus({ preventScroll: true });
      const tipped = t.closest("[data-tip], [data-why]");
      if (tipped && !Drag.active) Tip.over(tipped);
    });
    doc.addEventListener("pointerout", (e) => {
      const tipped = e.target.closest("[data-tip], [data-why]");
      const leaving = tipped && !tipped.contains(e.relatedTarget);
      if (leaving) Tip.hide();
    });
    doc.addEventListener("focusin", (e) => {
      const tipped = e.target.closest && e.target.closest("[data-tip], [data-why]");
      const is_keyboard = tipped && tipped.matches(":focus-visible");
      if (is_keyboard) Tip.over(tipped);
    });
    doc.addEventListener("focusout", () => Tip.hide());
    window.addEventListener("scroll", (e) => {
      Tip.hide();
      document.querySelectorAll(".why").forEach((b) => b.remove());
      const in_menu = e.target.closest && e.target.closest(".menu");
      const m = Menus.open_menu;
      if (in_menu || !m) return;
      if (Menus.opener) Menus.anchor(m, Menus.opener);
      else Menus.close();
    }, true);

    doc.addEventListener("keydown", (e) => {
      const t = e.target;
      const num = t.closest && t.closest(".num");
      const is_num_input = num && t.classList.contains("num-input");
      if (is_num_input) { Scrub.key(e, num); return; }
      const slider = t.closest && t.closest(".slider");
      if (slider) { sliderKey(e, slider); return; }
      const menu = Menus.open_menu;
      if (menu) {
        if (e.key === "Escape") {
          e.preventDefault();
          const opener = Menus.opener;
          Menus.close();
          if (opener) opener.focus();
          return;
        }
        if (e.key === "ArrowDown") { e.preventDefault(); Menus.step(menu, 1); return; }
        if (e.key === "ArrowUp") { e.preventDefault(); Menus.step(menu, -1); return; }
      }
      const combo_btn = t.closest && t.closest(".combo-btn");
      const is_combo_arrow = combo_btn && !menu && (e.key === "ArrowDown" || e.key === "ArrowUp");
      if (is_combo_arrow) {
        e.preventDefault();
        comboStep(combo_btn.parentElement, e.key === "ArrowDown" ? 1 : -1);
        return;
      }
      const tab = t.closest && t.closest(".tab");
      const is_tab_arrow = tab && (e.key === "ArrowRight" || e.key === "ArrowLeft");
      if (is_tab_arrow) {
        const all = Array.from(tab.closest(".tabs").querySelectorAll(".tab"));
        const i = all.indexOf(tab) + (e.key === "ArrowRight" ? 1 : -1);
        const next = all[(i + all.length) % all.length];
        next.focus();
        tabPick(next);
        return;
      }
      const press = t.closest && t.closest(kPressSel);
      const is_press_key = press && (e.key === " " || e.key === "Enter") && !e.repeat;
      if (is_press_key) pressDown(press);
    });
    doc.addEventListener("keyup", (e) => {
      const press = e.target.closest && e.target.closest(kPressSel);
      if (press) pressUp(press);
    });

    doc.addEventListener("focusin", (e) => {
      const input = e.target;
      const is_num = input.classList && input.classList.contains("num-input");
      if (is_num) window.setTimeout(() => input.select(), 0);
    });
    doc.addEventListener("focusout", (e) => {
      const input = e.target;
      const is_num = input.classList && input.classList.contains("num-input");
      if (!is_num) return;
      const num = input.closest(".num");
      const c = numConfig(num);
      const shown = fmt(numValue(num), c.decimals);
      const edited = input.value !== shown && !(input.value === "" && num.hasAttribute("data-mixed"));
      if (edited) Scrub.typed(num);
    });

    doc.addEventListener("change", (e) => {
      const box = e.target.closest(".check");
      if (box) pop(box.querySelector(".check-box"), 1.3);
    });

    window.addEventListener("resize", () => {
      doc.querySelectorAll(".tabs").forEach(tabsLayout);
      doc.querySelectorAll(".slider").forEach((s) => sliderSet(s, parseFloat(s.dataset.value), false, true));
      Menus.close();
    });
  }

  /* Build what needs building inside root: sliders, tab bars, docks,
     number fields. Safe to call again on new markup. */
  function init(root) {
    const r = root || document;
    r.querySelectorAll(".slider").forEach(sliderBuild);
    r.querySelectorAll(".tabs").forEach(tabsLayout);
    r.querySelectorAll(".dock[data-dock-id]").forEach((d) => {
      const saved = store("snapper.dock." + d.dataset.dockId);
      if (saved !== null) dockSet(d, saved === "1", false);
    });
    r.querySelectorAll(".dock").forEach((d) => {
      const t = d.querySelector(".dock-toggle");
      if (t) t.setAttribute("aria-expanded", String(!d.classList.contains("is-collapsed")));
    });
    r.querySelectorAll(".num").forEach((num) => {
      const input = num.querySelector(".num-input");
      if (!input) return;
      input.setAttribute("inputmode", "decimal");
      input.setAttribute("autocomplete", "off");
      const is_mixed = num.hasAttribute("data-mixed");
      if (is_mixed) { input.value = ""; input.placeholder = "Mixed"; return; }
      const has_value = num.dataset.value !== undefined;
      if (!has_value) num.dataset.value = input.value || "0";
      input.value = fmt(numValue(num), numConfig(num).decimals);
      const is_off = num.getAttribute("aria-disabled") === "true";
      if (is_off) input.readOnly = true;
    });
    r.querySelectorAll("[data-kit-motion]").forEach((b) => {
      b.setAttribute("aria-pressed", String(b.dataset.kitMotion === Motion.mode));
    });
    r.querySelectorAll("[data-kit-sound]").forEach((b) => {
      b.setAttribute("aria-pressed", String(Sound.enabled));
    });
  }

  Motion.init();
  window.Kit = {
    motion: Motion,
    sound: Sound,
    ms: (name) => Motion.ms(name),
    play: play,
    pop: pop,
    puff: puff,
    flash: flash,
    bounce: bounce,
    ring: ring,
    shake: shake,
    toast: toast,
    chip: Chip,
    tip: Tip,
    drag: drag,
    ghost: makeGhost,
    snap: snap,
    snapper: snapper,
    nearest: nearest,
    hoverOutline: hoverOutline,
    evalMath: evalMath,
    num: { set: numSet, value: numValue },
    slider: { set: sliderSet, build: sliderBuild },
    menus: Menus,
    contextMenu: contextMenu,
    tabs: { pick: tabPick, layout: tabsLayout },
    dock: { set: dockSet },
    dialog: Dialog,
    UndoStack: UndoStack,
    init: init,
    consts: { dragThreshold: kDragThreshold, snapCatch: kSnapCatch,
      chipOffset: kChipOffset, toastMs: kToastMs, motion: kMotionTable }
  };

  const start = () => {
    Sound.init();
    wire();
    init(document);
    const fonts = document.fonts;
    if (fonts && fonts.ready) fonts.ready.then(() => document.querySelectorAll(".tabs").forEach(tabsLayout));
  };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start);
  else start();
})();
