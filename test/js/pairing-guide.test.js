const test = require('node:test');
const assert = require('node:assert/strict');
const { PairingGuide } = require('../../data-src/js/pairing.js');

function guide(opts = {}) {
    const sent = [], marked = [];
    const g = new PairingGuide({
        pair: opts.pair, door: opts.door,
        send: (n) => { sent.push(n); return opts.refuse ? Promise.reject({ desc: 'Radio paused' }) : Promise.resolve(); },
        mark: (p) => { marked.push(p); return Promise.resolve(); }
    });
    return { g, sent, marked };
}
const ids = (g) => g.view().buttons.map((b) => b.id);

test('pairing sends only on Send and marks only after the user saw a jog', async () => {
    const { g, sent, marked } = guide();
    assert.deepEqual(ids(g), ['ready', 'close']);
    assert.equal(sent.length, 0);
    await g.act('ready');
    assert.equal(g.view().repeat, true);
    assert.equal(g.setRepeats('500'), 127); assert.equal(g.setRepeats('0'), 1); g.setRepeats('4');
    await g.act('send');
    assert.deepEqual(sent, [4]);
    assert.deepEqual(ids(g), ['yes', 'no', 'close']);
    await g.act('no');
    assert.equal(g.step, 'send'); assert.equal(sent.length, 1, 'no automatic resend');
    await g.act('send'); await g.act('yes');
    assert.equal(marked.length, 0);
    await g.act('save');
    assert.deepEqual(marked, [true]); assert.equal(g.step, 'closed');
});

test('unpair marks unpaired; a refusal sends nothing more', async () => {
    const u = guide({ pair: false });
    await u.g.act('ready'); await u.g.act('send'); await u.g.act('yes'); await u.g.act('save');
    assert.deepEqual(u.marked, [false]);
    const r = guide({ refuse: true });
    await r.g.act('ready'); await r.g.act('send');
    assert.equal(r.g.step, 'refused'); assert.match(r.g.view().text.join(' '), /Radio paused/);
    await r.g.act('back'); assert.equal(r.g.step, 'send'); assert.equal(r.sent.length, 1);
});

test('cancel closes from any step and unknown buttons do nothing', async () => {
    const { g, sent } = guide({ door: true });
    assert.match(g.view().text.join(' '), /garage door/);
    await g.act('send'); assert.equal(g.step, 'memory'); assert.equal(sent.length, 0);
    await g.act('close'); assert.equal(g.step, 'closed');
});
