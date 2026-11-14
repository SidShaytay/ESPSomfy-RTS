"""Release-board checks; RF requires explicit --radio --shade options."""
import time
import pytest
from test_commands import synced_model, op, is_ok


def required(schema):
    if 'liftCalibration' not in schema.type_numbers or 'motionDiagnostics' not in schema.type_numbers:
        pytest.skip('requires guided-calibration firmware')


def test_trace_is_runtime_configurable_and_expires_without_reboot(connect, schema):
    required(schema)
    c, _, _ = synced_model(connect, schema)
    assert is_ok(schema, op(c, schema, 'motionDiagnostics', {'traceMask': 0}))
    result = schema.expand(op(c, schema, 'motionDiagnostics', {'traceMask': 7, 'traceDuration': 100})['d'])
    assert result.get('traceEnabled') is True
    time.sleep(0.15)
    result = schema.expand(op(c, schema, 'motionDiagnostics', {'traceCursor': '0'})['d'])
    assert not result.get('traceEnabled', False)
    assert isinstance(result['traceCursor'], str)
    assert is_ok(schema, op(c, schema, 'motionDiagnostics', {'traceMask': 0}))


@pytest.mark.parametrize('fields', [
    {'traceMask': 8, 'traceDuration': 100}, {'traceMask': 1, 'traceDuration': 0},
    {'traceMask': 1, 'traceDuration': 900001}, {'traceCursor': '-1'},
    {'traceCursor': '18446744073709551616'}, {'traceUsb': True},
])
def test_invalid_trace_arguments_are_refused(connect, schema, fields):
    required(schema)
    c, _, _ = synced_model(connect, schema)
    assert not is_ok(schema, op(c, schema, 'motionDiagnostics', fields))


def test_calibration_begin_poll_and_discard_do_not_send_rf_or_change_profile(connect, schema):
    required(schema)
    c, shades, _ = synced_model(connect, schema)
    candidates = [s for s in shades if not s.get('tiltType', 0) and s.get('remoteAddress')]
    if not candidates:
        pytest.skip('no eligible lift-only shutter')
    shade = candidates[0]
    before = {s['shadeId']: s for s in shades}
    reply = op(c, schema, 'liftCalibration', {'calibrationAction': 100,
        'shadeId': shade['shadeId'], 'calibrationKind': 2})
    assert is_ok(schema, reply), 'idle calibration admission failed'
    token = schema.expand(reply['d'])['calibrationSession']
    try:
        assert is_ok(schema, op(c, schema, 'liftCalibration', {
            'calibrationAction': 101, 'calibrationSession': token}))
        # Automation must never claim to have observed an endpoint.
        assert not is_ok(schema, op(c, schema, 'liftCalibration', {
            'calibrationAction': 2, 'calibrationSession': token, 'calibrationHuman': False}))
    finally:
        assert is_ok(schema, op(c, schema, 'liftCalibration', {
            'calibrationAction': 10, 'calibrationSession': token}))
    c.close()
    time.sleep(0.5)
    _, after, _ = synced_model(connect, schema)
    fields = ['upTime', 'downTime', 'calibrationKind', 'lastRollingCode',
        'upTakeupMs', 'downTakeupMs', 'upMidpointMs', 'downMidpointMs',
        'upSettlingMs', 'downSettlingMs', 'upReversalMs', 'downReversalMs']
    for shade in after:
        previous = before[shade['shadeId']]
        for field in fields:
            assert shade.get(field, schema.default(field)) == previous.get(field, schema.default(field)), field


def test_release_rf_start_and_stop_outcomes(connect, schema, radio_shade, record_property):
    """Opt-in RF test: move briefly, wait for the full start, then Stop once.

    Reports actual controller timing. Leaves the shutter stopped where it is;
    it does not claim a physical position or return to a guessed position.
    """
    required(schema)
    c, shades, _ = synced_model(connect, schema)
    shade = next(s for s in shades if s['shadeId'] == radio_shade)
    assert not shade.get('direction', 0), 'start with an idle shutter'
    assert is_ok(schema, op(c, schema, 'motionDiagnostics', {'traceMask': 0}))
    assert is_ok(schema, op(c, schema, 'motionDiagnostics', {'traceMask': 2, 'traceDuration': 30000}))
    cursor, records = '0', []

    def collect_finished(after_count):
        nonlocal cursor
        end = time.monotonic() + 10
        while time.monotonic() < end:
            response = op(c, schema, 'motionDiagnostics', {'traceCursor': cursor})
            assert is_ok(schema, response)
            state = schema.expand(response['d'])
            assert not state.get('traceOverflow', False), 'RF evidence was overwritten'
            cursor = state['traceCursor']
            records.extend(state.get('traceRecords', []))
            assert not any(r.get('traceKind', 0) in (2, 3) for r in records), 'RF dropped or aborted'
            if sum(r.get('traceKind', 0) == 1 for r in records) >= after_count:
                return
            time.sleep(0.05)
        pytest.fail('No full RF completion within 10 seconds')

    try:
        command = 'Down' if shade.get('position', 0) < 50 else 'Up'
        assert is_ok(schema, op(c, schema, 'shadeCommand', {'shadeId': radio_shade, 'command': command}))
        collect_finished(1)
        time.sleep(1)
        assert is_ok(schema, op(c, schema, 'shadeCommand', {'shadeId': radio_shade, 'command': 'My'}))
        collect_finished(2)
        starts = {r.get('traceTicket', 0): r for r in records if r.get('traceKind', 0) == 0}
        finishes = [r for r in records if r.get('traceKind', 0) == 1]
        assert len(starts) == len(finishes) == 2, 'unexpected concurrent RF; rerun in a quiet period'
        for i, finish in enumerate(finishes):
            start = starts[finish['traceTicket']]
            duration = (finish.get('traceAt', 0) - start.get('traceAt', 0)) & 0xffffffff
            assert 0 < duration < 10000
            record_property(('direction' if i == 0 else 'stop') + '_rf_ms', duration)
        assert starts[finishes[1]['traceTicket']]['traceAt'] >= finishes[0]['traceAt']
        record_property('physical_position', 'not observed')
    finally:
        # Never retry a movement or My after a failed/ambiguous outcome.
        op(c, schema, 'motionDiagnostics', {'traceMask': 0})
