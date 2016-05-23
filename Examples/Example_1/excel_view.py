import xlwings as xw
from xlwings import Workbook, Sheet, Range, Chart
import time
import datetime
from subprocess import call
import matplotlib.pyplot as plt

# connect to the active workbook
#wb = Workbook('run.xlsm')
wb = Workbook.active()

# log status
Range('B8').value = 'running upload ...'

file = 'Output/exposure_trade_Swap_20y.csv'
# load data into arrays and cells
x = []
y = []
z = []
line = 2    
import csv
with open(file) as csvfile:
    reader = csv.DictReader(csvfile, delimiter=',')
    Range('H1').value = 'Time'
    Range('I1').value = 'EPE'
    Range('J1').value = 'ENE'
    for row in reader:
        x.append(float(row['Time']))
        y.append(float(row['EPE']))
        z.append(float(row['ENE']))
        Range('H' + str(line)).value = float(row['Time'])
        Range('I' + str(line)).value = float(row['EPE'])
        Range('J' + str(line)).value = float(row['ENE'])
        line = line + 1

# add chart
cellrange = str("H1:J") + str(line) 

chart = xw.Chart.add(source_data=xw.Range(cellrange).table)
chart.name = 'chart'
chart.chart_type = xw.ChartType.xlLine
chart.top = 200
chart.left = 0
chart.height = 250
chart.width = 350
chart.title = 'Exposure Evolution'
chart.xlabel = 'Time / Years'
chart.ylabel = 'Exposure'

# log status
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
Range('B8').value = st + " Upload completed"

# add same plot again using matplotlib
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_title("Exposure Evolution")
ax.set_xlabel("Time / Years")
ax.set_ylabel("Exposure")
ax.plot(x,y, label="EPE")
ax.plot(x,z, label="ENE")
legend = ax.legend(loc="upper right")

plot = xw.Plot(fig)

plot.show('plot', left=xw.Range('A33').left, top=xw.Range('A33').top, width=350, height=250)

# log status
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
xw.Range('B8').value = st + " Upload completed"
