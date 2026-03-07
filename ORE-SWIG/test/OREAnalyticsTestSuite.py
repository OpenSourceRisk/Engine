"""
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
"""
import os
import sys
import pytest
import ORE

def test():
    print('testing ORED ' + ORE.__version__)
    exit_code = pytest.main([os.path.dirname(os.path.abspath(__file__))])
    if exit_code != 0:
        sys.exit(exit_code)


if __name__ == '__main__':
    test()
