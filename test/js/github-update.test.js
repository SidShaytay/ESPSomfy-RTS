const test = require('node:test');
const assert = require('node:assert/strict');
const { GitUpdateView } = require('../../data-src/js/github-update.js');

function setup(responses = []) {
    const nodes = new Map();
    const node = (id) => {
        if (!nodes.has(id)) nodes.set(id, { style: {}, textContent: '', disabled: false, value: 0 });
        return nodes.get(id);
    };
    const calls = [], timers = [];
    let accepted = true, refused = 0;
    const view = new GitUpdateView({
        document: { getElementById: node }, token: () => 'secret-token',
        fetch: async (url, options) => {
            calls.push({ url, options });
            const next = responses.shift();
            if (next instanceof Error) throw next;
            return { ok: next.status < 400, status: next.status, json: async () => next.body };
        },
        confirm: () => accepted, onUnauthorized: () => refused++,
        setTimeout: (fn, ms) => { timers.push({ fn, ms }); return timers.length; }, clearTimeout() {}
    });
    return { view, node, calls, timers, decline: () => { accepted = false; }, refused: () => refused };
}
const available = { supported: true, state: 'available', installed: 'v4.0.2', available: 'v4.1.0', source: 'SidShaytay/ESPSomfy-RTS', compatible: true, canInstall: true, busy: false, progress: 0, error: '' };

test('unsupported boards keep GitHub controls hidden', async () => {
    const { view, node } = setup([{ status: 404 }]);
    await view.refresh();
    assert.equal(node('divGitUpdate').style.display, 'none');
});

test('renders release fields as text and enables install only when allowed', () => {
    const { view, node } = setup();
    view.render({ ...available, available: '<img src=x onerror=alert(1)>', compatible: false, canInstall: false });
    assert.equal(node('spanGitAvailable').textContent, '<img src=x onerror=alert(1)>');
    assert.equal(node('btnGitInstall').disabled, true);
    assert.equal(node('spanGitCompatibility').textContent, 'Not approved for installation');
    view.render(available);
    assert.equal(node('btnGitInstall').disabled, false);
});

test('mutating operations use POST and credentials only in header', async () => {
    const { view, calls } = setup([{ status: 202, body: { ...available, state: 'checking', busy: true } }]);
    await view.command('check');
    assert.equal(calls[0].url, '/githubUpdate/check');
    assert.equal(calls[0].options.method, 'POST');
    assert.equal(calls[0].options.headers.apikey, 'secret-token');
    assert.equal(calls[0].options.redirect, 'error');
});

test('install requires a compatible snapshot and explicit confirmation', async () => {
    const { view, calls, decline } = setup([]);
    await view.command('install');
    assert.equal(calls.length, 0);
    view.render(available);
    decline();
    await view.command('install');
    assert.equal(calls.length, 0);
});

test('busy lifecycle polls, cancel uses POST, terminal states stop polling', async () => {
    const { view, calls, timers, node } = setup([
        { status: 200, body: { ...available, state: 'downloading', busy: true, progress: 23 } },
        { status: 202, body: { ...available, state: 'cancelled', canInstall: false } }
    ]);
    await view.refresh();
    assert.equal(node('btnGitCancel').disabled, false);
    assert.equal(node('progressGitUpdate').value, 23);
    assert.equal(timers.filter(t => t.ms === 1500).length, 1);
    await view.command('cancel');
    assert.equal(calls[1].options.method, 'POST');
    assert.equal(calls[1].url, '/githubUpdate/cancel');
    assert.equal(node('btnGitCancel').disabled, true);
    assert.equal(timers.filter(t => t.ms === 1500).length, 1);
});

test('server refusal shows useful error and retry remains available', async () => {
    const { view, node } = setup([{ status: 409, body: { desc: 'Another update owns the flash' } }]);
    view.render(available);
    await view.command('check');
    assert.equal(node('spanGitError').textContent, 'Another update owns the flash');
    assert.equal(node('btnGitCheck').disabled, false);
    assert.equal(node('btnGitInstall').disabled, true);
});

test('authentication refusal hides controls and requests login', async () => {
    const env = setup([{ status: 401 }]);
    await env.view.refresh();
    assert.equal(env.refused(), 1);
    assert.equal(env.node('divGitUpdate').style.display, 'none');
});


test('confirmed compatible install uses its implemented POST route', async () => {
    const { view, calls } = setup([{ status: 202, body: { ...available, state: 'downloading', busy: true } }]);
    view.render(available);
    await view.command('install');
    assert.equal(calls.length, 1);
    assert.equal(calls[0].url, '/githubUpdate/install');
    assert.equal(calls[0].options.method, 'POST');
});

test('concurrent commands do not start another request while one is pending', async () => {
    const env = setup([]);
    let resolve;
    let requests = 0;
    env.view.fetch = () => { requests++; return new Promise(done => { resolve = done; }); };
    const first = env.view.refresh();
    await env.view.command('check');
    assert.equal(requests, 1);
    resolve({ status: 200, ok: true, json: async () => available });
    await first;
});

test('network failure clears stale installation eligibility', async () => {
    const { view, node } = setup([new Error('Connection lost')]);
    view.render(available);
    await view.refresh();
    assert.equal(node('btnGitInstall').disabled, true);
    assert.equal(node('spanGitError').textContent, 'Connection lost');
    assert.equal(node('btnGitCheck').disabled, false);
});


test('verified-update profiles disable legacy filesystem upload and explain provisioning', () => {
    const { view, node } = setup();
    view.render(available);
    assert.equal(node('btnUpdateApplication').disabled, true);
    assert.equal(node('btnUpdateApplication').style.display, 'none');
    assert.match(node('spanGitFilesystemPolicy').textContent, /coherent provisioning/);
    view.render({ supported: false });
    assert.equal(node('btnUpdateApplication').disabled, false);
    assert.equal(node('btnUpdateApplication').style.display, '');
});
