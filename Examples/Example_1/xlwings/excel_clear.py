import xlwings as xw
from xlwings import Workbook, Sheet, Range, Chart
# FIXME
# This import is not strictly required here
# but, if omitted, Excel will freeze for several
# seconds in the call wb = Workbook.active()
# before the script continues
import matplotlib.pyplot as plt

# connect to the active workbook
#wb = Workbook('run.xlsm')
wb = Workbook.active()

backup = Range('B2').value
Range('A1:Z100').clear()
Range('B2').value = backup

chart = xw.Chart('chart')
chart.delete()

picture = xw.Picture('plot')
picture.delete()
