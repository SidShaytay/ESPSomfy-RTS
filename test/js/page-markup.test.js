const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const html = fs.readFileSync(require.resolve('../../data-src/index.html'), 'utf8');
const css = fs.readFileSync(require.resolve('../../data-src/main.css'), 'utf8');

test('div.instructions is still the full-panel overlay', () => {
    const rule = css.slice(css.indexOf('div.instructions {'));
    assert.match(rule.slice(0, rule.indexOf('}')), /position:\s*absolute/);
});

test('static page text never uses the overlay class', () => {
    // Overlays are built by index.js when a wizard opens. A static div with this
    // class covers the whole tab, as the v4.3.0 Firmware tab did.
    const offenders = [...html.matchAll(/<div\b[^>]*class="[^"]*\binstructions\b[^"]*"[^>]*>/g)].map((m) => m[0]);
    assert.deepEqual(offenders, []);
});

test('battery reading and charge limit sit in the header and System > Options', () => {
    const options = html.slice(html.indexOf('<div id="divSystemOptions"'), html.indexOf('<div id="divSecurityOptions"'));
    assert.match(options, /id="selChargeLimit"/);
    assert.match(options, /id="spanBattery"/);
    const header = html.slice(html.indexOf('id="divWiFiStrength"'), html.indexOf('class="tab-container"'));
    assert.match(header, /id="divHeaderBattery"/);
    const firmware = html.slice(html.indexOf('<div id="divFirmware"'), html.indexOf('<div id="divNetworkSettings"'));
    assert.doesNotMatch(firmware, /selChargeLimit|spanBattery/);
});
