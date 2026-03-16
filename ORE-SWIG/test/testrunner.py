import os
import sys
import ORE
import pytest

sys.modules["QuantLib"] = ORE

if __name__=="__main__":
    pytest_args = sys.argv[1:] or ["../QuantLibTestSuite"]
    ret_code = pytest.main(pytest_args)
    print("PyTest Return Code: ", ret_code)