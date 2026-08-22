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
  api = new Function(js + '\nreturn { gattWrite, gattStream };')();
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
