const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(require.resolve('../../data-src/index.html'), 'utf8');
function fixture(saved, dark = false, blocked = false, noMedia = false) {
    const script = html.match(/<script id="theme-init">([\s\S]*?)<\/script>/);
    assert.ok(script, 'appearance must initialize in the head before stylesheets');
    assert.ok(html.indexOf('id="theme-init"') < html.indexOf('rel="stylesheet"'));
    const root = { dataset: {} };
    const select = { value: '', addEventListener(name, fn) { this.change = fn; } };
    const media = { matches: dark, addEventListener(name, fn) { this.change = fn; } };
    const storage = { value: saved, getItem() { if (blocked) throw Error('blocked'); return this.value; }, setItem(k, v) { if (blocked) throw Error('blocked'); this.value = v; } };
    const context = { window: { matchMedia: noMedia ? undefined : () => media }, localStorage: storage,
        document: { documentElement: root, getElementById: () => select, addEventListener(name, fn) { this.ready = fn; } } };
    vm.runInNewContext(script[1], context);
    context.document.ready();
    return { root, select, media, storage };
}
test('defaults to the system preference and follows changes', () => {
    const h = fixture(null, true);
    assert.equal(h.root.dataset.theme, 'dark');
    assert.equal(h.select.value, 'system');
    h.media.matches = false; h.media.change();
    assert.equal(h.root.dataset.theme, 'light');
});
test('explicit choice survives reload and overrides system changes', () => {
    const h = fixture(null);
    h.select.value = 'dark'; h.select.change();
    assert.equal(h.storage.value, 'dark');
    h.media.change(); assert.equal(h.root.dataset.theme, 'dark');
    assert.equal(fixture(h.storage.value).root.dataset.theme, 'dark');
    h.select.value = 'light'; h.select.change();
    assert.equal(h.root.dataset.theme, 'light');
    h.media.matches = true; h.media.change();
    assert.equal(h.root.dataset.theme, 'light');
    h.select.value = 'system'; h.select.change();
    assert.equal(h.root.dataset.theme, 'dark');
});
test('invalid saved preference and blocked storage fall back safely', () => {
    assert.equal(fixture('garbage', true).root.dataset.theme, 'dark');
    const h = fixture(null, true, true);
    h.select.value = 'light'; h.select.change();
    assert.equal(h.root.dataset.theme, 'light');
});

test('browsers without a media query API still allow explicit themes', () => {
    const h = fixture(null, false, false, true);
    assert.equal(h.root.dataset.theme, 'light');
    h.select.value = 'dark'; h.select.change();
    assert.equal(h.root.dataset.theme, 'dark');
});
