import os
Import("env")

# Define the folder and filename
DATA_FOLDER = "data-src"
FILENAME = "appversion"

# Construct the full path: /your/project/path/data-src/appversion
project_dir = env.get("PROJECT_DIR")
version_file_path = os.path.join(project_dir, DATA_FOLDER, FILENAME)

# Default fallback if something goes wrong
version = "0.0.0"

if os.path.exists(version_file_path):
    try:
        with open(version_file_path, "r") as f:
            version = f.read().strip()
            # Clean output for the PlatformIO console
            print(f"--- SUCCESS: Found version {version} in {version_file_path} ---")
    except Exception as e:
        print(f"--- ERROR: could not read version file: {e} ---")
else:
    print(f"--- ERROR: File NOT FOUND at {version_file_path} ---")

# Apply to the build environment
# We use escaped quotes so C++ treats it as a string literal "vX.X.X"
full_version_str = f'\\"v{version}\\"'

env.Append(CPPDEFINES=[
    ("FW_VERSION", full_version_str)
])
# Pure update compatibility identity only; no network/installation feature flag.
if env.get("PIOENV") == "lilygo-t-embed-cc1101":
    import json
    import sys
    from pathlib import Path
    sys.path.insert(0, os.path.join(project_dir, "tools"))
    from update_profiles import load_profile
    from update_identity import identity, tags_at_head, source_epoch
    from update_assets import verify_manifest
    from lilygo_manifest import source_record
    env.Append(LINKFLAGS=["-Wl,--undefined=espsomfyUpdateIdentity"])
    source = source_record(Path(project_dir))
    epoch=source_epoch(project_dir)
    env.Append(CPPDEFINES=[("ESPSOMFY_SOURCE_EPOCH",epoch)])
    record = identity(source, verify_manifest(Path(project_dir) / "data"),
                      tags_at_head(project_dir), load_profile(project_dir, env.get("PIOENV")))
    for name, value in (("ESPSOMFY_SOURCE_SHA", record["source_sha"]),
                        ("ESPSOMFY_RELEASE_TAG", record["version"]),
                        ("ESPSOMFY_APP_COMPAT", record["app_compat"])):
        env.Append(CPPDEFINES=[(name, '\\\"' + value + '\\\"')])
    build_dir = Path(env.subst("$BUILD_DIR"))
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "update-source.json").write_text(json.dumps(
        {"source": source, "identity": record}, sort_keys=True, indent=2) + "\n")
