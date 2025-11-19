from pathlib import Path
import shutil


def copy_existing_output(output_dir, expected_dir):

    if not output_dir.exists() or not expected_dir.exists():
        print("One of the directories does not exist.")
        return

    for file in output_dir.iterdir():
        if file.is_file():
            target = expected_dir / file.name
            if target.exists():
                shutil.copy2(file, target)
                print(f"Copied {file.name} to ExpectedOutput")
            else:
                print(f"Skipped {file.name} (not in ExpectedOutput)")


def copy_existing_files(test_path):
    output_dir = Path(test_path) / "Output"
    expected_dir = Path(test_path) / "ExpectedOutput"
    copy_existing_output(output_dir, expected_dir)
