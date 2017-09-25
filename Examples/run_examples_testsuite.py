import numpy as np

from ore_examples_helper import run_example, examples

output_directory = "Output"


class TestExamples(object):

    def test_examples(self):
        for example in examples[:1]:
            yield self.check_example, example

    def check_example(self, example_subdir):
        oreex = run_example(example_subdir)
        assert np.all(np.array(oreex.return_codes, dtype=int) == 0)
