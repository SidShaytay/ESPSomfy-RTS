const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
function fixture(stepSize = 127, command = 'StepUp') {
    const calls = [];
    const option = { getAttribute(name) { return ({ 'data-type': 'group', 'data-groupId': '9', 'data-address': '900' })[name]; } };
    const context = { console: { log() {} }, mouseDown: true, clearTimeout() {},
        ui: { fromElement(_, target) { target.stepSize = stepSize; target.repeat = 3; } },
        document: { getElementById() { return { querySelector() { return { selectedOptions: [option] }; } }; } },
        sendOp(name, fields, cb) { calls.push({ name, fields, cb }); } };
    const repeat = source.match(/function repeatOf\(repeat\) \{[^\n]+\}/)[0];
    const cls = source.slice(source.indexOf('class Somfy {'), source.indexOf('var somfy = new Somfy();'));
    const Somfy = vm.runInNewContext(repeat + '\n' + cls + '\nSomfy', context);
    const somfy = new Somfy(); context.somfy = somfy;
    return { calls, context, somfy, button: { parentElement: { parentElement: {} }, getAttribute() { return command; } } };
}
test('group virtual remote keeps the selected step and repeat on initial send and held repeats', () => {
    for (const size of [1, 127]) for (const command of ['StepUp', 'StepDown']) {
        const h = fixture(size, command); h.somfy.sendVRCommand(h.button);
        assert.equal(h.calls.length, 1); assert.equal(h.calls[0].name, 'groupCommand');
        assert.equal(h.calls[0].fields.groupId, 9); assert.equal(h.calls[0].fields.command, command);
        assert.equal(h.calls[0].fields.stepSize, size); assert.equal(h.calls[0].fields.repeat, 3);
        h.calls[0].cb(null, {}); assert.equal(h.calls.length, 2); assert.equal(h.calls[1].name, 'repeatCommand');
        assert.equal(h.calls[1].fields.stepSize, size); assert.equal(h.calls[1].fields.repeat, 3);
        h.context.mouseDown = false; h.calls[1].cb(null, {}); assert.equal(h.calls.length, 2);
    }
});
test('refused group request does not schedule a held repeat', () => {
    const h = fixture(); h.somfy.sendVRCommand(h.button); h.calls[0].cb({ desc: 'refused' }); assert.equal(h.calls.length, 1);
});
test('positional group callers preserve command, repeat and callback', () => {
    const h = fixture(), done = () => {};
    h.somfy.sendGroupCommand(9, 'Up', 2, done); h.somfy.sendGroupRepeat(9, 'Prog', 4, done);
    assert.equal(h.calls[0].fields.groupId, 9); assert.equal(h.calls[0].fields.command, 'Up'); assert.equal(h.calls[0].fields.repeat, 2);
    assert.equal(h.calls[1].fields.command, 'Prog'); assert.equal(h.calls[1].fields.repeat, 4);
    assert.equal(h.calls[0].cb, done); assert.equal(h.calls[1].cb, done); assert.equal(h.calls[0].fields.stepSize, undefined);
});
