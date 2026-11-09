"""Canonical-only prebuild: one immutable flash table, never a filesystem image."""
Import("env")
if env.get("PIOENV") == "lilygo-t-embed-cc1101":
    from pathlib import Path
    import sys
    project = Path(env.get("PROJECT_DIR"))
    sys.path.insert(0, str(project / "tools"))
    from bundled_web_assets import generate
    output = Path(env.subst("$BUILD_DIR"))
    record = generate(project / "data", output / "BundledWebData.h")
    env.Append(CPPPATH=[str(output)])
    print("[bundle] Firmware contains exact web assets: " + record['compatibility'])
