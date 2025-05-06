
# Copyright (C) 2019 Quaternion Risk Manaement Ltd
# All rights reserved.

from ORE import *

fileLogger = FileLogger("log.txt")
Log.instance().registerLogger(fileLogger)

mask = 7
Log.instance().setMask(mask)
assert mask == Log.instance().mask()

Log.instance().switchOn()

ALOG("Alert Message")
CLOG("Critical Message")
ELOG("Error Message")
WLOG("Warning Message")
LOG("Notice Message")
DLOG("Debug Message")
TLOG("Data Message")

bufferLogger = BufferLogger(ORE_NOTICE)
Log.instance().registerLogger(bufferLogger)
mask = 255
Log.instance().setMask(mask)
msg_d = "This is a debug message."
msg_w = "This is a warning message."
msg_e = "This is an error message."
DLOG(msg_d)
WLOG(msg_w)
ELOG(msg_e)
#msg_d_buf = bufferLogger.next() filtered
msg_w_buf = bufferLogger.next()
msg_e_buf = bufferLogger.next()

print("Printing log message ...")
print(msg_w_buf)
print(msg_e_buf)
print("End of printing log message.")

# FIFO
assert msg_w in msg_w_buf
assert msg_e in msg_e_buf
