from pathlib import Path
import shutil


def copy_existing_output(output_dir, expected_dir):

    if not output_dir.exists() or not expected_dir.exists():
        print("One of the directories does not exist.")
        return

    for file in output_dir.rglob('*'):
        if file.is_file():
            rel = file.relative_to(output_dir)
            target = expected_dir / rel
            if target.exists() and target.is_file():
                shutil.copy2(file, target)
                print(f"Copied {file.name} to ExpectedOutput")
            else:
                print(f"Skipped {file.name} (not in ExpectedOutput)")


def copy_existing_files(test_path):
    output_dir = Path(test_path) / "Output"
    expected_dir = Path(test_path) / "ExpectedOutput"
    copy_existing_output(output_dir, expected_dir)
