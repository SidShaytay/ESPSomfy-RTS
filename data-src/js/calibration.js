// A source-bound draft. This client never retries movement, invents an
// observation, or changes the saved shade model while measurements are taken.
class LiftCalibrationClient {
    constructor(proto, options = {}) {
        this.proto = proto;
        this.onState = options.onState || (() => {});
        this.onError = options.onError || (() => {});
        this.token = 0;
        this.state = null;
        this.epoch = 0;
        this.issued = 0;
        this.mutating = false;
    }
    async request(fields, begin = false) {
        const epoch = this.epoch, sequence = ++this.issued;
        try {
            const state = await this.proto.op('liftCalibration', fields);
            if (epoch !== this.epoch || sequence !== this.issued) return null;
            if (!state || Array.isArray(state) || !Number.isInteger(state.calibrationSession) ||
                state.calibrationSession < 1 || state.calibrationSession > 0xffffffff ||
                !Number.isInteger(state.calibrationStage) || state.calibrationStage < 0 || state.calibrationStage > 255)
                throw new Error('Invalid calibration response');
            if (!begin && state.calibrationSession !== this.token) throw new Error('Calibration session changed');
            this.token = state.calibrationSession;
            this.state = Object.assign({}, state);
            this.onState(this.state);
            return this.state;
        } catch (error) {
            if (epoch === this.epoch) this.onError(error);
            throw error;
        }
    }
    async mutation(fields, begin = false) {
        if (this.mutating) throw new Error('A calibration action is still pending');
        const epoch = this.epoch;
        this.mutating = true;
        try { return await this.request(fields, begin); }
        finally { if (epoch === this.epoch) this.mutating = false; }
    }
    async begin(id, kind) {
        if (this.token) throw new Error('Close the current calibration first');
        return this.mutation({ calibrationAction: 100, shadeId: id, calibrationKind: kind }, true);
    }
    async act(action, human = false) {
        if (!this.token) throw new Error('No current calibration session');
        return this.mutation({ calibrationAction: action, calibrationSession: this.token, calibrationHuman: human === true });
    }
    async poll() {
        if (!this.token) return null;
        return this.request({ calibrationAction: 101, calibrationSession: this.token });
    }
    async close() {
        if (this.token) await this.act(10, false);
        this.disconnect();
    }
    disconnect() {
        ++this.epoch;
        ++this.issued;
        this.token = 0;
        this.state = null;
        this.mutating = false;
    }
}
if (typeof module !== 'undefined' && module.exports) module.exports = { LiftCalibrationClient };

class LiftCalibrationDialog {
    constructor(proto, onClose = () => {}, dom = document) {
        this.proto = proto;
        this.dom = dom;
        this.onClose = onClose;
        this.pending = false;
        this.polling = false;
        this.dialog = dom.createElement('dialog');
        this.dialog.className = 'calibration-dialog';
        this.heading = dom.createElement('h2');
        this.hint = dom.createElement('p');
        this.confidence = dom.createElement('p');
        this.confidence.className = 'calibration-confidence';
        this.values = dom.createElement('div');
        this.controls = dom.createElement('div');
        this.controls.className = 'calibration-controls';
        this.error = dom.createElement('p');
        this.error.setAttribute('role', 'alert');
        this.error.className = 'calibration-error';
        const footer = dom.createElement('p');
        footer.className = 'calibration-note';
        footer.textContent = 'Discarding a draft leaves an accepted endpoint move running to its limit. Use Stop while the shutter is moving.';
        this.dialog.append(this.heading, this.hint, this.confidence, this.values, this.controls, this.error, footer);
        this.client = new LiftCalibrationClient(proto, {
            onState: () => this.render(), onError: (error) => this.showError(error)
        });
        this.dialog.addEventListener('cancel', (event) => { event.preventDefault(); this.close(); });
    }
    button(label, action) {
        const button = this.dom.createElement('button');
        button.type = 'button';
        button.textContent = label;
        button.disabled = this.pending;
        button.addEventListener('click', action);
        this.controls.append(button);
    }
    open(id, name) {
        this.id = id;
        this.heading.textContent = `Calibrate ${name || 'shutter'}`;
        this.hint.textContent = 'Stand where you can see the whole shutter. Choose its mechanism. This measures software positioning and does not reprogram motor limits.';
        this.button('Continuous roller / direct travel', () => this.begin(1));
        this.button('Segmented shutter / slack', () => this.begin(2));
        this.button('Cancel', () => this.destroy());
        this.dom.body.append(this.dialog);
        this.dialog.showModal();
        this.timer = setInterval(() => this.poll(), 500);
    }
    async begin(kind) {
        if (this.pending) return;
        this.pending = true;
        this.controls.querySelectorAll('button').forEach((button) => { button.disabled = true; });
        this.error.textContent = '';
        try { await this.client.begin(this.id, kind); }
        catch (_) { /* The client exposes the refusal without retrying it. */ }
        finally {
            this.pending = false;
            if (this.client.state) this.render();
            else this.controls.querySelectorAll('button').forEach((button) => { button.disabled = false; });
        }
    }
    showError(error) { this.error.textContent = error.message || 'Calibration request failed'; }
    render() {
        const state = this.client.state;
        if (!state) return;
        if (state.calibrationStage === 0) { this.destroy(); return; }
        this.heading.textContent = state.calibrationTitle || 'Calibration';
        this.hint.textContent = state.calibrationHint || '';
        const shade = this.proto.model && this.proto.model.shades && this.proto.model.shades.get(this.id);
        this.confidence.textContent = shade && shade.calibrationReferenceRequired
            ? 'Position uncertain. Open fully and confirm in this guide before percentage moves.'
            : shade && shade.endpointConfirmed
            ? 'Current endpoint was confirmed by the operator.'
            : 'Position is estimated. The motor does not return position feedback.';
        const measurementKey = JSON.stringify(['calibrationKind', 'upTime', 'downTime', 'upTakeupMs', 'downTakeupMs',
            'upMidpointMs', 'downMidpointMs', 'upSettlingMs', 'downSettlingMs', 'upReversalMs', 'downReversalMs', 'calibrationReversalsKnown'].map((key) => state[key]));
        if (measurementKey !== this.measurementKey) {
            this.measurementKey = measurementKey;
            this.values.replaceChildren();
        if (state.upTime > 0 && state.downTime > 0) {
            const table = this.dom.createElement('table');
            const header = this.dom.createElement('tr');
            for (const label of ['Measurement (ms)', 'Up', 'Down']) {
                const cell = this.dom.createElement('th'); cell.textContent = label; header.append(cell);
            }
            table.append(header);
            const rows = [['Total motor time', 'upTime', 'downTime'], ['Visible halfway', 'upMidpointMs', 'downMidpointMs']];
            if (state.calibrationKind === 2) rows.push(['Take-up', 'upTakeupMs', 'downTakeupMs'],
                ['Final settling', 'upSettlingMs', 'downSettlingMs'], ['Reversal take-up', 'upReversalMs', 'downReversalMs']);
            for (const [label, up, down] of rows) {
                const row = this.dom.createElement('tr');
                const unknown = up === 'upReversalMs' && !state.calibrationReversalsKnown;
                for (const value of [label, unknown ? 'Not measured' : state[up] || 0, unknown ? 'Not measured' : state[down] || 0]) {
                    const cell = this.dom.createElement('td'); cell.textContent = String(value); row.append(cell);
                }
                table.append(row);
            }
            this.values.append(table);
        }
        }
        const choices = (Array.isArray(state.calibrationChoices) ? state.calibrationChoices : []).filter((choice) =>
            choice && Number.isInteger(choice.calibrationAction) && choice.calibrationAction >= 0 && choice.calibrationAction <= 255 && typeof choice.name === 'string').slice(0, 4);
        const controlKey = JSON.stringify(choices);
        // Preserve pressed/focused controls across status polls. Replacing a
        // button between pointer-down and pointer-up can lose an observation.
        if (controlKey !== this.controlKey) {
            this.controlKey = controlKey;
            this.controls.replaceChildren();
            for (const choice of choices) this.button(choice.name, (event) => this.action(choice.calibrationAction, event.isTrusted));
        }
        this.controls.querySelectorAll('button').forEach((button) => { button.disabled = this.pending; });
    }
    async action(action, human) {
        if (this.pending) return;
        if (action === 10) { await this.close(); return; }
        this.pending = true; this.error.textContent = ''; this.render();
        try { await this.client.act(action, human); }
        catch (_) { /* Refreshing status is safe; movement is never retried. */ }
        finally { this.pending = false; this.render(); }
    }
    async poll() {
        if (this.pending || this.polling || !this.client.token) return;
        if (!this.proto.ready) {
            this.client.disconnect(); clearInterval(this.timer);
            this.controls.replaceChildren(); this.button('Dismiss', () => this.destroy());
            this.showError(new Error('Connection lost. Measurements were not saved; a move may continue to its endpoint.'));
            return;
        }
        this.polling = true;
        try { await this.client.poll(); }
        catch (_) { /* Keep the last view and the explicit Close action. */ }
        finally { this.polling = false; }
    }
    async close() {
        if (this.pending) return;
        if (!this.client.token) { this.destroy(); return; }
        this.pending = true; this.render();
        try { await this.client.close(); this.destroy(); }
        catch (_) { /* Do not report a remote cancellation as completed. */ }
        finally { this.pending = false; this.render(); }
    }
    destroy() {
        if (this.destroyed) return;
        this.destroyed = true;
        clearInterval(this.timer);
        this.client.disconnect();
        this.dialog.remove();
        this.onClose();
    }
}
if (typeof module !== 'undefined' && module.exports) module.exports.LiftCalibrationDialog = LiftCalibrationDialog;
