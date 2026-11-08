const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const { ProtoViews } = require('../../data-src/js/views.js');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const editCode = source.slice(source.indexOf('var modelSaveWarning = null;'), source.indexOf("// A command's repeat"));
function setup(outcome) {
    const calls = [], order = [], boxes = [];
    const node = () => ({ attrs: {}, children: [], handlers: {}, inner: {}, removed: false,
        setAttribute(k, v) { this.attrs[k] = v; }, getAttribute(k) { return this.attrs[k]; },
        querySelector() { return this.inner; }, appendChild(n) { this.children.push(n); },
        addEventListener(k, cb) { this.handlers[k] = cb; }, remove() { this.removed = true; } });
    const context = { document: { getElementById() { return {}; }, createElement: node },
        proto: { waitFor(name, match) { order.push('event'); assert.equal(match({ roomId: 7 }), true); return Promise.resolve({ roomId: 7 }); } },
        ui: { waitMessage() { return node(); }, errorMessage() { order.push('warning'); const box = node(); boxes.push(box); return box; }, serviceError(err) { order.push('error'); }, infoMessage() { order.push('info'); const box = node(); boxes.push(box); return box; } },
        runOp(name, fields) { calls.push({ name, fields }); return name === 'commitModel' ? Promise.resolve({ durable: true, modelSaveState: 0 }) : outcome(); }
    };
    vm.runInNewContext(editCode, context);
    return { context, calls, order, boxes };
}
const settle = () => new Promise((resolve) => setImmediate(resolve));
test('applied unsaved add adopts assigned ID before warning; retry sends COMMIT only', async () => {
    const err = ProtoViews.opError('addRoom', { code: 'internal', desc: 'Applied but not saved', data: { applied: true, modelSaveState: 1, roomId: 7, password: 'NEVER-COPY' } });
    assert.equal(err.data.password, undefined);
    const h = setup(() => Promise.reject(err));
    let adoptedId = 0;
    const target = { event: 'room', idOf: (reply) => reply.roomId, match: (value, id) => value.roomId === id, read: () => null };
    h.context.sendEdit('addRoom', { name: 'Bedroom' }, target, (error, room) => { assert.equal(error, null); adoptedId = room.roomId; h.order.push('adopt'); });
    await settle();
    assert.equal(adoptedId, 7); assert.deepEqual(h.order, ['event', 'adopt', 'warning']);
    assert.equal(h.boxes[0].inner.textContent, 'Applied but not saved');
    h.boxes[0].children[0].handlers.click(); await settle();
    assert.deepEqual(h.calls.map((call) => call.name), ['addRoom', 'commitModel']);
    assert.equal(Object.keys(h.calls[1].fields).length, 0);
    assert.equal(h.context.modelSaveWarning, null); assert.equal(h.boxes[0].removed, true);
});
test('applied unsaved delete closes its workflow before persistence warning', async () => {
    const h = setup(() => Promise.reject({ data: { applied: true, modelSaveState: 2, desc: 'Saved file uncertain' } }));
    h.context.sendEdit('deleteShade', { shadeId: 22 }, null, (err) => { assert.equal(err, null); h.order.push('closed'); });
    await settle(); assert.deepEqual(h.order, ['closed', 'warning']); assert.equal(h.calls.length, 1);
});
test('refused edit never adopts state, and repeated persistence events keep retry usable', async () => {
    const h = setup(() => Promise.reject({ desc: 'refused' }));
    h.context.sendEdit('addRoom', {}, null, (err, item) => { assert.equal(err.desc, 'refused'); assert.equal(item, null); h.order.push('refused'); });
    await settle(); assert.deepEqual(h.order, ['refused']); assert.equal(h.boxes.length, 0);
    h.context.showModelSaveWarning({ modelSaveState: 1, desc: 'Unsaved' });
    h.context.showModelSaveWarning({ modelSaveState: 1, desc: 'Unsaved' });
    assert.equal(h.boxes.length, 1);
    h.context.showModelSaveWarning({ durable: true }); assert.equal(h.context.modelSaveWarning, null);
});

test('dismissed automatic warning stays quiet until a fresh explicit edit fails', () => {
    const h = setup(() => Promise.resolve({}));
    h.context.showModelSaveWarning({ modelSaveState: 1, desc: 'Unsaved' });
    h.boxes[0].remove();
    h.context.showModelSaveWarning({ modelSaveState: 1, desc: 'Unsaved' });
    assert.equal(h.boxes.length, 1);
    h.context.showModelSaveWarning({ modelSaveState: 1, desc: 'New edit unsaved' }, true);
    assert.equal(h.boxes.length, 2);
});

test('actual authenticated client sends only the schema COMMIT op and reads file outcome', async () => {
    const { ProtoClient } = require('../../data-src/js/client.js');
    const { FakeSocket, FakeTimers, hello, sync } = require('./helpers.js');
    FakeSocket.reset(); const timers = new FakeTimers();
    const client = new ProtoClient({ host: 'device', token: 'test-token', WebSocket: FakeSocket, storage: null,
        setTimeout: timers.setTimeout, clearTimeout: timers.clearTimeout });
    client.connect(); const socket = FakeSocket.last; socket.open(); socket.deliver(hello);
    for (const frame of sync) socket.deliver(frame);
    client.schema.add({ T: [[55, 'commitModel'], [148, 'modelPersistence']],
        F: [['ap', 'applied', 'b', 0], ['du', 'durable', 'b', 0], ['ds', 'modelSaveState', 'i', -1]] });
    assert.match(socket.url, /apikey=test-token/);
    const pending = client.op('commitModel', {}); const frame = socket.sent.at(-1);
    assert.equal(frame.t, 55); assert.deepEqual(frame.d, {});
    socket.deliver({ v: 2, t: 2, id: frame.id, d: { k: 1, du: 1, ds: 0 } });
    const outcome = await pending; assert.equal(outcome.durable, true); assert.equal(outcome.modelSaveState, 0);
    client.close();
});

test('verified model file with deferred restart clears failure and explains queued operations', () => {
    const h = setup(() => Promise.resolve({}));
    h.context.showModelSaveWarning({ modelSaveState: 1, desc: 'Model file not saved' });
    h.context.showModelSaveWarning({ durable: true, rebooting: false, desc: 'Model file saved; restart not scheduled while operations remain queued.' });
    assert.equal(h.context.modelSaveWarning, null);assert.equal(h.order.at(-1), 'info');
    assert.match(h.boxes.at(-1).inner.textContent, /Model file saved; restart not scheduled/);
    assert.equal(h.calls.length, 0);
});
