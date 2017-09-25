import numpy as np

from ore_examples_helper import run_example, examples

output_directory = "Output"


class TestExamples(object):

    def test_examples(self):
        """
        This test factory produces 1 test for each example in 'examples'
        by calling 'check_example' with example.
        """

        for example in examples[:1]:
            yield self.check_example, example

    def check_example(self, example_subdir):
        """
        Runs the example in 'example_subdir', retrieves the exit codes of each call of ore.exe in there and checks
        that all these exit codes are identical to 0.

        :param example_subdir: A subdirectory of an example to test, for instance 'Example_5'.
        """

        oreex = run_example(example_subdir)
        assert np.all(np.array(oreex.return_codes, dtype=int) == 0)
