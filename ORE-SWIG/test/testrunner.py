import os
import sys
import ORE
import nose

sys.modules["QuantLib"] = ORE

if __name__=="__main__":
    nose.run()