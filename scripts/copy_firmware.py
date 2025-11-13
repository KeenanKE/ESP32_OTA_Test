import os
import shutil
import glob

try:
    # Provided by PlatformIO/SCons at build time. Wrap in try/except so
    # static analyzers or editors that don't have SCons installed won't fail.
    from SCons.Script import Import  # type: ignore

    Import("env")
except Exception:
    # Running outside of PlatformIO (for example in the editor), SCons isn't
    # available. Create a placeholder so the rest of the file can be parsed.
    env = None


def after_build(source, target, env):
    project_dir = env["PROJECT_DIR"]
    build_dir = env.subst("$BUILD_DIR")
    releases_dir = os.path.join(project_dir, "releases")
    os.makedirs(releases_dir, exist_ok=True)

    # Look for firmware.bin and firmware.elf in the build directory
    bin_path = os.path.join(build_dir, "firmware.bin")
    elf_path = os.path.join(build_dir, "firmware.elf")

    copied = []
    if os.path.isfile(bin_path):
        dst = os.path.join(releases_dir, "firmware.bin")
        shutil.copy2(bin_path, dst)
        copied.append(dst)

    if os.path.isfile(elf_path):
        dst = os.path.join(releases_dir, "firmware.elf")
        shutil.copy2(elf_path, dst)
        copied.append(dst)

    if copied:
        print("\n[copy_firmware] Copied build artifacts:")
        for p in copied:
            print("  -", p)
    else:
        print("\n[copy_firmware] No firmware.bin or firmware.elf found in", build_dir)


# Attach to the ELF target so this runs after a successful link/build
try:
    env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.elf"), after_build)
except Exception:
    # If env isn't available (e.g. editor linting), ignore.
    pass
