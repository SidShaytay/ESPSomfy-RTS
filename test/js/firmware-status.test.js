const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const cls = source.slice(source.indexOf('class Firmware {'), source.indexOf('var firmware = new Firmware();'));
function fixture() {
    const requests = [], timers = new Map(), instructions = { textContent: '' }, listeners = [];
    let timer = 0, refused = 0;
    const context = { console: { log() {} }, baseUrl: '', isBundledWeb: () => true, chipModel: () => 's3',
        security: { apiKey: 'test-token', tokenRefused() { ++refused; } },
        setTimeout(fn) { const id = ++timer; timers.set(id, fn); return id; }, clearTimeout(id) { timers.delete(id); } };
    context.XMLHttpRequest = class {
        constructor() { requests.push(this); this.upload = {}; }
        open(method, url) { this.method = method; this.url = url; }
        setRequestHeader(name, value) { this.header = [name, value]; }
        send() {} abort() { this.aborted = true; }
    };
    context.FormData = class { append() {} };
    const Firmware = vm.runInNewContext(cls + '\nFirmware', context), firmware = new Firmware();
    context.firmware = firmware; firmware.isMobile = () => true;
    const field = { value: 'firmware.bin', files: [{ size: 1024 }] };
    const widget = { style: { setProperty() {} }, addEventListener(_, fn) { listeners.push(fn); } };
    const el = { querySelector(selector) { return selector.startsWith('input') ? field : selector.includes('divInstText') ? instructions : widget; } };
    const identity = { operationGeneration: '9007199254740993' };
    const start = () => firmware.watchFirmware(el, identity);
    const reply = (data, code = 200) => {
        const req = requests.at(-1); req.status = code;
        req.responseText = JSON.stringify({ ...identity, source: 'manual-file', busy: true, ...data }); req.onload();
    };
    const next = () => { const [id, fn] = timers.entries().next().value; timers.delete(id); fn(); };
    return { requests, timers, instructions, listeners, identity, firmware, el, start, reply, next, get refused() { return refused; } };
}
test('firmware observes exact public operation; received/written never announce successful install', async () => {
    const h = fixture(); await h.firmware.uploadFile('/updateFirmware', h.el, {});
    assert.equal(h.requests[0].method, 'POST');
    h.reply({ status: 'QUEUED' }, 202);
    assert.equal(h.requests.length, 2); assert.equal(h.requests[1].method, 'GET');
    assert.match(h.requests[1].url, /9007199254740993/); assert.deepEqual(h.requests[1].header, ['apikey', 'test-token']);
    h.reply({ state: 'verifying' }); assert.match(h.instructions.textContent, /verif/i); h.next();
    h.reply({ state: 'writing', progress: 61 }); assert.match(h.instructions.textContent, /61%/); h.next();
    h.reply({ state: 'written', restartScheduled: false }); assert.equal(h.timers.size, 1); h.next();
    h.reply({ state: 'written', restartScheduled: true }); assert.equal(h.timers.size, 0);
    assert.match(h.instructions.textContent, /scheduled/i); assert.match(h.instructions.textContent, /verify/i);
    assert.doesNotMatch(h.instructions.textContent, /success|installed successfully/i);
});
test('stale status, changed source, bad identity and refused auth cannot observe another installation', () => {
    for (const kind of ['stale', 'operation', 'source', 'auth', 'phase']) {
        const h = fixture(); h.start();
        if (kind === 'stale') h.reply({}, 409);
        else if (kind === 'auth') h.reply({}, 401);
        else if (kind === 'operation') h.reply({ state: 'writing', operationGeneration: '9007199254740994' });
        else if (kind === 'source') h.reply({ state: 'writing', source: 'other' });
        else h.reply({ state: 'idle' });
        assert.equal(h.timers.size, 0); assert.equal(h.requests.length, 1);
        assert.equal(h.refused, kind === 'auth' ? 1 : 0);
    }
    for (const operationGeneration of ['0', '01', '18446744073709551616', 9007199254740993]) {
        const h = fixture(); h.firmware.watchFirmware(h.el, { operationGeneration }); assert.equal(h.requests.length, 0);
    }
});
test('firmware faults and connection loss report unconfirmed state without automatic replay', () => {
    for (const state of ['error', 'cancelled', 'cleanupPending']) {
        const h = fixture(); h.start(); h.reply({ state, error: 'Write refused', busy: state === 'cleanupPending' });
        assert.match(h.instructions.textContent, /Write refused/); assert.equal(h.timers.size, 0); assert.equal(h.requests.length, 1);
    }
    const h = fixture(); h.start(); h.requests[0].onerror();
    assert.match(h.instructions.textContent, /not confirmed|unconfirmed/i); assert.equal(h.requests.length, 1);
});
test('Close stops observation only; firmware polling is bounded', () => {
    const h = fixture(); h.start(); h.listeners[0](); assert.equal(h.requests[0].aborted, true);
    const b = fixture(); b.start();
    for (let i = 0; i < 360; ++i) { b.reply({ state: 'queued' }); if (b.timers.size) b.next(); }
    assert.equal(b.requests.length, 360); assert.equal(b.timers.size, 0);
    assert.match(b.instructions.textContent, /not confirmed|unconfirmed/i);
});
