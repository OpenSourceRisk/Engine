from unittest import TestCase
import os

import inspect
import shutil
import numpy as np


from ore_examples_helper import run_example

output_directory = "Output"


class ExamplesTest(TestCase):

    @classmethod
    def setUpClass(cls):
        pass

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
        self._clear_output_directory(example_subdir)
        oreex = run_example(example_subdir)
        self._check_output(example_subdir)
        self.assertEqual(np.all(np.array(oreex.return_codes, dtype=int)), 0)




    def test_Example_1(self):
        self.check_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_2(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_3(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_4(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_5(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_6(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_7(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_8(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_9(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_10(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_11(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_12(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_13(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_14(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_15(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_16(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_17(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_18(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_19(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_20(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_21(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_22(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])
    #
    # def test_Example_23(self):
    #     self.run_example(inspect.stack()[0][3].split("test_")[1])

