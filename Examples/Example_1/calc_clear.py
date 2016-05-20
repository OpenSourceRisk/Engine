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
cell = active_sheet.getCellRangeByName("B2")
fileName = cell.String

# remove charts
charts = active_sheet.getCharts()
charts.removeByName('chart')

# clear all cells content
cr = active_sheet.getCellRangeByName("A1:Z100")
cr.clearContents(1) # simple values
cr.clearContents(4) # strings
# clear all objects including charts, but also macro buttons
#cr.clearContents(128) 

#restore
cell = active_sheet.getCellRangeByName("B2")
cell.String = fileName

