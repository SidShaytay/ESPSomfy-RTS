const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

// Evaluate the actual browser class without starting the rest of the application.
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const wifiClass = source.slice(source.indexOf('class Wifi {'), source.indexOf('var wifi = new Wifi();'));

function element() {
    return {
        children: [],
        attributes: {},
        className: '',
        textContent: '',
        innerHTMLWrites: [],
        listeners: {},
        set innerHTML(value) { this.innerHTMLWrites.push(value); },
        get innerHTML() { return this.innerHTMLWrites.at(-1) || ''; },
        appendChild(child) { this.children.push(child); },
        replaceChildren(...children) { this.children = children; },
        addEventListener(name, callback) { this.listeners[name] = callback; },
        setAttribute(name, value) { this.attributes[name] = value; },
        getAttribute(name) { return this.attributes[name]; },
        querySelector(selector) {
            if (selector === 'span.ssid') return this.children.find(child => child.className === 'ssid');
        }
    };
}

function setup() {
    const nodes = new Map();
    const ssidInput = { value: '' };
    const document = {
        createElement: () => element(),
        getElementById(id) {
            if (!nodes.has(id)) nodes.set(id, element());
            return nodes.get(id);
        },
        getElementsByName(name) { return name === 'ssid' ? [ssidInput] : []; }
    };
    const Wifi = vm.runInNewContext(`${wifiClass}\nWifi`, { document, console: { log() {} }, Date });
    return { wifi: new Wifi(), document, ssidInput };
}

test('scanned SSID is displayed and selected as text, without HTML parsing', () => {
    const { wifi, document, ssidInput } = setup();
    const malicious = '<img src=x onerror=alert(1)> & Home';
    wifi.displayAPs({ accessPoints: [
        { name: malicious, channel: 6, strength: -40, macAddress: '\" onclick=alert(2)' },
        { name: 'Guest', channel: 1, strength: -70, macAddress: '11:22:33:44:55:66' }
    ] });

    const list = document.getElementById('divAps');
    assert.deepEqual(list.innerHTMLWrites, []);
    assert.equal(list.children.length, 2);
    assert.equal(list.children[0].querySelector('span.ssid').textContent, malicious);
    assert.equal(list.children[0].getAttribute('data-mac'), '\" onclick=alert(2)');
    list.children[0].listeners.click();
    assert.equal(ssidInput.value, malicious);
    assert.equal(list.children[1].querySelector('span.ssid').textContent, 'Guest');
});

test('current network SSID is displayed as text', () => {
    const { wifi, document } = setup();
    const malicious = '<svg onload=alert(1)> & Home';
    wifi.procWifiStrength({ ssid: malicious, channel: 6, strength: -40 });

    const label = document.getElementById('spanNetworkSSID');
    assert.equal(label.textContent, malicious);
    assert.deepEqual(label.innerHTMLWrites, []);
    wifi.procWifiStrength({ ssid: '', channel: -1, strength: -100 });
    assert.equal(label.textContent, '-------------');
});
