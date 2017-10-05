from ore_examples_helper import run_example, get_list_of_examples

output_directory = "Output"


class TestExamples(object):
    
    # List of tests to exclude
    # Example_20 because it is only a placeholder
    exclude_list = ['Example_20']

    def test_examples(self):
        """
        This test factory produces 1 test for each example in get_list_of_examples()
        by calling 'check_example' with example.
        """

        for example in get_list_of_examples(exclude_list):
            yield self.check_example, example

    def check_example(self, example_subdir):
        """
        Runs the example in 'example_subdir', retrieves the exit code of the corresponding run.py and checks
        that it is 0.

        :param example_subdir: A subdirectory of an example to test, for instance 'Example_5'.
        """

        assert run_example(example_subdir) == 0
