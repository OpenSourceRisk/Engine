#!/usr/bin/python
import collections
import logging
import os
import sys
import unittest
from pathlib import Path

import nose2

# collections.Callable = collections.abc.Callable

examples_exempt_from_scenariogenerator_samples_overwrite = [
    "Performance",
    "Legacy/Example_37",
    "Legacy/Example_54",
    "Legacy/Example_55",
    "Legacy/Example_56",
    "Legacy/Example_60",
    "Legacy/Example_68",
    "Legacy/Example_70",
    "Legacy/Example_72",
]

# Pull in some shared utilities
script_dir = Path(__file__).parents[0]
sys.path.append(os.path.join(script_dir, "../"))
from Tools.PythonTools.compare_files import compare_files  # noqa
from Tools.PythonTools.merge_comparison_configs import merge_configurations  # noqa
from Tools.PythonTools.setup_logging import setup_logging  # noqa

from Examples.ore_examples_helper import get_list_of_examples  # noqa
from Examples.ore_examples_helper import get_list_of_legacy_examples  # noqa
from Examples.ore_examples_helper import get_list_ore_academy  # noqa
from Examples.ore_examples_helper import run_example  # noqa


# Get all files in a directory
def get_files(dirname):
    res = []
    for cur_path, subdirs, files in os.walk(dirname):
        for file in files:
            res.append(os.path.relpath(os.path.join(cur_path, file), dirname))
    return res


# Unit test class
class TestExamples(unittest.TestCase):
    def setUp(self):
        self.logger = logging.getLogger(__name__)

    def compFiles(self, file1, file2, comp_config):
        self.logger.info(
            "{}: Checking {} and {}".format(self._testMethodName, file1, file2)
        )
        self.assertTrue(os.path.isfile(file1), file1 + " is not a file")
        self.assertTrue(os.path.isfile(file2), file2 + " is not a file")

        # check that it equals the expected output
        self.assertTrue(
            compare_files(file1, file2, self._testMethodName, comp_config),
            "Error comparing {} to {}".format(file1, file2),
        )

    def compAllFiles(self, comp_config):
        if os.path.isdir(os.path.join(os.getcwd(), "ExpectedOutput")):
            for f in get_files("ExpectedOutput"):
                self.compFiles(
                    os.path.join("ExpectedOutput", f),
                    os.path.join("Output", f),
                    comp_config,
                )
        else:
            self.logger.warning("No ExpectedOutput folder detected, skipped.")

    def runAndRegressExample(self, name):
        os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"] = "50"
        for exname in examples_exempt_from_scenariogenerator_samples_overwrite:
            if name.endswith(exname):
                os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"] = ""
        self.logger.info("{}: run {}".format(self._testMethodName, name))
        ret = run_example(name)
        os.environ["OVERWRITE_SCENARIOGENERATOR_SAMPLES"] = ""
        assert ret == 0

        self.logger.info("{}: run regression on {}".format(self._testMethodName, name))
        current_dir = os.getcwd()

        # Default comparison config file path
        default_comp_config_path = os.path.join(
            script_dir, "../Tools/PythonTools/comparison_config.json"
        )
        if not os.path.isfile(default_comp_config_path):
            raise ValueError("Expected path " + default_comp_config_path + " to exist.")

        # Check for a comparison config file, in the test directory, and use it if it exists.
        test_dir = os.path.join(current_dir, name)
        comp_config_path = os.path.join(test_dir, "comparison_config.json")
        if not os.path.isfile(comp_config_path):
            comp_config_path = None

        # Merge the static config with the test's config if there is one.
        comp_config = merge_configurations(default_comp_config_path, comp_config_path)

        try:
            os.chdir(test_dir)
            self.compAllFiles(comp_config)
        except Exception:
            self.logger.error("error in " + name)
            raise
        finally:
            os.chdir(current_dir)


# For each example we want to add a test to the test suite
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def add_utest(name):
    def do_run_test(self):
        self.runAndRegressExample(name)

    return do_run_test


# Need to have this as a function and call it before unitest.main()
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def regress_all_utests():
    i = 1
    examples = get_list_of_examples()
    legacyexamples = get_list_of_legacy_examples()
    academy = get_list_ore_academy()
    allexamples = sorted(examples + academy + legacyexamples)
    print("Legacy:", legacyexamples)
    print("Examples:", examples)
    print("Academy:", academy)
    for name in allexamples:
        # For Linux/docker: Replace the '/' in testable_name and class_name
        name2 = name.replace("/", "-", 1)
        print("add test:", name, name2)
        testable = add_utest(name)
        testable_name = "test_{0}".format(name2)
        testable.__name__ = testable_name
        class_name = "Test_{0}".format(name2)
        globals()[class_name] = type(
            class_name, (TestExamples,), {testable_name: testable}
        )


# First point in the code that is hit so set up logging here.
setup_logging()

# If all examples run, run regression
regress_all_utests()

if __name__ == "__main__":
    print("run nose tests")
    # nose2.runmodule(name="__main__")
    nose2.main()
