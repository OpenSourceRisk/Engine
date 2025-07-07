import utilities

trade = "BermudanSwaptionCash"
rawCubeFile = 'Output/Dim2/rawcube.csv'
expiryDepth = 1
asof = utilities.getAsOfDate("Input/Dim2/ore.xml")

sampleRange = [1,2,3,4,5,6,7,8,9,10]

for s in sampleRange:
    expiryDate = utilities.expiryDate(s, trade, rawCubeFile, expiryDepth)
    if expiryDate != "":
        expiryTime = utilities.getTimeDifference(asof, expiryDate) 
        print("sample", s, "expiry", expiryDate, "{:.2f}".format(expiryTime), trade)
