#!/usr/bin/env bash
# Exercise the actual release workflow shell block using real, local-only Git.
set -euo pipefail
repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
scratch="$(mktemp -d)"
trap 'rm -rf -- "$scratch"' EXIT
export GIT_CONFIG_COUNT=0 GIT_CONFIG_NOSYSTEM=1 GIT_CONFIG_GLOBAL=/dev/null
export GIT_ALLOW_PROTOCOL=file GIT_TERMINAL_PROMPT=0
python3 - "$repo/.github/workflows/build.yaml" "$scratch/release-step.sh" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();step=s.index('      - name: Commit version update\n')
a=s.index('        run: |\n',step)+len('        run: |\n');lines=[]
for line in s[a:].splitlines():
    if line and not line.startswith('          '):break
    lines.append(line[10:])
assert lines and any('git push origin' in line for line in lines)
Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
PY
mkdir "$scratch/bin"
cat > "$scratch/bin/git" <<'WRAPPER'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "$RELEASE_GIT_LOG"
exec /usr/bin/git "$@"
WRAPPER
chmod +x "$scratch/bin/git"
for scenario in unchanged changed; do
  base="$scratch/$scenario";mkdir -p "$base/work/.github"
  /usr/bin/git -c init.defaultBranch=main init -q --bare "$base/remote.git"
  /usr/bin/git -c init.defaultBranch=main init -q "$base/work"
  for file in src/ConfigSettings.h data-src/appversion data-src/index.html data-src/index.js .github/set_version.sh; do
    mkdir -p "$base/work/$(dirname "$file")";cp "$repo/$file" "$base/work/$file"
  done
  (
    cd "$base/work"
    /usr/bin/git config user.name 'Local release fixture'
    /usr/bin/git config user.email 'release-test@example.invalid'
    /usr/bin/git config commit.gpgsign false
    /usr/bin/git add .;/usr/bin/git commit -qm 'Versioned source'
    /usr/bin/git remote add origin "$base/remote.git"
    /usr/bin/git push -q origin HEAD:main
    before="$(/usr/bin/git rev-parse HEAD)"
    export TARGET=main TAG="v$(cat data-src/appversion)"
    /usr/bin/git tag "$TAG"
    [[ "$scenario" == unchanged ]] || TAG=v99.0.7
    export RELEASE_GIT_LOG="$base/calls.log"
    PATH="$scratch/bin:$PATH" bash -e "$scratch/release-step.sh"
    after="$(/usr/bin/git rev-parse HEAD)"
    remote="$(/usr/bin/git --git-dir="$base/remote.git" rev-parse refs/heads/main)"
    [[ "$remote" == "$after" ]]
    /usr/bin/git diff --exit-code --quiet
    if [[ "$scenario" == unchanged ]]; then
      [[ "$after" == "$before" ]]
      ! grep -Eq '^(commit|push)( |$)' "$RELEASE_GIT_LOG"
    else
      [[ "$after" != "$before" && "$(/usr/bin/git rev-list --count "$before..$after")" == 1 ]]
      [[ "$(cat data-src/appversion)" == 99.0.7 ]]
      grep -Eq '^commit -am chore: bump version to v99.0.7$' "$RELEASE_GIT_LOG"
      grep -Eq '^push origin HEAD:main$' "$RELEASE_GIT_LOG"
    fi
  )
done
printf 'Release version step: clean tagged no-op and changed local commit/push passed\n'
