// Characterize the existing Light gap; these are not claims of working control.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
function handler(eventName, receiver, button) {
    const start = source.indexOf(`btns[i].addEventListener('${eventName}', (event) => {`, source.indexOf('    setShadesList(shades)'));
    assert.ok(start > 0);
    const end = source.indexOf('}, true);', start) + '}, true);'.length;
    const callbacks = {};
    vm.runInNewContext('(function(){' + source.slice(start, end) + '\n}).call(receiver)', {
        receiver, btns: [{ addEventListener(name, fn) { callbacks[name] = fn; } }], i: 0,
        console: { log() {} }, makeBool: value => value === 'true', clearTimeout() {},
    });
    return () => callbacks[eventName]({ currentTarget: button, preventDefault() {} });
}
test('existing Light mouse click changes appearance without sending a command', () => {
    const attributes = { 'data-cmd': 'light', 'data-shadeid': '7', 'data-on': 'true' };
    const commands = [];
    const button = { getAttribute: key => attributes[key], setAttribute(key, value) { attributes[key] = String(value); },
        closest() { return { getAttribute() { return '0'; } }; } };
    const receiver = { sendCommand(...args) { commands.push(args); }, sendTiltCommand(...args) { commands.push(args); } };
    handler('mousedown', receiver, button)();
    const up = handler('mouseup', receiver, button); up();
    assert.equal(attributes['data-on'], 'false'); assert.deepEqual(commands, []);
    up(); assert.equal(attributes['data-on'], 'true'); assert.deepEqual(commands, []);
});
