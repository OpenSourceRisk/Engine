#!/usr/bin/python
import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../'))
import json
import logging
import unittest
from Examples.ore_examples_helper import get_list_of_examples
from Examples.ore_examples_helper import run_example
from Tools.PythonTools.compare_files import compare_files
from Tools.PythonTools.setup_logging import setup_logging


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
                self.compFiles(os.path.join('Output', f), os.path.join('ExpectedOutput', f), comp_config)
        else:
            self.logger.warning('No ExpectedOutput folder detected, skipped.')

    def runAndRegressExample(self, name, comp_config):
        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = '50'
        self.logger.info('{}: run {}'.format(self._testMethodName, name))
        ret = run_example(name)
        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = ''
        assert ret == 0

        self.logger.info('{}: run regression on {}'.format(self._testMethodName, name))
        current_dir = os.getcwd()
        try:
            os.chdir(os.path.join(os.getcwd(), name))
            self.compAllFiles(comp_config)
        except Exception:
            self.logger.error('error in ' + name)
            raise
        finally:
            os.chdir(current_dir)


# For each example we want to add a test to PricerRegressionTests
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def add_utest(name, comp_config):
    def do_run_test(self):
        self.runAndRegressExample(name, comp_config)

    return do_run_test


# Need to have this as a function and call it before unitest.main()
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def regress_all_utests():
    logger = logging.getLogger(__name__)

    # Attempt to read in a comparison configuration file
    comp_config_file = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '../Tools/PythonTools/comparison_config.json')

    if os.path.isfile(comp_config_file):
        with open(comp_config_file, 'r') as f:
            comp_config = json.load(f)
    else:
        logger.warning('The comparison config file, %s, does not exist so proceeding without one.', comp_config_file)
        comp_config = None

    i = 1
    for name in get_list_of_examples():
        test_method = add_utest(name, comp_config)
        test_method.__name__ = 'test{}_{}'.format(str(i).zfill(2), name)
        setattr(TestExamples, test_method.__name__, test_method)

        i += 1


# First point in the code that is hit so set up logging here.
setup_logging()

# If all examples run, run regression
regress_all_utests()

if __name__ == '__main__':
    unittest.main()
