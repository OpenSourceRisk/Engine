import os
import sys
import ORE
import pytest

sys.modules["QuantLib"] = ORE

QUANTLIB_TESTS = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              os.pardir, "QuantLib-SWIG", "Python", "test")

if __name__=="__main__":
    pytest_args = sys.argv[1:] or [QUANTLIB_TESTS]
    ret_code = pytest.main(pytest_args)
    print("PyTest Return Code: ", ret_code)
    sys.exit(ret_code)
