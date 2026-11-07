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
# Exact-board update identity is derived before compilation and checked again by
# the release packager. A dirty/untagged build is explicitly a development build.
if env.get("PIOENV") == "lilygo-t-embed-cc1101-plus":
    import json
    from pathlib import Path
    import sys
    sys.path.insert(0, os.path.join(project_dir, "tools"))
    from lilygo_manifest import source_record
    from update_identity import identity, tags_at_head, git
    source = source_record(Path(project_dir))
    build_identity = identity(source, version, tags_at_head(project_dir))
    source_epoch = int(git(project_dir, "show", "-s", "--format=%ct", "HEAD"))
    if source_epoch < 1577836800:
        raise ValueError("invalid source timestamp for verified update clock floor")
    env.Append(CPPDEFINES=[("ESPSOMFY_SOURCE_EPOCH", source_epoch)])
    for name, value in (("ESPSOMFY_SOURCE_SHA", build_identity["source_sha"]),
                        ("ESPSOMFY_RELEASE_TAG", build_identity["version"]),
                        ("ESPSOMFY_APP_COMPAT", build_identity["app_compat"])):
        env.Append(CPPDEFINES=[(name, '\\\"' + value + '\\\"')])
    build_dir = Path(env.subst("$BUILD_DIR"))
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "update-source.json").write_text(
        json.dumps({"source": source, "identity": build_identity}, sort_keys=True, indent=2) + "\n")
