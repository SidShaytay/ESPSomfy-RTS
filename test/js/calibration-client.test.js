const test = require('node:test');
const assert = require('node:assert/strict');
const { LiftCalibrationClient } = require('../../data-src/js/calibration.js');

function deferred() {
    let resolve, reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
}
function rig() {
    const calls = [], states = [], errors = [];
    const proto = {
        op(name, data) {
            const request = deferred();
            calls.push({ name, data, ...request });
            return request.promise;
        },
        model: { shade: { shadeId: 7, upTime: 10000, downTime: 10000, calibrationKind: 0 } }
    };
    const client = new LiftCalibrationClient(proto, {
        onState: (state) => states.push(state),
        onError: (error) => errors.push(error)
    });
    return { client, proto, calls, states, errors };
}
function reply(token = 42, stage = 2, extra = {}) {
    return { calibrationSession: token, calibrationStage: stage, ...extra };
}
async function begun(r, token = 42) {
    const pending = r.client.begin(7, 2);
    assert.equal(r.calls.length, 1);
    r.calls[0].resolve(reply(token));
    await pending;
    return r;
}
async function ignoreOrReject(promise) {
    try { await promise; } catch (_) { /* A stale response may be refused or ignored. */ }
}

test('begin, action and poll use named protocol fields without changing the saved model', async () => {
    const r = rig();
    const saved = JSON.parse(JSON.stringify(r.proto.model));
    const begin = r.client.begin(7, 2);
    assert.deepEqual(r.calls[0].data, { calibrationAction: 100, shadeId: 7, calibrationKind: 2 });
    assert.equal(r.calls[0].name, 'liftCalibration');
    assert.equal(r.client.token, 0);
    r.calls[0].resolve(reply(0xffffffff, 2, { upTime: 20000, downTime: 15000 }));
    await begin;
    assert.equal(r.client.token, 0xffffffff);
    assert.equal(r.client.state.calibrationStage, 2);
    assert.equal(r.states.length, 1);
    assert.equal(r.states[0].calibrationStage, 2);
    const action = r.client.act(6, true);
    assert.deepEqual(r.calls[1].data, { calibrationAction: 6, calibrationSession: 0xffffffff, calibrationHuman: true });
    r.calls[1].resolve(reply(0xffffffff, 7));
    await action;
    const poll = r.client.poll();
    assert.deepEqual(r.calls[2].data, { calibrationAction: 101, calibrationSession: 0xffffffff });
    r.calls[2].resolve(reply(0xffffffff, 8));
    await poll;
    assert.equal(r.client.state.calibrationStage, 8);
    assert.deepEqual(r.states.map((state) => state.calibrationStage), [2, 7, 8]);
    assert.deepEqual(r.proto.model, saved);
    assert.equal(r.calls.length, 3);
});

test('only one begin or action mutation may be in flight', async () => {
    const r = rig();
    const begin = r.client.begin(7, 1);
    await assert.rejects(async () => r.client.begin(8, 2));
    await assert.rejects(async () => r.client.act(3));
    assert.equal(r.calls.length, 1);
    r.calls[0].resolve(reply());
    await begin;
    const action = r.client.act(3);
    await assert.rejects(async () => r.client.act(4));
    await assert.rejects(async () => r.client.begin(8, 2));
    assert.equal(r.calls.length, 2);
    assert.equal(r.calls[1].data.calibrationHuman, false);
    r.calls[1].resolve(reply(42, 3));
    await action;
});

test('older polls cannot overwrite a newer issued action regardless of reply order', async () => {
    for (const oldReplyFirst of [false, true]) {
        const r = await begun(rig());
        const baselineCallbacks = r.states.length;
        const poll = r.client.poll();
        const action = r.client.act(6, true);
        assert.equal(r.calls.length, 3);
        if (oldReplyFirst) {
            r.calls[1].resolve(reply(42, 4));
            await ignoreOrReject(poll);
            assert.equal(r.client.state.calibrationStage, 2);
            assert.equal(r.states.length, baselineCallbacks);
        }
        r.calls[2].resolve(reply(42, 8));
        await action;
        if (!oldReplyFirst) {
            r.calls[1].resolve(reply(42, 4));
            await ignoreOrReject(poll);
        }
        assert.equal(r.client.state.calibrationStage, 8);
        assert.equal(r.states.length, baselineCallbacks + 1);
    }
    const r = await begun(rig());
    const first = r.client.poll(), second = r.client.poll();
    assert.equal(r.calls.length, 3);
    r.calls[2].resolve(reply(42, 9));
    await second;
    r.calls[1].resolve(reply(42, 5));
    await ignoreOrReject(first);
    assert.equal(r.client.state.calibrationStage, 9);
});

test('disconnect clears immediately and delayed begin, action or poll replies emit no RF or new state', async () => {
    for (const operation of ['begin', 'action', 'poll']) {
        const r = rig();
        let pending;
        if (operation === 'begin') pending = r.client.begin(7, 2);
        else {
            await begun(r);
            pending = operation === 'action' ? r.client.act(3) : r.client.poll();
        }
        const request = r.calls.at(-1);
        const sent = r.calls.length;
        r.client.disconnect();
        assert.equal(r.client.token, 0);
        assert.equal(r.client.state, null);
        const callbacks = r.states.length;
        request.resolve(reply(42, 8));
        await ignoreOrReject(pending);
        assert.equal(r.client.token, 0);
        assert.equal(r.client.state, null);
        assert.equal(r.calls.length, sent);
        assert.equal(r.states.length, callbacks);
        assert.equal(await r.client.poll(), null);
        assert.equal(r.calls.length, sent);
    }
});

test('request errors are reported and bubble without retrying begin, save, poll or cancel', async () => {
    for (const operation of ['begin', 'save', 'poll', 'close']) {
        const r = rig();
        if (operation !== 'begin') await begun(r);
        const before = r.calls.length;
        const failure = new Error(`${operation} request failed`);
        const pending = operation === 'begin' ? r.client.begin(7, 1)
            : operation === 'save' ? r.client.act(9)
            : operation === 'poll' ? r.client.poll() : r.client.close();
        assert.equal(r.calls.length, before + 1);
        r.calls.at(-1).reject(failure);
        await assert.rejects(pending, /request failed/);
        assert.equal(r.errors.length, 1);
        assert.equal(r.calls.length, before + 1);
    }
});

test('reply validation requires exact nonzero uint32 sessions and integer stages', async () => {
    const invalid = [
        reply(0), reply(-1), reply(1.5), reply(0x100000000), reply('42'),
        { calibrationSession: undefined, calibrationStage: 2 }, reply(true), reply(NaN),
        reply(42, -1), reply(42, 256), reply(42, 1.5), reply(42, '2'), reply(42, null),
        { calibrationSession: 42 }, { calibrationStage: 2 }, null, []
    ];
    for (const response of invalid) {
        const r = rig();
        const pending = r.client.begin(7, 2);
        r.calls[0].resolve(response);
        await assert.rejects(pending);
        assert.equal(r.client.token, 0);
        assert.equal(r.client.state, null);
        assert.equal(r.states.length, 0);
        assert.equal(r.errors.length, 1);
        assert.equal(r.calls.length, 1);
    }
    for (const stage of [0, 255]) {
        const r = rig();
        const pending = r.client.begin(7, 1);
        r.calls[0].resolve(reply(1, stage));
        await pending;
        assert.equal(r.client.token, 1);
        assert.equal(r.client.state.calibrationStage, stage);
    }
});

test('a response for another session cannot replace the active token or state', async () => {
    const r = await begun(rig());
    const state = r.client.state;
    const callbacks = r.states.length;
    const pending = r.client.act(6, true);
    r.calls[1].resolve(reply(99, 8));
    await ignoreOrReject(pending);
    assert.equal(r.client.token, 42);
    assert.deepEqual(r.client.state, state);
    assert.equal(r.states.length, callbacks);
    assert.equal(r.calls.length, 2);
});

test('close sends one explicit cancel and obsolete poll replies cannot reopen the guide', async () => {
    const r = await begun(rig());
    const poll = r.client.poll();
    const close = r.client.close();
    assert.equal(r.calls.length, 3);
    assert.equal(r.calls[2].name, 'liftCalibration');
    assert.equal(r.calls[2].data.calibrationAction, 10);
    assert.equal(r.calls[2].data.calibrationSession, 42);
    r.calls[2].resolve(reply(42, 0));
    await close;
    assert.equal(r.client.token, 0);
    assert.equal(r.client.state, null);
    const callbacks = r.states.length;
    r.calls[1].resolve(reply(42, 8));
    await ignoreOrReject(poll);
    assert.equal(r.client.token, 0);
    assert.equal(r.client.state, null);
    assert.equal(r.states.length, callbacks);
    assert.equal(await r.client.poll(), null);
    assert.equal(r.calls.length, 3);
});
