// The web UI's view of the device's GitHub updater (protocol 2 op gitUpdate)
// and of its battery (op battery).  The device does all of the downloading,
// verification and installation, with the same checks the LCD uses; this
// client only asks, shows the state and polls while something is running.
// The minify step joins this file into index.js.
const GIT_UPDATE_ACTION = { status: 0, check: 1, install: 2, cancel: 3 };
// GitUpdateReason numbers the device sends as updateReasonCode.
const GIT_UPDATE_REASON = { none: 0, development: 3, noNewerRelease: 21 };
const GIT_UPDATE_ACTIVE = ['queued', 'checking', 'preparing', 'prepared', 'downloading', 'verifying', 'writing', 'written', 'cleanupPending', 'retryWaiting'];
// A decimal id the device sent; kept as text, as it can exceed 2^53.
function isDecimalId(text) {
    return typeof text === 'string' && /^[1-9][0-9]{0,19}$/.test(text) &&
        (text.length < 20 || text <= '18446744073709551615');
}
class GitUpdateClient {
    // proto: an object with op(name, fields) -> Promise.  options: onState,
    // onError, setTimeout/clearTimeout (for the tests), pollMs.
    constructor(proto, options = {}) {
        this.proto = proto;
        this.onState = options.onState || (() => {});
        this.onError = options.onError || (() => {});
        this.setTimeout = options.setTimeout || ((fn, ms) => setTimeout(fn, ms));
        this.clearTimeout = options.clearTimeout || ((id) => clearTimeout(id));
        this.pollMs = options.pollMs || 1500;
        this.state = null;
        this.timer = null;
        this.supported = true;
        this.lostWhileInstalling = 0;
    }
    stop() {
        if (this.timer) this.clearTimeout(this.timer);
        this.timer = null;
    }
    schedule() {
        this.stop();
        if (!GitUpdateClient.active(this.state)) return;
        this.timer = this.setTimeout(() => { this.timer = null; this.refresh().catch(() => {}); }, this.pollMs);
    }
    static active(state) {
        return !!state && (state.updateBusy === true || GIT_UPDATE_ACTIVE.includes(state.updatePhase) || state.updateBootPending === true);
    }
    async request(fields) {
        try {
            const state = await this.proto.op('gitUpdate', fields);
            if (!state || typeof state !== 'object' || Array.isArray(state) || typeof state.updatePhase !== 'string')
                throw new Error('Invalid update response');
            for (const key of ['updateCandidate', 'updateOperation'])
                if (typeof state[key] !== 'undefined' && !isDecimalId(state[key])) throw new Error('Invalid update identity');
            this.lostWhileInstalling = 0;
            this.state = Object.assign({}, state);
            this.onState(this.state);
            this.schedule();
            return this.state;
        } catch (error) {
            if (error && error.code === 'unknownOp') {
                // Firmware from before the web updater: leave the section hidden.
                this.supported = false;
                this.stop();
                this.onState(null);
                return null;
            }
            // While installing, the device restarts and the socket drops: keep
            // asking rather than report a failure that has not happened.
            if (this.state && ['written', 'writing', 'verifying'].includes(this.state.updatePhase) &&
                error && ['closed', 'notConnected', 'timeout', 'busy'].includes(error.code) && this.lostWhileInstalling < 120) {
                ++this.lostWhileInstalling;
                this.onState(Object.assign({}, this.state, { restarting: true }));
                this.stop();
                this.timer = this.setTimeout(() => { this.timer = null; this.refresh().catch(() => {}); }, this.pollMs * 2);
                return null;
            }
            this.onError(error);
            throw error;
        }
    }
    refresh() { return this.request({}); }
    check() { return this.request({ updateAction: GIT_UPDATE_ACTION.check }); }
    install() {
        const s = this.state;
        if (!s || !s.updateCanInstall || !isDecimalId(s.updateCandidate)) return Promise.reject(new Error('Check for a release first'));
        return this.request({ updateAction: GIT_UPDATE_ACTION.install, updateCandidate: s.updateCandidate });
    }
    cancel() {
        const s = this.state;
        if (!s || !s.updateCancellable || !isDecimalId(s.updateOperation)) return Promise.reject(new Error('Nothing to cancel'));
        return this.request({ updateAction: GIT_UPDATE_ACTION.cancel, updateOperation: s.updateOperation });
    }
    // What to show: title, detail, progress (0-100 or null) and which buttons.
    static describe(s) {
        const out = { title: '', detail: '', progress: null, check: false, install: false, cancel: false };
        if (!s) return out;
        const reason = s.updateReason || '';
        const mb = s.updateSize ? ` (${(s.updateSize / 1048576).toFixed(1)} MB)` : '';
        if (s.restarting) {
            out.title = 'Restarting with the new firmware';
            out.detail = 'Keep the controller powered. This page reconnects when it is back.';
            return out;
        }
        if (s.updateBootPending) {
            out.title = 'Checking the new firmware';
            out.detail = 'The controller is confirming that the new firmware runs properly. If it does not, it rolls back to the previous version on its own.';
            return out;
        }
        if (!s.updateSupported) {
            out.title = 'GitHub updates are not available on this controller';
            out.detail = 'Use Update Firmware with a file instead.';
            return out;
        }
        if (s.updateReasonCode === GIT_UPDATE_REASON.development) {
            out.title = 'Development build';
            out.detail = `${s.updateInstalled || 'This firmware'} was not built from a release tag, so it cannot take stable releases from GitHub. ` +
                'Install a tagged release once with Update Firmware or USB; from then on, updates can come from GitHub.';
            return out;
        }
        const phase = s.updatePhase;
        switch (phase) {
            case 'queued': case 'checking':
                out.title = 'Checking GitHub for a newer release…';
                break;
            case 'available':
                if (s.updateCanInstall && s.updateVersion) {
                    out.title = `${s.updateVersion} is available${mb}`;
                    out.detail = `Installed: ${s.updateInstalled}. Installing downloads and verifies the release, saves your shades, then restarts the controller.`;
                    out.install = true;
                } else {
                    out.title = 'A release was found but cannot be installed';
                    out.detail = reason;
                }
                out.check = true;
                break;
            case 'current':
                out.title = 'This firmware is up to date';
                out.detail = `Installed: ${s.updateInstalled}.`;
                out.check = true;
                break;
            case 'preparing': case 'prepared': case 'downloading': case 'verifying': case 'writing': {
                const names = { preparing: 'Preparing', prepared: 'Preparing', downloading: 'Downloading', verifying: 'Verifying', writing: 'Installing' };
                out.title = `${names[phase]} ${s.updateVersion || 'the update'}…`;
                out.progress = Number.isInteger(s.updateProgress) ? s.updateProgress : 0;
                out.detail = s.updateCancellable ? 'Keep the controller powered.' : 'Keep the controller powered. This step cannot be cancelled.';
                if (s.updateRemainingMs) out.detail += ` Time limit: ${Math.ceil(s.updateRemainingMs / 60000)} min.`;
                break;
            }
            case 'retryWaiting':
                out.title = 'Connection problem; retrying';
                out.detail = `Retry ${s.updateRetries || 1} in ${Math.ceil((s.updateRetryMs || 0) / 1000)} s.`;
                break;
            case 'written':
                out.title = 'Installed; final checks before restart';
                out.detail = 'Keep the controller powered.';
                break;
            case 'cleanupPending':
                out.title = 'Finishing up…';
                break;
            case 'cancelled': case 'changed': case 'error':
                out.title = phase === 'cancelled' ? 'Update cancelled' : phase === 'changed' ? 'The release changed' : 'Update failed';
                out.detail = reason;
                out.check = true;
                break;
            case 'idle': default:
                out.title = `Installed: ${s.updateInstalled || 'unknown'}`;
                out.detail = 'Check GitHub for a newer stable release. Nothing is checked automatically.';
                out.check = phase !== 'unavailable';
                if (phase === 'unavailable') out.detail = reason;
                break;
        }
        out.cancel = !!s.updateCancellable;
        if (s.updateBusy) { out.check = false; out.install = false; }
        return out;
    }
}
// Battery: BatteryGauge::State numbers and the charge limits the device offers.
const BATTERY_STATE = { notMonitored: 0, initializing: 1, ready: 2, noBattery: 3, readFailed: 4, invalid: 5 };
// Charge limits (BatteryCharger::Limit). 60% is the lowest the charger allows
// and the recommendation for a controller that runs on USB all the time.
const CHARGE_LIMITS = [
    { value: 0, label: '100% (4.21 V)', detail: 'The charger\'s default. Use it only if the controller often runs on battery: a cell kept full wears out fastest. For a controller always on USB, choose 60%.' },
    { value: 1, label: '90% (4.13 V)', detail: 'Slightly less wear than 100%.' },
    { value: 2, label: '80% (4.06 V)', detail: 'Less wear; most of the backup runtime is kept.' },
    { value: 3, label: '70% (3.97 V)', detail: 'Much less wear.' },
    { value: 4, label: '60% (3.84 V), recommended', detail: 'Recommended when the controller is powered over USB all the time. It is the lowest the charger allows and gives the battery the longest life; backup runtime is about 60% of a full charge.' }
];
const CHARGE_LIMIT_RECOMMENDED = 4;
// null when the board has no battery to show; otherwise text for the status line.
function describeBattery(b) {
    if (!b || !Number.isInteger(b.batteryState)) return null;
    switch (b.batteryState) {
        case BATTERY_STATE.notMonitored: case BATTERY_STATE.noBattery: return null;
        case BATTERY_STATE.initializing: return { text: 'Starting…' };
        case BATTERY_STATE.readFailed: return { text: 'Unavailable (read failed)' };
        case BATTERY_STATE.invalid: return { text: 'Unavailable (invalid reading)' };
        case BATTERY_STATE.ready: {
            const ma = b.batteryMilliamps || 0;
            const flow = ma > 5 ? ', charging' : ma < -5 ? ', discharging' : '';
            return { percent: b.batteryPercent || 0, text: `${b.batteryPercent || 0}% (${((b.batteryMillivolts || 0) / 1000).toFixed(2)} V${flow})` };
        }
        default: return null;
    }
}
if (typeof module !== 'undefined' && module.exports) module.exports = { GitUpdateClient, GIT_UPDATE_ACTION, describeBattery, CHARGE_LIMITS, CHARGE_LIMIT_RECOMMENDED, BATTERY_STATE, isDecimalId };
