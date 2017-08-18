from unittest import TestCase
import os

import inspect
import shutil
import numpy as np


from ore_examples_helper import run_example, examples
import functools

output_directory = "Output"


class ExamplesTest(TestCase):

    def __init__(self, *args, **kwargs):
        def boilerplate_test(example_directory):
            self.check_example(example_directory)
        super(ExamplesTest, self).__init__(*args, **kwargs)
        for example in examples[:2]:
            test_method = functools.partial(boilerplate_test, example_directory=example)
            test_method.__name__ = "test_" + example
            setattr(ExamplesTest, test_method.__name__, test_method)

    def _clear_output_directory(self, example_subdir):
        example_dir = os.path.join(os.getcwd(), example_subdir)
        example_output_dir = os.path.join(example_dir, output_directory)
        try:
            shutil.rmtree(example_output_dir)
            pass
        except OSError:
            pass

    def _check_output(self, example_subdir):
        example_dir = os.path.join(os.getcwd(), example_subdir)

        example_output_dir = os.path.join(example_dir, output_directory)
        self.assertEqual(os.path.exists(example_output_dir), True)

        produced_output_files = os.listdir(example_output_dir)
        self.assertTrue("npv.csv" in produced_output_files)
        self.assertTrue("flows.csv" in produced_output_files)
        self.assertTrue("curves.csv" in produced_output_files)
        self.assertTrue("xva.csv" in produced_output_files)
        self.assertTrue("rawcube.csv" in produced_output_files)
        self.assertTrue("netcube.csv" in produced_output_files)
        self.assertTrue(len([f for f in produced_output_files if "exposure_trade" in f]) > 0)
        self.assertTrue(len([f for f in produced_output_files if "exposure_nettingset" in f]) > 0)
        self.assertTrue(len([f for f in produced_output_files if ".dat" in f]) > 0)
        self.assertTrue(len([f for f in produced_output_files if ".pdf" in f]) > 0)

    def check_example(self, example_subdir):
        #self._clear_output_directory(example_subdir)
        oreex = run_example(example_subdir)
        #self._check_output(example_subdir)
        self.assertEqual(np.all(np.array(oreex.return_codes, dtype=int)), 0)



