# A small script to patch ORE.py if needed.
# For example, line 569 and 571 of QuantLib inflation.i
file(READ "${OREPY_TO_PATCH}" CONTENT)
string(REPLACE "_QuantLib" "_OREP" UPDATED_CONTENT "${CONTENT}")
file(WRITE "${OREPY_TO_PATCH}" "${UPDATED_CONTENT}")
