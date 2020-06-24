from ore_examples_helper import run_example, get_list_of_examples
import os

output_directory = "Output"


class TestExamples(object):

    def test_examples(self):
        """
        This test factory produces 1 test for each example in get_list_of_examples()
        by calling 'check_example' with example.
        """

        for example in get_list_of_examples():
            yield self.check_example, example

    def check_example(self, example_subdir):
        """
        Runs the example in 'example_subdir', retrieves the exit code of the corresponding run.py and checks
        that it is 0. The number of simulation samples are reduced for this test. Example_10 requires at least
        around 50 samples, otherwise an error is thrown from the DIM calculation.

        :param example_subdir: A subdirectory of an example to test, for instance 'Example_5'.
        """

        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = '50'
        ret = run_example(example_subdir)
        os.environ['OVERWRITE_SCENARIOGENERATOR_SAMPLES'] = ''
        assert ret == 0
