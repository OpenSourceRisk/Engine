"""
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
"""
import os
import sys
import pytest
import ORE

if __name__ == '__main__':
    print('testing ORED ' + ORE.__version__)
    sys.exit(pytest.main([os.path.dirname(__file__)]))

