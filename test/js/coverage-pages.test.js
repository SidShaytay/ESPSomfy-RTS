// Keep coverage publishing optional without disabling the test/coverage jobs.
const test = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const { runInNewContext } = require('node:vm');

const workflow = readFileSync(join(__dirname, '../../.github/workflows/build.yaml'), 'utf8');
const condition = workflow.match(/^  coverage-pages:\n    if: (.+)$/m)?.[1];

// This condition uses boolean fields, == and &&, whose semantics agree in
// JavaScript and Actions for these fixtures. Missing has_pages must fail closed.
for (const [name, eventName, ref, hasPages, expected] of [
    ['enabled Pages on main', 'push', 'refs/heads/main', true, true],
    ['disabled Pages on main', 'push', 'refs/heads/main', false, false],
    ['missing Pages flag', 'push', 'refs/heads/main', undefined, false],
    ['null Pages flag', 'push', 'refs/heads/main', null, false],
    ['pull request', 'pull_request', 'refs/heads/main', true, false],
    ['release', 'release', 'refs/heads/main', true, false],
    ['another branch', 'push', 'refs/heads/feature', true, false],
]) {
    test(`coverage publishing: ${name}`, () => {
        assert.ok(condition, 'coverage-pages job must have an explicit condition');
        const github = {
            event_name: eventName,
            ref,
            event: { repository: { has_pages: hasPages } },
        };
        assert.equal(runInNewContext(condition, { github }), expected);
    });
}
