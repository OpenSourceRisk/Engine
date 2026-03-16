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
cell = active_sheet.getCellRangeByName("B16")
cell.String = "Upload in progress ..."

# Create portfolio sheet and import from file
sheets = model.getSheets()
#sheets.removeByName('Portfolio')
sheets.insertNewByName('Portfolio', 1)
sheet = sheets.getByName('Portfolio')
copyXmlToSheet(portfolioFileName, sheet)

# status
cell = active_sheet.getCellRangeByName("B16")
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
cell.String = st + " uploaded completed, see sheet Portfolio"

