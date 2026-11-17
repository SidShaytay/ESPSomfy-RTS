const test = require('node:test');
const assert = require('node:assert/strict');
const { GitUpdateClient, describeBattery, CHARGE_LIMITS, CHARGE_LIMIT_RECOMMENDED, isDecimalId } = require('../../data-src/js/updates.js');

function rig(replies) {
    const calls = [], states = [], errors = [], timers = new Map();
    let timer = 0;
    const proto = {
        op(name, fields) {
            calls.push({ name, fields });
            const r = replies.shift();
            if (r instanceof Error) return Promise.reject(r);
            return Promise.resolve(r);
        }
    };
    const client = new GitUpdateClient(proto, {
        onState: (s) => states.push(s), onError: (e) => errors.push(e),
        setTimeout: (fn) => { const id = ++timer; timers.set(id, fn); return id; },
        clearTimeout: (id) => timers.delete(id)
    });
    const fire = () => { const [id, fn] = timers.entries().next().value; timers.delete(id); fn(); };
    return { client, calls, states, errors, timers, fire };
}
const err = (code) => Object.assign(new Error(code), { code, desc: code });
const base = { updateSupported: true, updateInstalled: 'v4.2.2', updateReason: '', updateReasonCode: 0 };

test('check, review and install send exact ids and never install on their own', async () => {
    const r = rig([
        { ...base, updatePhase: 'checking', updateBusy: true, updateOperation: '7' },
        { ...base, updatePhase: 'available', updateCanInstall: true, updateCandidate: '9007199254740993', updateVersion: 'v4.3.0', updateSize: 2097152 },
        { ...base, updatePhase: 'downloading', updateBusy: true, updateCancellable: true, updateOperation: '9', updateProgress: 40 }
    ]);
    await r.client.check();
    assert.deepEqual(r.calls[0], { name: 'gitUpdate', fields: { updateAction: 1 } });
    assert.equal(r.timers.size, 1, 'polls while checking');
    r.fire(); await new Promise(setImmediate);
    assert.equal(r.calls[1].fields.updateAction, undefined, 'a poll is a plain status read');
    assert.equal(r.timers.size, 0, 'stops polling once idle');
    const v = GitUpdateClient.describe(r.client.state);
    assert.ok(v.install && v.check && !v.cancel);
    assert.match(v.title, /v4\.3\.0.*2\.0 MB/);
    await r.client.install();
    assert.deepEqual(r.calls[2].fields, { updateAction: 2, updateCandidate: '9007199254740993' });
    const p = GitUpdateClient.describe(r.client.state);
    assert.equal(p.progress, 40); assert.ok(p.cancel); assert.ok(!p.install && !p.check);
});

test('install and cancel refuse without a current candidate or operation', async () => {
    const r = rig([]);
    await assert.rejects(r.client.install());
    await assert.rejects(r.client.cancel());
    r.client.state = { ...base, updatePhase: 'downloading', updateCancellable: true, updateOperation: '12' };
    const c = rig([{ ...base, updatePhase: 'cancelled' }]);
    c.client.state = r.client.state;
    await c.client.cancel();
    assert.deepEqual(c.calls[0].fields, { updateAction: 3, updateOperation: '12' });
});

test('malformed ids and old firmware', async () => {
    for (const bad of ['0', '01', '18446744073709551616', 5, '1e3']) assert.equal(isDecimalId(bad), false, String(bad));
    assert.equal(isDecimalId('18446744073709551615'), true);
    const r = rig([{ ...base, updatePhase: 'available', updateCandidate: '0' }]);
    await assert.rejects(r.client.refresh());
    assert.equal(r.errors.length, 1);
    const old = rig([err('unknownOp')]);
    assert.equal(await old.client.refresh(), null);
    assert.equal(old.client.supported, false);
    assert.equal(old.states.at(-1), null);
});

test('a dropped socket while installing is shown as a restart, not a failure', async () => {
    const r = rig([{ ...base, updatePhase: 'written', updateBusy: true, updateOperation: '3' }, err('closed'), { ...base, updatePhase: 'idle', updateInstalled: 'v4.3.0', updateBootPending: true }]);
    await r.client.refresh();
    r.fire(); await new Promise(setImmediate);
    assert.equal(r.errors.length, 0);
    assert.equal(r.states.at(-1).restarting, true);
    assert.match(GitUpdateClient.describe(r.states.at(-1)).title, /Restarting/);
    r.fire(); await new Promise(setImmediate);
    assert.match(GitUpdateClient.describe(r.client.state).title, /Checking the new firmware/);
});

test('development builds and unsupported boards explain themselves and offer no check', () => {
    const dev = GitUpdateClient.describe({ ...base, updatePhase: 'unavailable', updateReasonCode: 3, updateInstalled: 'v4.2.1' });
    assert.match(dev.title, /Development build/); assert.match(dev.detail, /tagged release/); assert.ok(!dev.check);
    const none = GitUpdateClient.describe({ updatePhase: 'unavailable', updateSupported: false });
    assert.ok(!none.check && !none.install);
    const busy = GitUpdateClient.describe({ ...base, updatePhase: 'available', updateCanInstall: true, updateVersion: 'v5', updateBusy: true });
    assert.ok(!busy.install && !busy.check);
});

test('battery text shows only when a battery is there', () => {
    assert.equal(describeBattery(null), null);
    assert.equal(describeBattery({ batteryState: 0 }), null);
    assert.equal(describeBattery({ batteryState: 3 }), null);
    assert.match(describeBattery({ batteryState: 1 }).text, /Starting/);
    assert.match(describeBattery({ batteryState: 4 }).text, /read failed/);
    const ok = describeBattery({ batteryState: 2, batteryPercent: 82, batteryMillivolts: 4051, batteryMilliamps: 120 });
    assert.equal(ok.percent, 82); assert.match(ok.text, /82% \(4\.05 V, charging\)/);
    assert.match(describeBattery({ batteryState: 2, batteryPercent: 50, batteryMillivolts: 3800, batteryMilliamps: -90 }).text, /discharging/);
    assert.deepEqual(CHARGE_LIMITS.map((l) => l.value), [0, 1, 2, 3, 4]);
    assert.deepEqual(CHARGE_LIMITS.map((l) => parseInt(l.label, 10)), [100, 90, 80, 70, 60]);
    assert.match(CHARGE_LIMITS[CHARGE_LIMIT_RECOMMENDED].label, /60%.*recommended/);
});
