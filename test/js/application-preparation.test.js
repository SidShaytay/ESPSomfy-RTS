const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const cls = source.slice(source.indexOf('class Firmware {'), source.indexOf('var firmware = new Firmware();'));
const settle = () => new Promise(resolve => setImmediate(resolve));
function fixture() {
    const requests = [], timers = [], errors = [], nodes = {};
    function node() { return { textContent: '', style: { setProperty() {} }, handlers: {}, children: [],
        addEventListener(name, fn) { (this.handlers[name] ||= []).push(fn); },
        click() { for (const fn of this.handlers.click || []) fn(); },
        appendChild(child) { this.children.push(child); }, setAttribute() {}, remove() { this.removed = true; } }; }
    const el = node();
    el.querySelector = selector => nodes[selector] ||= node();
    const file = el.querySelector('input[type="file"]'); file.value = 'web.littlefs.bin'; file.files = [{ size: 4096 }];
    class XHR {
        constructor() { requests.push(this); this.headers = {}; }
        open(method, path) { this.method = method; this.path = path; }
        setRequestHeader(name, value) { this.headers[name] = value; }
        send(body) { this.body = body; }
        abort() { this.aborted = true; if (this.onabort) this.onabort(); }
        respond(status, body) { this.status = status; this.responseText = JSON.stringify(body); if (this.onload) this.onload(); }
        upload = {};
    }
    const context = { XMLHttpRequest: XHR, FormData: class { entries = []; append(...value) { this.entries.push(value); } },
        setTimeout(fn) { timers.push(fn); return fn; }, clearTimeout(fn) { const i = timers.indexOf(fn); if (i >= 0) timers.splice(i, 1); },
        baseUrl: '', security: { apiKey: 'fixture-key', tokenRefused() {} }, isBundledWeb: () => false,
        document: { createElement: node }, console: { log() {} },
        ui: { errorMessage(message) { errors.push(message); const n = node(); n.querySelector = () => node(); return n; }, serviceError(...value) { errors.push(value); }, infoMessage() {} } };
    const Firmware = vm.runInNewContext(cls + '\nFirmware', context), firmware = new Firmware(); context.firmware = firmware;
    firmware.isMobile = () => true;
    const identity = { nonce: '18446744073709551615', generation: '7', bytes: 4096 };
    return { firmware, requests, timers, errors, el, identity, close: el.querySelector('button[id="btnClose"]'), instructions: el.querySelector('div[id="divInstText"]') };
}
test('browser sends the file only after exact preparation and observes recovery without replay', async () => {
    const h = fixture(), done = h.firmware.uploadFile('/updateApplication', h.el, {});
    assert.equal(h.requests.length, 1); assert.match(h.requests[0].path, /^\/prepareApplication\?bytes=4096$/);
    h.requests[0].respond(202, { ...h.identity, phase: 'queued' }); await settle();
    assert.equal(h.requests.length, 2); assert.match(h.requests[1].path, /^\/applicationUpdateStatus\?/);
    assert.ok(h.requests.every(r => !r.body));
    h.requests[1].respond(200, { ...h.identity, phase: 'ready' }); await done;
    assert.equal(h.requests.length, 3); const upload = h.requests[2];
    assert.match(upload.path, /^\/updateApplication\?nonce=18446744073709551615&generation=7$/);
    assert.equal(upload.body.entries.length, 1); assert.equal(upload.headers.apikey, 'fixture-key');
    upload.respond(202, { ...h.identity, phase: 'recovering', uploaded: true }); await settle();
    assert.match(h.requests[3].path, /^\/applicationUpdateStatus\?/);
    h.requests[3].respond(200, { ...h.identity, phase: 'completed', desc: 'Saved current state. Restart scheduled.' }); await settle();
    assert.equal(h.instructions.textContent, 'Saved current state. Restart scheduled.');
    assert.equal(h.requests.filter(r => r.body).length, 1); assert.equal(h.timers.length, 0);
    h.close.click(); assert.equal(h.requests.length, 4);
});
test('closing while preparation is pending cancels only that source and sends no file', async () => {
    const h = fixture(), done = h.firmware.uploadFile('/updateApplication', h.el, {});
    h.requests[0].respond(202, { ...h.identity, phase: 'queued' }); await settle();
    h.close.click(); await done;
    assert.ok(h.requests.some(r => r.method === 'POST' && r.path.includes('/applicationUpdate?') && r.path.endsWith('&action=cancel')));
    assert.ok(h.requests.every(r => !r.body));
});
test('stale preparation identity cannot admit a file', async () => {
    const h = fixture(), done = h.firmware.uploadFile('/updateApplication', h.el, {});
    h.requests[0].respond(202, { ...h.identity, phase: 'queued' }); await settle();
    h.requests[1].respond(200, { ...h.identity, generation: '8', phase: 'ready' }); await done;
    assert.ok(h.requests.every(r => !r.body)); assert.ok(h.errors.length);
});
test('attention offers reducing recovery only after an explicit click', async () => {
    const h = fixture(); h.firmware.watchApplication(h.el, h.identity); await settle();
    h.requests[0].respond(200, { ...h.identity, phase: 'attention', desc: 'Recovery needs attention.' }); await settle();
    assert.equal(h.requests.length, 1); const retry = h.el.children.find(n => n.textContent === 'Retry recovery'); assert.ok(retry);
    retry.click(); await settle(); assert.match(h.requests[1].path, /&action=retry$/);
    h.requests[1].respond(202, { ...h.identity, phase: 'recovering' }); await settle();
    h.requests[2].respond(200, { ...h.identity, phase: 'completed', restartScheduled: false, desc: 'State saved. No restart scheduled.' }); await settle();
    assert.equal(h.instructions.textContent, 'State saved. No restart scheduled.');
    assert.ok(h.requests.every(r => !r.body && !r.path.startsWith('/updateApplication?')));
});

test('invalid or rounded preparation identities never send a file', async () => {
    for (const nonce of ['01', '18446744073709551616', 123]) {
        const h = fixture(), done = h.firmware.uploadFile('/updateApplication', h.el, {});
        h.requests[0].respond(202, { ...h.identity, nonce, phase: 'ready' }); await done;
        assert.ok(h.requests.every(r => !r.body)); assert.ok(h.errors.length);
    }
});
test('lost status stops observation without resending or restarting', async () => {
    const h = fixture(); h.firmware.watchApplication(h.el, h.identity); await settle();
    h.requests[0].ontimeout(); await settle();
    assert.equal(h.requests.length, 1); assert.equal(h.timers.length, 0);
    assert.match(h.instructions.textContent, /not confirmed/);
});
