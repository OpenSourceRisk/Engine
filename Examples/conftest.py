# conftest.py
import pytest, os, sys
from pathlib import Path
script_dir = Path(__file__).parents[0]
sys.path.append(os.path.join(script_dir, '../Tools/PythonTools'))
from update_expected_output import copy_existing_files


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    report = outcome.get_result()

    # Only proceed during the "call" phase
    if report.when != "call":
        return

    # Read environment variables
    copy_all_output = os.getenv("UPDATE_ALL_EXPECTED_OUTPUT", "").upper() == "TRUE"
    copy_failed_output = os.getenv("UPDATE_FAILED_EXPECTED_OUTPUT", "").upper() == "TRUE"

    # Logic:
    # - UPDATE_ALL_EXPECTED_OUTPUT → always copy
    # - UPDATE_FAILED_EXPECTED_OUTPUT → copy only if test failed
    if copy_all_output or (copy_failed_output and report.failed):
        # Build test path
        test_name = item.name
        test_name = test_name.replace('-', '/', 1)
        test_path = Path(item.config.rootdir) / test_name

        copy_existing_files(test_path)



