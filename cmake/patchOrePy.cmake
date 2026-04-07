# A small script to patch ORE.py if needed.
# For example, line 569 and 571 of QuantLib inflation.i
if(NOT EXISTS "${OREPY_TO_PATCH}")
    message(STATUS "Skipping ORE.py patch; file not found: ${OREPY_TO_PATCH}")
    return()
endif()

file(READ "${OREPY_TO_PATCH}" CONTENT)
string(REPLACE "_QuantLib" "_OREP" UPDATED_CONTENT "${CONTENT}")
file(WRITE "${OREPY_TO_PATCH}" "${UPDATED_CONTENT}")
