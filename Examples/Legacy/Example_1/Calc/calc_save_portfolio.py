#import socket  # only needed on win32-OOo3.0.0
import uno
import sys
import time
import datetime
from subprocess import call

###############################################
# Function definitions                        #
###############################################

def copyCsvToSheet(fileName, sheet):
    i = 0
    import csv
    with open(fileName) as csvfile:
        reader = csv.reader(csvfile, delimiter=',')
        for row in reader:
            j = 0
            for col in row:
                cell = sheet.getCellByPosition(j,i)
                cell.String = row[j]
                j = j + 1
            i = i + 1
    return

def copyXmlToSheet(fileName, sheet):
    with open(fileName) as f:
        alist = f.read().splitlines()
    i = 0
    for line in alist:
        cell = sheet.getCellByPosition(0,i)
        cell.String = line
        i = i + 1
    return

def writeFirstColumnToFile(fileName, sheet, lastLineContainsThisString):
    target = open(fileName, 'w') 
    target.truncate()
    i = 0
    while True:
        cell = sheet.getCellByPosition(0,i)
        target.write(cell.String)
        target.write("\n")
        i = i + 1
        if lastLineContainsThisString in cell.String:
            break
    target.close()
    return

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
# Run ORE batch                               #
###############################################

# run first batch
cell = active_sheet.getCellRangeByName("B14")
portfolioFileName = cell.String

#status
cell = active_sheet.getCellRangeByName("B18")
cell.String = "Export in progress ..."

# Create portfolio sheet and import from file
sheets = model.getSheets()
sheet = sheets.getByName('Portfolio')
outputFileName = "Output/portfolio_out.xml"
writeFirstColumnToFile(outputFileName, sheet, "/Portfolio")

# status
#cell = active_sheet.getCellRangeByName("B18")
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
cell.String = st + " export completed"

