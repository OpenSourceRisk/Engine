import os

import shutil
import numpy as np

from ore_examples_helper import run_example, examples

output_directory = "Output"


class TestExamples(object):

    def _clear_output_directory(self, example_subdir):
        example_dir = os.path.join(os.getcwd(), example_subdir)
        example_output_dir = os.path.join(example_dir, output_directory)
        try:
            shutil.rmtree(example_output_dir)
        except OSError:
            pass

    def _check_output(self, example_subdir):
        example_dir = os.path.join(os.getcwd(), example_subdir)

        example_output_dir = os.path.join(example_dir, output_directory)
        assert os.path.exists(example_output_dir)

        produced_output_files = os.listdir(example_output_dir)
        assert "npv.csv" in produced_output_files
        assert "flows.csv" in produced_output_files
        assert "curves.csv" in produced_output_files
        assert "xva.csv" in produced_output_files
        assert "rawcube.csv" in produced_output_files
        assert "netcube.csv" in produced_output_files
        assert len([f for f in produced_output_files if "exposure_trade" in f]) > 0
        assert len([f for f in produced_output_files if "exposure_nettingset" in f]) > 0
        assert len([f for f in produced_output_files if ".dat" in f]) > 0
        assert len([f for f in produced_output_files if ".pdf" in f]) > 0

    def test_examples(self):
        for example in examples[:3]:
            yield self.check_example, example

    def check_example(self, example_subdir):
        #self._clear_output_directory(example_subdir)
        oreex = run_example(example_subdir)
        #self._check_output(example_subdir)
        assert np.all(np.array(oreex.return_codes, dtype=int)) == 0
