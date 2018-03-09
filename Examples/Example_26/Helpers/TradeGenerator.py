import sys
import xml
import xml.etree.ElementTree as ET
import shutil as su
import datetime
import os
import copy
from xml.dom import minidom

marketData = {}
curveID = 'all'
writeType = 'w'
backup = False
startDateStr = "20160205"
swapConventions = {}
swapIndexConventions = {}
FRAConventions = {}
DepositConventions = {}
marketDataInputFile = "..\\..\\Input\\market_" + startDateStr + ".txt"
trades = ET.Element("Portfolio")



if len(sys.argv) > 1:
    curveID = sys.argv[1]
    if len(sys.argv) > 2:
        writeType = sys.argv[2]


#####for testing
tradeFile = "TradeTemplate.xml"
template = ET.parse(tradeFile)
swapTemplate = template.find('.//Trade[@id="swaptemplate"]')
FRATemplate = template.find('.//Trade[@id="fratemplate"]')


inputfile = "..\Input\portfolio_swap.xml"
print("Input File Name: " + inputfile.split("\\")[-1])
if backup:
    dt = datetime.datetime.now()
    mydate = str(dt.year)
    if dt.month < 10:
        mydate = mydate + "0" + str(dt.month)
    else:
        mydate = mydate + str(dt.month)
    if dt.day < 10:
        mydate = mydate + "0" + str(dt.day)
    else:
        mydate = mydate + str(dt.day)
    if dt.hour < 10:
        mytime = "0" + str(dt.hour)
    else:
        mytime = str(dt.hour)
    if dt.minute < 10:
        mytime = mytime + "0" + str(dt.minute)
    else:
        mytime = mytime + str(dt.minute)
    backupfolder = "..\\backup\\" + mydate + "\\" + mytime
    backupfile = backupfolder + "\\" + inputfile.split("\\")[-1]
    if not os.path.exists(backupfolder):
        os.makedirs(backupfolder)
    print ("Copying " + inputfile + " to:")
    print(backupfile)


    su.copyfile(inputfile, backupfile)
print("Hello World!")
convetions_file = "..\..\Input\conventions.xml"
curve_config_file = "..\..\Input\curveconfig.xml"

def currencyDaycountConvention(curr, tenor=''):
    if curr == "EUR":
        return "ACT/360"
    if curr == "GBP":
        return "ACT365"
    return "ACT/360"

def freqToTenor(freq):
    if freq == "Annual":
        return "1Y"
    if freq == "Semiannual":
        return "6M"
    if freq == "Quarterly":
        return "3M"
    if freq == "Bimonthly":
        return "2M"
    if freq == "Monthly":
        return "1M"
    if freq == "Weekly":
        return "1W"
    if freq == "Daily":
        return "1D"
    if freq == "Lunarmonth":
        return "1L"
    if freq == "Once":
        return "1Z"
    print("Could not find frequency: " + freq)
    return ""

def importMarketData(mdFileLoc):
    mdFile = open(mdFileLoc, "r")
    for line in mdFile:
        info = line.split(" ")
        if info[0] == startDateStr:
            marketData[info[1]] = info[2].rstrip()

def addTenorToDate(date, tenorString):
    while date.weekday() > 4 or date.weekday() < 1:
        date += datetime.timedelta(days=1)
    if len(tenorString) > 3:
        # aYbM -> (12*a + b)M
        years = tenorString[0]
        months = tenorString[2]
        tenor = str(12 * int(years) + int(months)) + "M"

    endDay = date.day
    endMonth = date.month
    endYear = date.year

    dmy = tenorString[-1]
    num = int(tenorString[0:-1])
    if dmy == "Y":
        endDate = datetime.date(date.year + num, date.month, date.day)
    elif dmy == "M":
        endMonth = date.month + num
        while endMonth > 12:
            endMonth -= 12
            endYear += 1
        endDate = datetime.date(endYear, endMonth, endDay)

    elif dmy == "W":
        endDate = date + datetime.timedelta(days=num * 7)

    return endDate


def CreateTrade(tradeType, tradeQuote, curve):

    details = tradeQuote.split("/")

    if tradeType == "Swap":
        newTrade = copy.deepcopy(swapTemplate)
        newRoot = newTrade.getroot()
        try:
            convention = swapConventions[curve]
        except:
            print("convention not found: " + curve)
            return
        currency = details[2]
        tenor = details[4]
        maturity = details[5]
        fixedCalendar = convention.find("FixedCalendar").text
        fixedFrequency = freqToTenor(convention.find("FixedFrequency").text)
        fixedConvention = convention.find("FixedConvention").text
        fixedDayCounter = convention.find("FixedDayCounter").text
        floatIndex = convention.find("Index").text
        try:
            fixedRate = marketData[tradeQuote]
        except:
            print("Market data not found for: " + startDateStr + " " + tradeQuote)
            return

        #both legs

        startDate = datetime.datetime.strptime(startDateStr, "%Y%m%d").date()
        fixingDays = 2  # depends on convention
        startDate += datetime.timedelta(days=fixingDays)
        while startDate.weekday() >4 or startDate.weekday() < 1:
            startDate += datetime.timedelta(days=1)
        if len(maturity) > 3:
            #aYbM -> (12*a + b)M
            years = maturity[0]
            months = maturity[2]
            maturity = str(12*int(years)+int(months)) + "M"



        endDay = startDate.day
        endMonth = startDate.month
        endYear = startDate.year

        dmy = maturity[-1]
        num = int(maturity[0:-1])
        if dmy == "Y":
            endDate = datetime.date(startDate.year+num,startDate.month,startDate.day)
        elif dmy == "M":
            endMonth = startDate.month + num
            while endMonth > 12:
                endMonth -= 12
                endYear +=1
            endDate = datetime.date(endYear,endMonth,endDay)

        elif dmy == "W":
            endDate = startDate + datetime.timedelta(days=num*7)
        endDate = endDate.strftime('%Y%m%d')
        startDate = startDate.strftime("%Y%m%d")
        id = currency + "_" + tradeType + "_" + tenor + "_" + maturity
        newRoot.set("id",id)
        newRoot.find("TradeType").text = tradeType
        legs = newRoot.findall(".//LegData")

        for leg in legs:
            if(currency != "EUR"):
                print("non-EUR trade")
            leg.find("ScheduleData/Rules/StartDate").text = startDate
            leg.find("ScheduleData/Rules/EndDate").text = endDate
            legType = leg.find("LegType").text
            if legType == "Fixed":
                print("Creating Fixed Leg")
                #fixed leg
                leg.find("Currency").text = currency
                leg.find("DayCounter").text = fixedDayCounter
                leg.find("PaymentConvention").text = fixedConvention
                leg.find("")
                #fixedlegdata/rates/rate
                #scheduledata
                    #rules

                leg.find("ScheduleData/Rules/Tenor").text = fixedFrequency
                leg.find("ScheduleData/Rules/Calendar").text = fixedCalendar
                leg.find("ScheduleData/Rules/Convention").text = fixedConvention
                leg.find("ScheduleData/Rules/TermConvention").text = fixedConvention
                leg.find("FixedLegData/Rates/Rate").text = fixedRate

                        #convention
                        #termconvention


            elif legType == "Floating":

                print("Creating Floating Leg")
                #floatingleg
                #daycounter
                leg.find("Currency").text = currency
                leg.find("PaymentConvention").text = fixedConvention
                #floatinglegdata
                    #index
                leg.find("FloatingLegData/Index").text = floatIndex
                leg.find("FloatingLegData/FixingDays").text = str(fixingDays)
                #scheduledata
                    #rules
                leg.find("ScheduleData/Rules/Tenor").text = tenor
                leg.find("ScheduleData/Rules/Calendar").text = fixedCalendar
                leg.find("ScheduleData/Rules/Convention").text = fixedConvention
                leg.find("ScheduleData/Rules/TermConvention").text = fixedConvention
                leg.find("DayCounter").text = "ACT/360"
                        #termconvention
            else:
                print("Leg Type Not Fixed or Floating")

        try:


            trades.append(newRoot)
        except Exception as e:
            print("Failed to dump")

    elif tradeType == "Deposit":
        currency = ""
        tenor = ""
        maturity = ""
        trade = ET.Element("Trade")
        trade.tag = r'id="TEST"'
    elif tradeType == "FRA":
        newRoot = copy.deepcopy(FRATemplate)

        try:
            convention = FRAConventions[curve]
        except:
            print("convention not found: " + curve)
            return
        try:
            fixedRate = marketData[tradeQuote]
        except:
            print("Market data not found for: " + startDateStr + " " + tradeQuote)
            return
        currency = details[2]
        tenor = details[4]
        startDate = datetime.datetime.strptime(startDateStr, "%Y%m%d").date()
        forwardWait = details[3]
        startDate =  addTenorToDate(startDate, forwardWait)
        endDate = addTenorToDate(startDate, tenor)
        longshort = "Long"
        index = convention.find("Index").text
        trade = ET.Element("Trade")
        id = "FRA_" + currency + "_" + forwardWait + "_" + tenor

        trade.set("id", id)
        trade.find("ForwardRateAgreementData/StartDate").text = startDate.strftime('%Y-%m-%d')
        trade.find("ForwardRateAgreementData/EndDate").text = endDate.strftime('%Y-%m-%d')
        trade.find("ForwardRateAgreementData/Currency").text
        trade.find("ForwardRateAgreementData/Index").text
        trade.find("ForwardRateAgreementData/LongShort").text
        trade.find("ForwardRateAgreementData/Strike").text = marketData[tradeQuote]
        ##trade.find("ForwardRateAgreementData/Notional").text ##set in template
        trades.append(newRoot)
        #startDate, ednDate, currency, index, longshort, strike,notional
        #
    else:
        currency = ""
        tenor = ""
        maturity = ""
        trade = ET.Element("Trade")
        trade.tag = r'id="TEST"'




importMarketData(marketDataInputFile)


conventions = ET.parse(convetions_file)
conventions = conventions.getroot()


for conv in conventions:
    type = conv.tag
    ID = conv.find("./Id").text
    if type == "Swap":
        swapConventions[ID] = conv
    elif type == "FRA":
        FRAConventions[ID] = conv


configs = ET.parse(curve_config_file)
root = configs.getroot()
yield_curves = root.find('YieldCurves')


for child in yield_curves.getchildren():
    currentCurve = child.find("CurveId").text
    if curveID == currentCurve or curveID == 'all':
        types = child.findall(".//Simple")
        for quoteType in types:
            conventionID = quoteType.find("Conventions").text
            type = quoteType.find("Type").text
            quotes = quoteType.find("Quotes").getchildren()
            for quote in quotes:

                CreateTrade(type,quote.text,quoteType.find(".//Conventions").text)




outputfile = open(inputfile, "w")
outputfile.write(r'<?xml version="1.0" encoding="utf-8"?>')
xmlString = minidom.parseString(ET.tostring(trades)).toprettyxml(indent="  ")
outputfile.write(xmlString)
outputfile.close()

print("Hello World!")




