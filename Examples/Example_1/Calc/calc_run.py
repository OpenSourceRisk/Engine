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
cell = active_sheet.getCellRangeByName("B2")
fileName = cell.String
cell = active_sheet.getCellRangeByName("B5")
cmd = "../../App/ore " + fileName
cell.String = "running ORE with " + fileName + " ..."
return_code = call(cmd, shell=True)

# Import NPV report
sheets = model.getSheets()
sheets.insertNewByName('NPV', 1)
sheet = sheets.getByName('NPV')
copyCsvToSheet('Output/npv.csv', sheet)

# Import CASHFLOW report
sheets = model.getSheets()
sheets.insertNewByName('Cashflow', 2)
sheet = sheets.getByName('Cashflow')
copyCsvToSheet('Output/flows.csv', sheet)
        
# Import XVA report
sheets = model.getSheets()
sheets.insertNewByName('XVA', 3)
sheet = sheets.getByName('XVA')
copyCsvToSheet('Output/xva.csv', sheet)

# Import ore.xml
sheets = model.getSheets()
sheets.insertNewByName('ORE', 4)
sheet = sheets.getByName('ORE')
copyXmlToSheet('Input/ore.xml', sheet)

ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
cell.String = st + " ORE completed, return code " + str(return_code)

