from unittest import TestCase
import os
from ore_examples_helper import run_example
import shutil
import inspect

output_directory = "Output"


class ExamplesTest(TestCase):

    def run_example(self, example_subdir):
        # clear output directory
        example_dir = os.path.join(os.getcwd(), example_subdir)
        example_output_dir = os.path.join(example_dir, output_directory)
        try:
            shutil.rmtree(example_output_dir)
        except OSError:
            pass

        # run example
        run_example(example_subdir)

        # check if output directory exists
        self.assertEqual(os.path.exists(example_output_dir), True)

        # check if all output filetypes produced
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

    def test_Example_1(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_2(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_3(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_4(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_5(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_6(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_7(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_8(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_9(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_10(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_11(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_12(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_13(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_14(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_15(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_16(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_17(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])

    def test_Example_18(self):
        self.run_example(inspect.stack()[0][3].split("test_")[1])


