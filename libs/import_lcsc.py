import argparse
import os
import shutil
import subprocess
import sys

# Folder this script lives in (…/drone_PCB/libs). All defaults are resolved
# relative to this, so the script works no matter what directory you run it from.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def run_easyeda2kicad_from_file(input_file, output_dir=None, python_exec=None):
    if output_dir is None:
        output_dir = os.path.join(SCRIPT_DIR, "drone_lib")
    input_file = os.path.expanduser(input_file)
    output_dir = os.path.expanduser(output_dir)
    python_exec = python_exec or sys.executable

    if not os.path.isfile(input_file):
        print(f"Error: File not found: {input_file}")
        return 2

    # If a simple name was provided, check PATH; if an absolute path, check that file exists.
    found = shutil.which(python_exec) if os.path.basename(python_exec) == python_exec else os.path.exists(python_exec)
    if not found:
        print(f"Warning: Python executable '{python_exec}' not found in PATH or as given path. Trying anyway.")

    # Verify easyeda2kicad is importable in the chosen interpreter before running any commands.
    check = subprocess.run(
        [python_exec, "-c", "import easyeda2kicad"],
        capture_output=True,
    )
    if check.returncode != 0:
        print(f"❌ 'easyeda2kicad' is not installed in '{python_exec}'.")
        print(f"   Fix: {python_exec} -m pip install easyeda2kicad")
        return 4

    # Ensure the PARENT folder exists, but do NOT create `output_dir` itself as a
    # directory. easyeda2kicad treats the last path segment as the library BASE NAME,
    # so output_dir=".../libs/drone_lib" appends to drone_lib.kicad_sym and writes into
    # drone_lib.pretty/ and drone_lib.3dshapes/. If we made "drone_lib" a real folder,
    # it would instead dump everything as easyeda2kicad.* inside it.
    parent = os.path.dirname(output_dir)
    if parent:
        os.makedirs(parent, exist_ok=True)

    with open(input_file, "r", encoding="utf-8") as f:
        # ignore blank lines and comments
        lines = [line.strip() for line in f if line.strip() and not line.lstrip().startswith("#")]

    if not lines:
        print("No LCSC IDs found in input file.")
        return 0

    for idx, lcsc_id in enumerate(lines, start=1):
        cmd = [
            python_exec,
            "-m", "easyeda2kicad",
            "--full",
            f"--lcsc_id={lcsc_id}",
            f"--output={output_dir}",
        ]
        print(f"[{idx}/{len(lines)}] Running: {' '.join(cmd)}")
        try:
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            if result.stdout:
                print(result.stdout, end="")
        except subprocess.CalledProcessError as e:
            print(f"❌ Error processing {lcsc_id} (exit {e.returncode}):")
            if e.stdout:
                print(e.stdout, end="")
            if e.stderr:
                print(e.stderr, end="")
        except FileNotFoundError as e:
            print(f"❌ Executable not found: {e}")
            return 3

    print("✅ All commands completed.")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run easyeda2kicad for a list of LCSC IDs.")
    parser.add_argument("input_file", nargs="?", default=os.path.join(SCRIPT_DIR, "lcsc.txt"),
                        help="File with one LCSC ID per line (default: lcsc.txt next to this script)")
    parser.add_argument("output_dir", nargs="?", default=os.path.join(SCRIPT_DIR, "drone_lib"),
                        help="Library base path; easyeda2kicad writes <base>.kicad_sym/.pretty/.3dshapes "
                             "(default: libs/drone_lib)")
    parser.add_argument("--python", dest="python_exec", default=None,
                        help="Python executable to use (default: the interpreter running this script)")
    args = parser.parse_args(argv)

    return_code = run_easyeda2kicad_from_file(args.input_file, args.output_dir, args.python_exec)
    sys.exit(return_code if isinstance(return_code, int) else 0)


if __name__ == "__main__":
    main()
