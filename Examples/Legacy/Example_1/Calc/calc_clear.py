#import socket  # only needed on win32-OOo3.0.0
import uno
import sys

###############################################
# Generic part establishing contact with Calc #
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
# Specific part                               #
###############################################

# backup
cellB2 = active_sheet.getCellRangeByName("B2")
backupB2 = cellB2.String

cellB14 = active_sheet.getCellRangeByName("B14")
backupB14 = cellB14.String

cellB22 = active_sheet.getCellRangeByName("B22")
backupB22 = cellB22.String

# remove charts
charts = active_sheet.getCharts()
charts.removeByName('chart')

# clear all cells content
cr = active_sheet.getCellRangeByName("B1:Z100")
cr.clearContents(1) # simple values
cr.clearContents(4) # strings
# clear all objects including charts, but also macro buttons
#cr.clearContents(128) 

#restore
cellB2.String = backupB2
cellB14.String = backupB14
cellB22.String = backupB22

sheets = model.getSheets()
try:
    sheets.removeByName('NPV')
except :
    pass

try:
    sheets.removeByName('Cashflow')
except :
    pass

try:
    sheets.removeByName('XVA')
except :
    pass

try:
    sheets.removeByName('ORE')
except :
    pass

try:
    sheets.removeByName('Portfolio')
except :
    pass

try:
    sheets.removeByName('Market')
except :
    pass
