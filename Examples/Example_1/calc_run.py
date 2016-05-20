#import socket  # only needed on win32-OOo3.0.0
import uno
import sys
import time
import datetime
from subprocess import call

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
ts = time.time()
st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
cell.String = st + " ORE completed, return code " + str(return_code)

