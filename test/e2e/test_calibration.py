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


def rf_stop_test_supported(shade):
    return (shade.get('shadeType', 0) in (0, 4) and not shade.get('tiltType', 0)
        and shade.get('position', 0) == 100 and not shade.get('direction', 0)
        and not shade.get('flipPosition', False) and not shade.get('flipCommands', False)
        and not shade.get('calibrationReferenceRequired', False)
        and shade.get('upTime', 0) >= 8000)


@pytest.mark.parametrize('change', [
    {'upTime': 1000}, {'upTime': 0}, {'position': 0}, {'position': 50},
    {'direction': -1}, {'flipPosition': True}, {'flipCommands': True},
    {'calibrationReferenceRequired': True}, {'tiltType': 1}, {'shadeType': 5},
])
def test_rf_timing_guard_rejects_endpoint_ambiguous_setup(change):
    shade = {'shadeType': 4, 'position': 100, 'upTime': 20000}
    assert rf_stop_test_supported(shade)
    shade.update(change)
    assert not rf_stop_test_supported(shade)


def test_release_rf_start_and_stop_outcomes(connect, schema, radio_shade, record_property):
    """Opt-in RF timing under exclusive guide ownership; no physical markers.

    Requires an idle, unflipped roller/shutter reported closed and >=8s Up.
    Stops early in that full stroke, never after a timed endpoint. If admission
    or evidence is ambiguous, no My is issued and the endpoint run continues.
    """
    required(schema)
    c, shades, _ = synced_model(connect, schema)
    shade = next(s for s in shades if s['shadeId'] == radio_shade)
    if not rf_stop_test_supported(shade):
        pytest.skip('RF timing requires an idle closed lift-only shutter, known reference, no flips and Up >=8s')
    assert is_ok(schema, op(c, schema, 'motionDiagnostics', {'traceMask': 0}))
    assert is_ok(schema, op(c, schema, 'motionDiagnostics', {'traceMask': 2, 'traceDuration': 30000}))
    begin = op(c, schema, 'liftCalibration', {'calibrationAction': 100,
        'shadeId': radio_shade, 'calibrationKind': 2})
    assert is_ok(schema, begin), 'exclusive calibration admission refused before RF'
    token = schema.expand(begin['d'])['calibrationSession']
    cursor, records = '0', []

    def guide(action):
        return op(c, schema, 'liftCalibration', {'calibrationAction': action,
            'calibrationSession': token, 'calibrationHuman': False})

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
        issued_at = time.monotonic()
        assert is_ok(schema, guide(0)) # Open only; no invented endpoint marker.
        collect_finished(1)
        assert len(records) == 2 and records[0].get('traceKind', 0) == 0 and records[1].get('traceKind', 0) == 1
        assert records[0]['traceTicket'] == records[1]['traceTicket']
        # Fresh model plus a short elapsed budget prevent My at a timed idle
        # endpoint. The session itself correlates its exact start ticket and
        # prevents any other controller RF throughout the test.
        reader, latest, _ = synced_model(connect, schema)
        reader.close()
        current = next(s for s in latest if s['shadeId'] == radio_shade)
        assert current.get('direction', 0) == -1 and current.get('position', 0) > 0
        assert time.monotonic() - issued_at < shade['upTime'] / 4000, 'Stop test window elapsed; leave full Open running'
        assert is_ok(schema, guide(1))
        collect_finished(2)
        starts = {r.get('traceTicket', 0): r for r in records if r.get('traceKind', 0) == 0}
        finishes = [r for r in records if r.get('traceKind', 0) == 1]
        assert len(records) == 4 and len(starts) == len(finishes) == 2
        for i, finish in enumerate(finishes):
            start = starts[finish['traceTicket']]
            duration = (finish.get('traceAt', 0) - start.get('traceAt', 0)) & 0xffffffff
            assert 0 < duration < 10000
            record_property(('direction' if i == 0 else 'stop') + '_rf_ms', duration)
        assert ((starts[finishes[1]['traceTicket']]['traceAt'] - finishes[0]['traceAt']) & 0xffffffff) < 10000
        reader, stopped, _ = synced_model(connect, schema)
        reader.close()
        stopped = next(s for s in stopped if s['shadeId'] == radio_shade)
        assert not stopped.get('direction', 0)
        assert stopped.get('position', 0) == stopped.get('target', 0)
        time.sleep(0.25)
        reader, stable, _ = synced_model(connect, schema)
        reader.close()
        stable = next(s for s in stable if s['shadeId'] == radio_shade)
        assert not stable.get('direction', 0)
        assert stable.get('position', 0) == stopped.get('position', 0), 'stopped model drifted'
        record_property('stopped_model_position', stable.get('position', 0))
        record_property('physical_position', 'not observed')
    finally:
        # Cancel never invents a Stop or endpoint; ambiguous movement runs to
        # its limit. The old profile is retained and no observation is saved.
        guide(10)
        op(c, schema, 'motionDiagnostics', {'traceMask': 0})
