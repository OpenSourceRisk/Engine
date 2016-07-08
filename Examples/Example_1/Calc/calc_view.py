#import socket  # only needed on win32-OOo3.0.0
import uno
import sys
import time
import datetime
from subprocess import call
from com.sun.star.awt import Rectangle
from com.sun.star.table import CellRangeAddress

###############################################
# Function definitions for charts             #
###############################################

# Removes a chart if it exists in the list of charts
def removeChart(rName, charts):
	total = charts.getCount()
	for i in range(total):
		aChart = charts.getByIndex(i)
		if aChart.getName()== rName:
			charts.removeByName(rName)
			return True
	return False

# Given a bunch of parameters, generate a chart in scalc
def generateChart(sheetObj, sheetIndex, ID, rWidth, rHeight, rX, rY, 
        range, colHeader=False, rowHeader=False):
	
	# Selecting data range to process
	cr = sheetObj.getCellRangeByName(range).getRangeAddress()

	dataRange = []
	dataRange.append(cr)
	
	# Size of the graph
	rect = Rectangle(rX,rY,rWidth,rHeight)
	
	# Generating the chart
	charts = sheetObj.getCharts()
	# If a chart with this name exists, delete it
	removeChart('chart', charts)
	#charts.addNewByName(str(ID), rect, tuple(dataRange), colHeader, rowHeader)
	charts.addNewByName('chart', rect, tuple(dataRange), colHeader, rowHeader)
	return charts.getByName('chart')

###############################################
# Establish contact with Calc                 #
###############################################

# get the uno component context from the PyUNO runtime
localContext = uno.getComponentContext()

# create the UnoUrlResolver
resolver = localContext.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", localContext)

# connect to the running office
ctx = resolver.resolve("uno:socket,host=localhost,port=2002;urp;StarOffice.ComponentContext")
smgr = ctx.ServiceManager

# get the central desktop object
desktop = smgr.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)

# access the current writer document
model = desktop.getCurrentComponent()

# access the active sheet
active_sheet = model.CurrentController.ActiveSheet

###############################################
# Load results into the sheet and add chart   #
###############################################

cell = active_sheet.getCellRangeByName("B8")
cell.String = "running upload..."

# copy results into columns
line = 2    
import csv
with open('Output/exposure_trade_Swap_20y.csv') as csvfile:
    reader = csv.DictReader(csvfile, delimiter=',')
    cell = active_sheet.getCellRangeByName("H1")
    cell.String = 'Time'
    cell = active_sheet.getCellRangeByName("I1")
    cell.String = 'EPE'
    cell = active_sheet.getCellRangeByName("J1")
    cell.String = 'ENE'
    for row in reader:
        #print (row['Time'], row['EPE'], row['ENE'])
        cell = active_sheet.getCellRangeByName("H" + str(line))
        cell.Value = float(row['Time'])
        cell = active_sheet.getCellRangeByName("I" + str(line))
        cell.Value = float(row['EPE'])
        cell = active_sheet.getCellRangeByName("J" + str(line))
        cell.Value = float(row['ENE'])
        line = line + 1

# add a chart

cellrange = str("H1:J") + str(line) 
dx = 14000
dy = 10000
x = 0
y = 14000
oChart = generateChart(active_sheet, 1, 'Exposure', dx, dy, x, y, cellrange, True, True)
oChartDoc = oChart.getEmbeddedObject()  
oDiagram = oChartDoc.createInstance( "com.sun.star.chart.XYDiagram" ) 
#oDiagram = oChartDoc.createInstance( "com.sun.star.chart.LineDiagram" ) 
oDiagram.Vertical = False
oDiagram.SymbolType =-3
oDiagram.HasXAxisTitle = True
oDiagram.XAxisTitle.String = "Time/Years"
oDiagram.HasYAxisTitle = True
oDiagram.YAxisTitle.String = "Exposure"
oDiagram.XAxisTitle.CharHeight = 10
oDiagram.YAxisTitle.CharHeight = 10
oDiagram.YAxis.CharHeight = 10
oDiagram.XAxis.CharHeight = 10
oChartDoc.HasMainTitle = True
oChartDoc.HasLegend = True
oChartDoc.Title.String = "Exposure Evolution"
oChartDoc.Title.CharHeight = 10
oChartDoc.setDiagram( oDiagram ) 
oDiagram = oChartDoc.getDiagram()
 
# write final status message

cell = active_sheet.getCellRangeByName("B8")
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
cell.String = st + " Upload completed"

