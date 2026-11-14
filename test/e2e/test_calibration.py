"""Release-board calibration admission and trace checks. No motor commands."""
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
