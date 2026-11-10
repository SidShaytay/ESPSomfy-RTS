const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const somfyClass = source.slice(source.indexOf('class Somfy {'), source.indexOf('var somfy = new Somfy();'));
function fixture(chip = 's3') {
    const nodes = new Map(), errors = [], edits = [];
    function node() {
        return { style: { setProperty() {} }, attrs: {}, value: '', checked: false,
            parentElement: { style: {}, querySelector: () => node() },
            classList: { contains() { return false; }, add() {}, remove() {} },
            setAttribute(key, value) { this.attrs[key] = String(value); },
            getAttribute(key) { return this.attrs[key]; } };
    }
    class Select {
        constructor() { this.options = []; this.style = {}; this.selectedIndex = 0; }
        get firstChild() { return this.options[0]; }
        removeChild() { this.options.shift(); }
        get value() { return this.options[this.selectedIndex]?.value || ''; }
        set value(value) { this.selectedIndex = this.options.findIndex(opt => opt.value === String(value)); }
    }
    function get(id) {
        if (!nodes.has(id)) nodes.set(id, id.startsWith('selShadeGPIO') ? new Select() : node());
        return nodes.get(id);
    }
    const document = { getElementById: get, querySelector: () => node() };
    const context = { document, console: { log() {} }, chipModel: () => chip,
        Option: function(text, value, selected) { this.text = text; this.value = String(value); this.defaultSelected = !!selected; },
        ui: { fromElement: () => fixture.payload,
              errorMessage: (_, message) => errors.push(message), serviceError: err => errors.push(err) },
        sendEdit: (name, payload) => edits.push({ name, payload }), editTarget() {} };
    const Somfy = vm.runInNewContext(`${somfyClass}\nSomfy`, context);
    get('selShadeType').value = '0'; get('selTiltType').value = '0';
    get('spanShadeId').innerText = '7';
    context.somfy = new Somfy();
    return { somfy: context.somfy, get, errors, edits, context };
}
test('every GPIO category exposes its complete physical protocol pads', () => {
    const { somfy, get } = fixture();
    for (let category = 0; category <= 16; category++) {
        get('selShadeType').value = String(category);
        for (const protocol of [0, 1, 2, 8, 9]) {
            get('selShadeProto').value = String(protocol);
            somfy.onShadeProtoChanged(get('selShadeProto'));
            somfy.onShadeTypeChanged(get('selShadeType'));
            const gp = protocol === 8 || protocol === 9;
            assert.equal(get('divGPIOControl').style.display, gp ? 'inline-block' : 'none');
            assert.equal(get('divGPIOUp').style.display, gp ? 'inline-block' : 'none');
            assert.equal(get('divGPIODown').style.display, gp ? 'inline-block' : 'none');
            assert.equal(get('divGPIOMy').style.display, protocol === 9 ? 'inline-block' : 'none');
        }
    }
});
test('shade selectors offer None and preserve unavailable stored GPIO values', () => {
    const { somfy, get } = fixture();
    somfy.loadShadePins({ gpioUp: 19, gpioDown: 255, gpioMy: 42 });
    for (const id of ['selShadeGPIOUp', 'selShadeGPIODown', 'selShadeGPIOMy'])
        assert.equal(get(id).options.filter(opt => opt.value === '255').length, 1);
    assert.match(get('selShadeGPIOUp').options.find(opt => opt.value === '19').text, /unavailable/i);
    // Actual binder picks an existing option by value. No fallback to GPIO0.
    const binder = source.slice(source.indexOf('class UIBinder {'), source.indexOf('var ui = new UIBinder();'));
    const UIBinder = vm.runInNewContext(`${binder}\nUIBinder`, { HTMLSelectElement: get('selShadeGPIOUp').constructor,
        HTMLInputElement: class {}, HTMLElement: class {}, console });
    const ui = new UIBinder();
    ui.setValue(get('selShadeGPIOUp'), 19);
    ui.setValue(get('selShadeGPIODown'), 255);
    assert.equal(get('selShadeGPIOUp').value, '19');
    assert.equal(get('selShadeGPIODown').value, '255');
    const radio = new (get('selShadeGPIOUp').constructor)();
    somfy.loadPins('out', radio);
    assert.equal(radio.options.some(opt => opt.value === '255'), false);
});
test('GPIO duplicate validation uses physical pads, excluding unassigned None', () => {
    const h = fixture();
    const base = { name: 'Test', remoteAddress: 100, upTime: 1000, downTime: 1000 };
    for (const category of [0, 5, 9, 10, 14, 15, 16]) {
        for (const protocol of [8, 9]) {
            fixture.payload = { ...base, shadeType: category, proto: protocol, gpioUp: 17, gpioDown: 17, gpioMy: 42 };
            h.somfy.saveShade();
            assert.equal(h.edits.length, 0);
            assert.match(h.errors.pop(), /unique/);
        }
    }
    fixture.payload = { ...base, shadeType: 9, proto: 9, gpioUp: 255, gpioDown: 255, gpioMy: 255 };
    h.somfy.saveShade();
    assert.equal(h.edits.length, 1);
    assert.equal(h.edits[0].payload.gpioUp, 255);
    assert.equal(h.edits[0].payload.gpioMy, 255);
});
test('opening existing legacy pins preserves selected payload across protocol and category changes', () => {
    const h = fixture();
    const stored = { name: 'Legacy', shadeId: 7, shadeType: 9, proto: 9,
        gpioUp: 19, gpioDown: 255, gpioMy: 42, linkedRemotes: [] };
    h.context.proto = { model: { shades: new Map([[7, stored]]) } };
    h.context.ui.toElement = (_, shade) => {
        for (const [key, id] of [['gpioUp', 'selShadeGPIOUp'], ['gpioDown', 'selShadeGPIODown'], ['gpioMy', 'selShadeGPIOMy']])
            h.get(id).value = shade[key];
    };
    h.somfy.showEditShade = () => {};
    h.somfy.setLinkedRemotesList = () => {};
    h.somfy.openEditShade(7);
    for (const proto of [8, 0, 9]) {
        h.get('selShadeProto').value = String(proto);
        h.somfy.onShadeProtoChanged(h.get('selShadeProto'));
        h.get('selShadeType').value = '5';
        h.somfy.onShadeTypeChanged(h.get('selShadeType'));
        assert.equal(h.get('selShadeGPIOUp').value, '19');
        assert.equal(h.get('selShadeGPIODown').value, '255');
        assert.equal(h.get('selShadeGPIOMy').value, '42');
    }
    assert.equal(stored.gpioUp, 19);
});
test('a new shade starts unassigned rather than inheriting an old form selection', () => {
    const h = fixture();
    h.context.sendOp = (_, fields, callback) => callback(null, { gpioUp: 0, gpioDown: 0, gpioMy: 0 });
    h.context.proto = { schema: { fill: value => value } };
    h.context.ProtoModel = { FIELDS: { shade: [] } };
    let defaults;
    h.context.ui.toElement = (_, shade) => { defaults = shade; };
    h.somfy.showEditShade = () => {};
    h.somfy.openEditShade();
    assert.equal(defaults.gpioUp, 255);
    assert.equal(defaults.gpioDown, 255);
    assert.equal(defaults.gpioMy, 255);
    assert.equal(h.get('selShadeGPIOUp').value, '255');
});
test('new GPIO domain failures have actionable canonical browser messages', () => {
    const values = vm.runInNewContext(source.slice(source.indexOf('var errors = ['), source.indexOf('document.oncontextmenu')) + '\nerrors');
    assert.match(values.find(error => error.code === -10).desc, /board.*radio|radio.*board/i);
    assert.match(values.find(error => error.code === -13).desc, /distinct|unique/i);
    assert.match(values.find(error => error.code === -14).desc, /output.*GPIO|GPIO.*output/i);
});

test('unavailable pin-ownership storage has a distinct canonical failure description', () => {
    const start = source.indexOf('var errors = [');
    const end = source.indexOf('document.oncontextmenu', start);
    const context = {}; vm.runInNewContext(source.slice(start, end), context);
    const unavailable = context.errors.find(error => error.code === -15);
    assert.match(unavailable.desc, /ownership settings are unavailable/);
    assert.match(unavailable.desc, /radio\/Ethernet/);
});
