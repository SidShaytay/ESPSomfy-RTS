// Firmware-only GitHub update controls. A snapshot is copied by the server;
// polling never pumps the controller's protocol or modifies its shade model.
class GitUpdateView {
    constructor(opts) {
        this.document = opts.document;
        this.fetch = opts.fetch;
        this.token = opts.token;
        this.confirm = opts.confirm;
        this.onUnauthorized = opts.onUnauthorized;
        this.setTimeout = opts.setTimeout || ((fn, ms) => setTimeout(fn, ms));
        this.clearTimeout = opts.clearTimeout || ((id) => clearTimeout(id));
        this.snapshot = null;
        this.pending = false;
        this.timer = null;
    }
    node(id) { return this.document.getElementById(id); }
    render(status) {
        this.snapshot = status;
        this.node('divGitUpdate').style.display = status.supported ? '' : 'none';
        this.node('btnUpdateApplication').disabled = !!status.supported;
        this.node('btnUpdateApplication').style.display = status.supported ? 'none' : '';
        if (!status.supported) return;
        // Release metadata and errors are untrusted text, never HTML.
        const labels = {
            spanGitInstalled: status.installed || 'Unknown',
            spanGitAvailable: status.available || 'Check for a release',
            spanGitSource: status.source || '',
            spanGitState: status.state || 'idle',
            spanGitError: status.error || '',
            spanGitFilesystemPolicy: 'Filesystem changes require coherent provisioning during an approved service window. Web filesystem upload is disabled on this profile.',
            spanGitCompatibility: status.canInstall && status.compatible ? 'Exact board and application match' : 'Not approved for installation'
        };
        for (const id of Object.keys(labels)) this.node(id).textContent = labels[id];
        this.node('progressGitUpdate').value = Math.max(0, Math.min(100, Number(status.progress) || 0));
        this.node('btnGitCheck').disabled = !!status.busy;
        this.node('btnGitInstall').disabled = !!status.busy || !status.compatible || !status.canInstall;
        this.node('btnGitCancel').disabled = !['checking', 'downloading', 'verifying'].includes(status.state);
    }
    async refresh() { return this.request(''); }
    async command(op) {
        if (!['check', 'install', 'cancel'].includes(op)) return;
        if (op === 'install') {
            const status = this.snapshot;
            if (!status || status.busy || !status.canInstall || !status.compatible) return;
            if (!this.confirm('Install ' + status.available + ' from ' + status.source + '? The controller will restart. Save a fresh backup and use an authorized service window.')) return;
        }
        return this.request(op);
    }
    async request(op) {
        if (this.pending) return;
        this.pending = true;
        if (this.timer !== null) this.clearTimeout(this.timer);
        this.timer = null;
        const abort = typeof AbortController !== 'undefined' ? new AbortController() : null;
        const deadline = abort ? this.setTimeout(() => abort.abort(), 10000) : null;
        try {
            const response = await this.fetch('/githubUpdate' + (op ? '/' + op : ''), {
                method: op ? 'POST' : 'GET',
                headers: { apikey: this.token(), Accept: 'application/json' },
                cache: 'no-store', redirect: 'error', signal: abort ? abort.signal : undefined
            });
            if (response.status === 404) { this.render({ supported: false }); return; }
            if (response.status === 401) {
                this.render({ supported: false });
                this.onUnauthorized();
                return;
            }
            const status = await response.json();
            if (!response.ok) throw new Error(status.desc || status.error || 'Update request refused (' + response.status + ')');
            this.render(status);
            if (status.busy) this.timer = this.setTimeout(() => this.refresh(), 1500);
        } catch (error) {
            // A stale success must never leave Install enabled after a failure.
            if (this.snapshot && this.snapshot.supported) {
                this.render(Object.assign({}, this.snapshot, { busy: false, canInstall: false, state: 'error', error: error.message || 'Unable to contact controller; check its status before retrying.' }));
            }
        } finally {
            if (deadline !== null) this.clearTimeout(deadline);
            this.pending = false;
        }
    }
}
if (typeof module !== 'undefined' && module.exports) module.exports = { GitUpdateView };
