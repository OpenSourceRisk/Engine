import xlwings as xw
from xlwings import Workbook, Sheet, Range, Chart
import time
import datetime
from subprocess import call
# FIXME
# This import is not strictly required here
# but, if omitted, Excel will freeze for several
# seconds in the call wb = Workbook.active()
# before the script continues
import matplotlib.pyplot as plt

# connect to the active workbook
#wb = Workbook('run.xlsm')
wb = Workbook.active()

Range('B5').value = 'ORE running ...'

# read input file
fileName = Range('B2').value
cmd = "../../App/ore " + fileName

# log status and run
Range('B5').value = 'running ORE with ' + fileName + ' ...'
return_code = call(cmd, shell=True)

# update status when finished
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
Range('B5').value = st + ' ORE completed, return code ' + str(return_code)
