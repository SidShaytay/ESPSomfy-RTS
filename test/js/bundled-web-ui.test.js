const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const cls = source.slice(source.indexOf('class Firmware {'), source.indexOf('var firmware = new Firmware();'));
function fixture(mode) {
    const nodes = new Map(), infos = [], pickers = [], sent = [];
    const get = id => {
        if (!nodes.has(id)) nodes.set(id, { style: {}, textContent: '', appendChild() {} });
        return nodes.get(id);
    };
    const context = { document: { getElementById: get,
        querySelector() { return mode ? { getAttribute() { return mode; } } : null; } },
        chipModel: () => 's3', navigator: { userAgent: '' }, proto: { model: { header: { hasOtaSlot: true } } },
        ui: { infoMessage(message) { infos.push(message); } }, general: {}, console: { log() {} } };
    const helper = source.match(/function isBundledWeb\(\) \{[\s\S]*?\n\}/)?.[0] || '';
    const Firmware = vm.runInNewContext(helper+'\n'+cls+'\nFirmware', context);
    const firmware = new Firmware(); context.firmware = firmware;
    firmware.isMobile = () => true;
    firmware.createFileUploader = service => { pickers.push(service); return { querySelector() { return get('instructions'); } }; };
    context.FormData = class { append() {} };
    context.XMLHttpRequest = class { constructor() { sent.push(this); this.upload = {}; } open(_, service) { this.service=service; } setRequestHeader() {} send() {} };
    context.security = { apiKey: 'synthetic-test-token' }; context.baseUrl = '';
    const upload = filename => {
        const field = { value: filename, files: [{ size: 1 }] };
        const widget = { style: { setProperty() {} }, addEventListener() {} };
        return firmware.uploadFile('/updateFirmware', { querySelector(selector) { return selector.startsWith('input') ? field : widget; } }, {});
    };
    return { firmware, get, infos, pickers, sent, context, upload };
}
test('bundled UI hides separate Application and explains the single firmware update', async () => {
    const h = fixture('bundled'); h.firmware.init();
    assert.equal(h.get('btnUpdateApplication').style.display, 'none');
    assert.match(h.get('bundledWebNotice').textContent, /firmware.*web UI|web UI.*firmware/i);
    h.firmware.updateApplication();
    await h.firmware.uploadFile('/updateApplication', null, null);
    assert.equal(h.pickers.length, 0); assert.equal(h.sent.length, 0); assert.equal(h.context.general.reloadApp, undefined);
    assert.equal(h.infos.length, 2);
    h.firmware.updateFirmware();
    assert.deepEqual(h.pickers, ['/updateFirmware']);
    assert.match(h.get('instructions').innerHTML, /firmware\.bin/);
    assert.match(h.get('instructions').innerHTML, /web UI/i);
    assert.doesNotMatch(h.get('instructions').innerHTML, /littlefs|onboard/i);
});
test('bundled filename filter accepts current package names as UX only', () => {
    const h = fixture('bundled');
    for (const name of ['firmware.bin', 'firmware-lilygo-t-embed-cc1101.bin', 'C:\\fakepath\\firmware.bin'])
        assert.equal(h.firmware.firmwareFilenameAllowed(name), true);
    for (const name of ['SomfyController.littlefs.esp32s3.bin', 'firmware.factory.bin', 'bootloader.bin', 'firmware-other-board.bin'])
        assert.equal(h.firmware.firmwareFilenameAllowed(name), false);
});
test('bundled actual upload method reaches only the firmware endpoint', async () => {
    const h = fixture('bundled'); await h.upload('firmware.bin');
    assert.equal(h.sent.length, 1); assert.equal(h.sent[0].service, '/updateFirmware');
});
test('generic or older pages retain separate Application and original firmware names', () => {
    for (const mode of ['filesystem', null]) {
        const h = fixture(mode); h.firmware.init();
        assert.notEqual(h.get('btnUpdateApplication').style.display, 'none');
        h.firmware.updateApplication(); assert.deepEqual(h.pickers, ['/updateApplication']);
        assert.equal(h.context.general.reloadApp, true);
        assert.equal(h.firmware.firmwareFilenameAllowed('SomfyController.ino.esp32s3.bin'), true);
        assert.equal(h.firmware.firmwareFilenameAllowed('firmware.bin'), false);
    }
});

test('bundled desktop recovery guidance preserves current configuration and counters', () => {
    const bundled=fixture('bundled');bundled.firmware.isMobile=()=>false;
    bundled.firmware.updateFirmware();
    const text=bundled.get('instructions').innerHTML;
    assert.doesNotMatch(text,/onboarding|restore this file/i);
    assert.match(text,/current configuration/i);assert.match(text,/rolling codes/i);
    const generic=fixture('filesystem');generic.firmware.isMobile=()=>false;
    generic.firmware.updateFirmware();
    assert.match(generic.get('instructions').innerHTML,/onboarding/i);
});
