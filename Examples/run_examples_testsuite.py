#!/usr/bin/python
import os
import sys
import logging
import unittest
from pathlib import Path
import pytest

examples_exempt_from_scenariogenerator_samples_overwrite = [
    'Performance',
    'Legacy/Example_37',
    'Legacy/Example_54',
    'Legacy/Example_55',
    'Legacy/Example_56',
    'Legacy/Example_60',
    'Legacy/Example_68',
    'Legacy/Example_70',
    'Legacy/Example_72']

# Pull in some shared utilities
script_dir = Path(__file__).parents[0]
sys.path.append(os.path.join(script_dir, '../'))
from Examples.ore_examples_helper import get_list_of_legacy_examples  # noqa
from Examples.ore_examples_helper import get_list_of_examples  # noqa
from Examples.ore_examples_helper import get_list_ore_academy  # noqa
from Examples.ore_examples_helper import run_example  # noqa
from Tools.PythonTools.compare_files import compare_files  # noqa
from Tools.PythonTools.setup_logging import setup_logging  # noqa
from Tools.PythonTools.merge_comparison_configs import merge_configurations  # noqa

logger = logging.getLogger(__name__)

# Allow an environment variable to be provided as a flag.
def get_env_bool(var_name, default=False):
    val = os.getenv(var_name)
    if val is None:
        return default
    return val.lower() in ("true", "1", "yes", "on", "t")

# Get all files in a directory
def get_files(dirname):
    res = []
    for cur_path, subdirs, files in os.walk(dirname):
        for file in files:
            res.append(os.path.relpath(os.path.join(str(cur_path), file), dirname))
    return res


# Unit test class
class TestExamples(unittest.TestCase):
    def setUp(self):
        self.logger = logger

    def compFiles(self, file1, file2, comp_config):
        self.logger.info('{}: Checking {} and {}'.format(self._testMethodName, file1, file2))
        self.assertTrue(os.path.isfile(file1), file1 + ' is not a file')
        self.assertTrue(os.path.isfile(file2), file2 + ' is not a file')

        # check that it equals the expected output
        self.assertTrue(compare_files(file1, file2, self._testMethodName, comp_config),
                        'Error comparing {} to {}'.format(file1, file2))

    def compAllFiles(self, comp_config):
        curr_dir = os.getcwd()
        dir_leaf = os.path.basename(os.path.normpath(curr_dir))
        if os.path.isdir(os.path.join(curr_dir, 'ExpectedOutput')):
            for f in get_files('ExpectedOutput'):
                with self.subTest(test_name=dir_leaf, filename=f):
                    exp_file = os.path.join('ExpectedOutput', f)
                    out_file = os.path.join('Output', f)
                    self.compFiles(exp_file, out_file, comp_config)
        else:
            self.logger.warning('No ExpectedOutput folder detected, skipped.')

    def runAndRegressExample(self, name):
        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = '50'
        for exname in examples_exempt_from_scenariogenerator_samples_overwrite:
            if name.endswith(exname):
                os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = ''
        self.logger.info('{}: run {}'.format(self._testMethodName, name))
        if not get_env_bool('ORE_EXAMPLES_COMPARE_ONLY'):
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


# For each example we want to add a test to the test suite
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def add_utest(name):
    def do_run_test(self):
        self.runAndRegressExample(name)

    return do_run_test


# Need to have this as a function and call it before unitest.main()
# https://stackoverflow.com/questions/2798956/python-unittest-generate-multiple-tests-programmatically
def regress_all_utests():
    all_examples = None

    # If a file containing a list of examples to run is provided via environment variable, use it.
    test_list_file = os.getenv('ORE_EXAMPLES_TEST_FILE')
    if test_list_file:
        test_list_file_path = Path(test_list_file)
        if test_list_file_path.exists():
            all_examples = sorted(test_list_file_path.read_text().splitlines())
            logger.info('Examples from ORE_EXAMPLES_TEST_FILE: {}'.format(all_examples))
        else:
            logger.warning('ORE_EXAMPLES_TEST_FILE provided, {}, but the path does not exist.'.format(test_list_file))
            logger.warning('Will continue and run all examples.')

    if all_examples is None:
        examples = get_list_of_examples()
        legacy_examples = get_list_of_legacy_examples()
        academy = get_list_ore_academy()
        all_examples = sorted(examples + academy + legacy_examples)
        logger.info('Legacy: {}'.format(legacy_examples))
        logger.info('Examples: {}'.format(examples))
        logger.info('Academy: {}'.format(academy))

    for name in all_examples:
        # For Linux/docker: Replace the '/' in testable_name and class_name 
        name2 = name.replace('/', '-', 1)
        logger.info('Add test: {}, {}'.format(name, name2))
        testable = add_utest(name)
        testable_name = 'test_{0}'.format(name2)
        testable.__name__ = testable_name
        class_name = 'test_{0}'.format(name2)
        globals()[class_name] = type(class_name, (TestExamples,), {testable_name: testable})


# First point in the code that is hit so set up logging here.
setup_logging()

# If all examples run, run regression
regress_all_utests()

if __name__ == '__main__':
    print("run pytest")
    ret_code = pytest.main([sys.argv[0], "-n", "16", "-o", "log_cli=true"])
    print("PyTest Return Code: ", ret_code)
