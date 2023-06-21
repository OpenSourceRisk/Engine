#!/usr/bin/python
import os
import sys
import logging
import unittest
from pathlib import Path
import nose
import collections
#collections.Callable = collections.abc.Callable

# Pull in some shared utilities
script_dir = Path(__file__).parents[0]
sys.path.append(os.path.join(script_dir, '../'))
from Examples.ore_examples_helper import get_list_of_examples  # noqa
from Examples.ore_examples_helper import get_list_ore_academy  # noqa
from Examples.ore_examples_helper import run_example  # noqa
from Tools.PythonTools.compare_files import compare_files  # noqa
from Tools.PythonTools.setup_logging import setup_logging  # noqa
from Tools.PythonTools.merge_comparison_configs import merge_configurations  # noqa


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
        self.logger.info('{}: Checking {} and {}'.format(self._testMethodName, file1, file2))
        self.assertTrue(os.path.isfile(file1), file1 + ' is not a file')
        self.assertTrue(os.path.isfile(file2), file2 + ' is not a file')

        # check that it equals the expected output
        self.assertTrue(compare_files(file1, file2, self._testMethodName, comp_config),
                        'Error comparing {} to {}'.format(file1, file2))

    def compAllFiles(self, comp_config):
        if os.path.isdir(os.path.join(os.getcwd(), 'ExpectedOutput')):
            for f in get_files('ExpectedOutput'):
                self.compFiles(os.path.join('ExpectedOutput', f), os.path.join('Output', f), comp_config)
        else:
            self.logger.warning('No ExpectedOutput folder detected, skipped.')

    def runAndRegressExample(self, name):
        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = '50'
        self.logger.info('{}: run {}'.format(self._testMethodName, name))
        ret = run_example(name)
        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = ''
        assert ret == 0

        self.logger.info('{}: run regression on {}'.format(self._testMethodName, name))
        current_dir = os.getcwd()

        # Default comparison config file path
        default_comp_config_path = os.path.join(script_dir, '../Tools/PythonTools/comparison_config.json')
        if not os.path.isfile(default_comp_config_path):
            raise ValueError('Expected path ' + default_comp_config_path + ' to exist.')

        # Check for a comparison config file, in the test directory, and use it if it exists.
        test_dir = os.path.join(current_dir, name)
        comp_config_path = os.path.join(test_dir, 'comparison_config.json')
        if not os.path.isfile(comp_config_path):
            comp_config_path = None

        # Merge the static config with the test's config if there is one.
        comp_config = merge_configurations(default_comp_config_path, comp_config_path)

        try:
            os.chdir(test_dir)
            self.compAllFiles(comp_config)
        except Exception:
            self.logger.error('error in ' + name)
            raise
        finally:
            os.chdir(current_dir)


# For each example we want to add a test to PricerRegressionTests
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def add_utest(name):
    def do_run_test(self):
        self.runAndRegressExample(name)

    return do_run_test


# Need to have this as a function and call it before unitest.main()
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def regress_all_utests():
    i = 1
    for name in (get_list_of_examples() + get_list_ore_academy()):
        testable_name = 'test_{0}'.format(name)
        testable = add_utest(name)
        testable.__name__ = testable_name
        class_name = 'Test_{0}'.format(name)
        globals()[class_name] = type(class_name, (TestExamples,), {testable_name: testable})


# First point in the code that is hit so set up logging here.
setup_logging()

# If all examples run, run regression
regress_all_utests()

if __name__ == '__main__':
    nose.runmodule(name='__main__')
