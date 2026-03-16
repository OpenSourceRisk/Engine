"""
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
"""
import sys
import nose
import ORE

def test():
    print('testing ORED ' + ORE.__version__)
    result = nose.run()
    if result == False:
        sys.exit(1)

if __name__ == '__main__':
    test()

