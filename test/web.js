// Checks for web/index.html, which has no build step and no linter.
//   node test/web.js
//
// Two things, both of which are otherwise only discoverable on the phone:
//
//   1. The script's top level evaluates against DOM stubs. Catches undeclared
//      identifiers and getElementById typos.
//   2. Every GATT write goes through one gate. Web Bluetooth allows exactly one
//      operation at a time per device, so a control write landing while the mic
//      is streaming throws "GATT operation already in progress" on both.
//   3. All three bands respond, and spectral tilt tracks where the energy sits.
//      Both were broken at once: a shared absolute noise floor pinned the high
//      band to zero, and tilt was computed from auto-gained levels that cannot
//      tell a sub-heavy track from a bright one.
const fs = require('fs');
const path = require('path');
const assert = require('assert');

const html = fs.readFileSync(path.join(__dirname, '..', 'web', 'index.html'), 'utf8');
const scripts = html.match(/<script>([\s\S]*)<\/script>/);
assert.ok(scripts, 'no <script> block found in web/index.html');
const js = scripts[1];

// --- Stubs -----------------------------------------------------------------
const seen = new Set();
const stubEl = () => ({
  style: {}, dataset: {}, textContent: '', value: '#000000',
  checked: false, disabled: false,
  classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
  addEventListener() {}, querySelectorAll: () => [],
});
global.document = {
  getElementById(id) { seen.add(id); return stubEl(); },
  querySelectorAll(sel) { seen.add(sel); return []; },
};
global.navigator = {
  serviceWorker: { register: () => Promise.reject(new Error('stub')) },
  bluetooth: {},
};
global.window = global;
global.AudioContext = function () {};
global.requestAnimationFrame = () => 0;
global.cancelAnimationFrame = () => {};

// --- 1. Top level ----------------------------------------------------------
let api;
try {
  api = new Function(js + '\nreturn { gattWrite, gattStream, makeBandTracker, BANDS };')();
} catch (err) {
  console.error(`FAIL - script top level threw ${err.constructor.name}: ${err.message}`);
  process.exit(1);
}
console.log('PASS - script top level evaluates');

for (const id of seen) {
  if (id.startsWith('.')) {
    assert.ok(html.includes(`class="${id.slice(1)}`), `no element with class ${id}`);
  } else {
    const n = html.split(`id="${id}"`).length - 1;
    assert.strictEqual(n, 1, `id "${id}" appears ${n} times in the markup, expected 1`);
  }
}
console.log(`PASS - all ${seen.size} referenced ids and selectors exist in the markup`);

// --- 2. GATT serialisation --------------------------------------------------
let inFlight = 0, maxInFlight = 0;
const landed = [];
const mkChar = (name) => ({
  writeValue(bytes) {
    inFlight++;
    maxInFlight = Math.max(maxInFlight, inFlight);
    return new Promise(res => setTimeout(() => { landed.push(`${name}:${bytes[0]}`); inFlight--; res(); }, 5));
  },
  writeValueWithoutResponse(bytes) { return this.writeValue(bytes); },
});
const ctrl = mkChar('ctrl'), audio = mkChar('audio');

// --- 3. Band response and spectral tilt ------------------------------------
// getByteFrequencyData maps dB onto 0..255 across its default -100..-30 window.
const dbToByte = db => Math.max(0, Math.min(255, Math.round(255 * (db + 100) / 70)));

// Built from the app's own BANDS, so retuning them can't silently invalidate
// these tests. Everything outside a band is floor.
function spectrum(bassDb, midDb, highDb) {
  const bins = new Uint8Array(1024).fill(dbToByte(-95));
  [bassDb, midDb, highDb].forEach((db, b) => {
    const [lo, hi] = api.BANDS[b];
    for (let j = lo; j <= hi; j++) bins[j] = dbToByte(db);
  });
  return bins;
}

// Play `frames` of a spectrum that alternates loud/quiet, as music does, so the
// per-band floor and peak have something to separate around.
function play(track, frames, loudDb, quietDb) {
  let last, peakLevel = [0, 0, 0];
  for (let i = 0; i < frames; i++) {
    const d = i % 20 < 10 ? loudDb : quietDb;
    last = track(spectrum(d[0], d[1], d[2]), 16);
    if (i > frames - 100) last.level.forEach((v, b) => { peakLevel[b] = Math.max(peakLevel[b], v); });
  }
  return { last, peakLevel };
}

{
  const track = api.makeBandTracker();
  // Bass-heavy, with the high band ~26dB down — a realistic bass-music spectrum.
  const { peakLevel } = play(track, 600, [-32, -45, -58], [-50, -62, -75]);
  const names = ['bass', 'mid', 'high'];
  peakLevel.forEach((v, i) => {
    assert.ok(v > 0.5, `${names[i]} band peaked at only ${v.toFixed(2)} — band is dead`);
  });
  console.log(`PASS - all three bands respond (peaks ${peakLevel.map(v => v.toFixed(2)).join(' / ')})`);
}

{
  // Tilt must tell a bright passage from a dark one. The old level-based tilt
  // could not: auto-gain normalises every band to its own range, so both
  // passages peg bass and high alike.
  const track = api.makeBandTracker();
  play(track, 300, [-40, -48, -62], [-55, -63, -77]);          // neutral baseline
  const bright = play(track, 300, [-40, -46, -46], [-55, -61, -61]).last;
  assert.ok(bright.tilt > 0.3, `bright passage gave tilt ${bright.tilt.toFixed(2)}, expected > 0.3`);

  const dark = play(track, 300, [-34, -50, -72], [-49, -65, -87]).last;
  assert.ok(dark.tilt < -0.3, `dark passage gave tilt ${dark.tilt.toFixed(2)}, expected < -0.3`);

  console.log(`PASS - tilt tracks spectral balance (bright ${bright.tilt.toFixed(2)}, dark ${dark.tilt.toFixed(2)})`);
}

{
  // The measured reality: the top band moves only a few dB while bass swings
  // 30. It still has to produce something — a band that reads exactly zero is
  // how the whole texture feature came to be dead.
  const track = api.makeBandTracker();
  // 1.5dB of swing, matching the 5-10kHz regions in the real measurement. That
  // is under MIN_SPAN, so this is precisely the case a hard gate zeroed.
  const { peakLevel } = play(track, 600, [-40, -60, -85.0], [-70, -75, -86.5]);
  assert.ok(peakLevel[2] > 0.25,
    `a 1.5dB-swinging top band produced only ${peakLevel[2].toFixed(2)} — a hard gate would zero it`);
  assert.ok(peakLevel[0] > 0.5, `bass should be strong, got ${peakLevel[0].toFixed(2)}`);
  console.log(`PASS - a 1.5dB top band still registers (${peakLevel[2].toFixed(2)}) beside a 30dB bass (${peakLevel[0].toFixed(2)})`);
}

(async () => {
  // A burst of user actions while audio streams every frame — the collision
  // this gate exists to prevent.
  for (let i = 0; i < 5; i++) {
    api.gattWrite(ctrl, new Uint8Array([i]));
    api.gattStream(audio, new Uint8Array([100 + i]));
  }
  const spam = setInterval(() => api.gattStream(audio, new Uint8Array([200])), 1);
  await new Promise(r => setTimeout(r, 300));
  clearInterval(spam);

  const ctrlLanded = landed.filter(x => x.startsWith('ctrl:'));
  const audioLanded = landed.filter(x => x.startsWith('audio:'));

  assert.strictEqual(maxInFlight, 1, `${maxInFlight} GATT operations overlapped`);
  console.log('PASS - no two GATT operations overlap');

  assert.deepStrictEqual(ctrlLanded, ['ctrl:0', 'ctrl:1', 'ctrl:2', 'ctrl:3', 'ctrl:4'],
    `control writes must all land in order, got ${ctrlLanded}`);
  console.log('PASS - control writes queue, none dropped, order kept');

  // ~300 attempts against a 5ms link. Dropping is the point: queueing them
  // would build the unbounded backlog the gate exists to avoid.
  assert.ok(audioLanded.length > 0, 'audio never got through');
  assert.ok(audioLanded.length < 100, `audio queued instead of dropping (${audioLanded.length} landed)`);
  console.log(`PASS - audio drops rather than queues (${audioLanded.length} landed of ~300)`);
})();
