import os
import shutil
import glob

# This script is designed to be run by PlatformIO's build system.
# It copies the compiled firmware files (.bin and .elf) from the build
# directory to a 'releases' folder in the project's root.

try:
    # 'SCons' is the build tool that PlatformIO is based on.
    # We import 'Import' to get access to the build environment variables.
    # This line will only work when the script is run by PlatformIO.
    from SCons.Script import Import  # type: ignore

    # The 'env' object is the gateway to the build environment. It contains
    # paths, build flags, and other important information.
    Import("env")
except Exception:
    # If this script is opened in a regular editor, 'SCons' won't be available,
    # causing an error. This 'except' block creates a placeholder 'env' variable
    # to prevent the script from crashing the editor's linter/analyzer.
    env = None


def after_build(source, target, env):
    """
    This function is executed after PlatformIO successfully builds the firmware.
    'source': A list of the source files for the action.
    'target': A list of the target files for the action (in this case, the .elf file).
    'env': The build environment object.
    """
    # Get the project's root directory from the environment variables.
    project_dir = env["PROJECT_DIR"]
    # Get the build output directory (e.g., '.pio/build/esp32dev').
    build_dir = env.subst("$BUILD_DIR")
    # Define the path for our 'releases' folder.
    releases_dir = os.path.join(project_dir, "releases")

    # Create the 'releases' directory if it doesn't already exist.
    # 'exist_ok=True' prevents an error if the directory is already there.
    os.makedirs(releases_dir, exist_ok=True)

    # Construct the full paths to the firmware files we want to copy.
    # The .bin file is used for OTA updates.
    bin_path = os.path.join(build_dir, "firmware.bin")
    # The .elf file contains debug symbols and is useful for advanced debugging.
    elf_path = os.path.join(build_dir, "firmware.elf")

    copied = []
    # Check if the firmware.bin file actually exists before trying to copy it.
    if os.path.isfile(bin_path):
        # Define the destination path.
        dst = os.path.join(releases_dir, "firmware.bin")
        # Copy the file. 'shutil.copy2' also copies metadata like timestamps.
        shutil.copy2(bin_path, dst)
        copied.append(dst)

    # Check if the firmware.elf file exists.
    if os.path.isfile(elf_path):
        dst = os.path.join(releases_dir, "firmware.elf")
        shutil.copy2(elf_path, dst)
        copied.append(dst)

    # --- Generate version.txt file ---
    # Extract the FIRMWARE_VERSION from the build defines.
    # PlatformIO stores preprocessor defines in CPPDEFINES.
    version = None

    # CPPDEFINES can be a list of strings or tuples
    for define in env.get("CPPDEFINES", []):
        # Check if it's a tuple like ('FIRMWARE_VERSION', '\\"1.0.0\\"')
        if isinstance(define, tuple) and len(define) == 2:
            if define[0] == "FIRMWARE_VERSION":
                # Remove surrounding quotes and backslashes
                # The value comes as '\\"1.0.0\\"' and we need just '1.0.0'
                version = define[1].replace("\\", "").replace('"', "").replace("'", "")
                break
        # Check if it's a string like 'FIRMWARE_VERSION="1.0.0"'
        elif isinstance(define, str) and define.startswith("FIRMWARE_VERSION="):
            version = (
                define.replace("FIRMWARE_VERSION=", "")
                .replace("\\", "")
                .replace('"', "")
                .replace("'", "")
            )
            break

    if version:
        version_file_path = os.path.join(releases_dir, "version.txt")
        with open(version_file_path, "w") as f:
            f.write(version)
        copied.append(version_file_path)
        print(f"[copy_firmware] Generated version.txt with version: {version}")
    else:
        print(
            "[copy_firmware] Warning: FIRMWARE_VERSION not found in build defines. version.txt not created."
        )

    # Provide feedback in the terminal to confirm what was copied.
    if copied:
        print("\n[copy_firmware] Copied build artifacts:")
        for p in copied:
            print("  -", p)
    else:
        # Or print a message if the files weren't found.
        print("\n[copy_firmware] No firmware.bin or firmware.elf found in", build_dir)


# This is where we hook our function into the PlatformIO build process.
try:
    # 'AddPostAction' registers a function to be run after a specific build target is created.
    # We are targeting the '.elf' file. The '.elf' file is created at the end of the
    # linking stage, so this is a reliable point to know the build was successful.
    # The '.bin' is created from the '.elf', so this ensures both are ready.
    env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.elf"), after_build)
except Exception:
    # Again, if 'env' is not defined (because we're not in a PlatformIO build),
    # this will fail. We catch the exception so the script doesn't crash.
    pass
