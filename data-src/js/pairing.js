// Step-by-step pairing and unpairing for one RF shade, as the LCD's
// programming guide does it.  It only sends Prog when the user presses Send,
// never retries on its own, and records the "paired" marker only after the
// user says the motor answered.  The marker is a note in the configuration;
// the motor's own memory is what really decides.  The minify step joins this
// file into index.js.
class PairingGuide {
    // opts: pair (true to pair, false to unpair), door (garage door motor),
    // send(repeats) -> Promise, mark(paired) -> Promise.
    constructor(opts) {
        this.pair = opts.pair !== false;
        this.door = !!opts.door;
        this.send = opts.send;
        this.mark = opts.mark;
        this.step = 'memory';
        this.repeats = 1;
        this.error = '';
        this.busy = false;
    }
    setRepeats(value) {
        const n = parseInt(value, 10);
        if (Number.isInteger(n)) this.repeats = Math.min(127, Math.max(1, n));
        return this.repeats;
    }
    // One of the button ids view() returned.  Resolves to the new step;
    // 'closed' means close the dialog.
    async act(id) {
        if (this.busy) return this.step;
        switch (`${this.step}:${id}`) {
            case 'memory:ready': this.step = 'send'; break;
            case 'send:send':
                this.busy = true; this.error = '';
                try { await this.send(this.repeats); this.step = 'observe'; }
                catch (err) { this.error = (err && (err.desc || err.message)) || 'The request was refused'; this.step = 'refused'; }
                finally { this.busy = false; }
                break;
            case 'observe:yes': this.step = 'marker'; break;
            case 'observe:no': case 'refused:back': this.step = 'send'; this.error = ''; break;
            case 'marker:save':
                this.busy = true; this.error = '';
                try { await this.mark(this.pair); this.step = 'closed'; }
                catch (err) { this.error = (err && (err.desc || err.message)) || 'The marker was not saved'; }
                finally { this.busy = false; }
                break;
            default:
                if (id === 'close') this.step = 'closed';
                break;
        }
        return this.step;
    }
    // What to show for the current step: a title, paragraphs, whether the
    // repeat field shows, and buttons [{ id, label, primary }].
    view() {
        const thing = this.door ? 'door' : 'shade';
        const verb = this.pair ? 'pair' : 'unpair';
        const close = { id: 'close', label: 'Cancel' };
        switch (this.step) {
            case 'memory':
                return {
                    title: `Step 1 of 3: open the motor's memory`,
                    text: this.door ?
                        ['Open the garage door motor\'s memory as its own instructions describe.', 'Nothing has been sent yet.'] :
                        [`Using a remote that already controls this ${thing}, press and hold its Prog button (usually on the back) until the ${thing} jogs briefly up and down.`,
                         'That jog means the motor is listening for a new remote. It stays listening for about two minutes.',
                         'Nothing has been sent yet.'],
                    buttons: [{ id: 'ready', label: `The ${thing} jogged`, primary: true }, close]
                };
            case 'send':
                return {
                    title: `Step 2 of 3: send Prog from ESPSomfy`,
                    text: [this.pair ?
                        `This sends Prog from this ${thing}'s own remote address, so the motor learns it.` :
                        `This sends Prog from this ${thing}'s remote address while the memory is open, so the motor forgets it.`,
                        this.door ? 'A one-button door motor may move when it receives this.' :
                        'Most motors answer one press. Some need a longer press: raise the repeat count if the first send gets no jog.'],
                    repeat: true,
                    buttons: [{ id: 'send', label: this.busy ? 'Sending…' : 'Send Prog', primary: true }, close]
                };
            case 'observe':
                return {
                    title: `Step 3 of 3: did the ${thing} jog again?`,
                    text: [`ESPSomfy sent the command. It cannot hear the motor, so only you can tell whether it worked: a second jog means the motor ${this.pair ? 'learned' : 'forgot'} the remote.`],
                    buttons: [{ id: 'yes', label: 'Yes, it jogged', primary: true }, { id: 'no', label: 'No, try again' }, close]
                };
            case 'marker':
                return {
                    title: this.pair ? 'Paired' : 'Unpaired',
                    text: [`Mark this ${thing} as ${verb}ed in ESPSomfy? This only records what you saw; it sends nothing.`],
                    error: this.error,
                    buttons: [{ id: 'save', label: this.pair ? 'Mark as paired' : 'Mark as unpaired', primary: true }, { id: 'close', label: 'Close' }]
                };
            case 'refused':
                return {
                    title: 'Nothing was sent',
                    text: [this.error, 'Check the controller and the motor before trying again.'],
                    buttons: [{ id: 'back', label: 'Back', primary: true }, { id: 'close', label: 'Close' }]
                };
            default:
                return { title: '', text: [], buttons: [] };
        }
    }
}
if (typeof module !== 'undefined' && module.exports) module.exports = { PairingGuide };
