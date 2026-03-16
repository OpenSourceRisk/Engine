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
FXFWDConventions = {}
oisConventions = {}
xccyBasisConventions = {}
basisConventions = {}
marketDataInputFile = "../../Input/market_" + startDateStr + ".txt"
oisTrades = ET.Element("Portfolio")
EURxoisTrades = ET.Element("Portfolio")
USDxoisTrades = ET.Element("Portfolio")
debug = False


if len(sys.argv) > 1:
    curveID = sys.argv[1]
    if len(sys.argv) > 2:
        writeType = sys.argv[2]


#####for testing
tradeFile = "Helpers/TradeTemplate.xml"
template = ET.parse(tradeFile)
swapTemplate = template.find('.//Trade[@id="swaptemplate"]')
FRATemplate = template.find('.//Trade[@id="fratemplate"]')
FXFWDTemplate = template.find('.//Trade[@id="fxfwdtemplate"]')
xccyBasisTemplate = template.find('.//Trade[@id="xccybasisswaptemplate"]')
basisTemplate = template.find('.//Trade[@id="xccybasisswaptemplate"]')


oisFile = "Input/ois_portfolio.xml"
EURxoisFile = r"Input/EUR_xois_portfolio.xml"
USDxoisFile = r"Input/USD_xois_portfolio.xml"

conventions_file = "../../Input/conventions.xml"
curve_config_file = "../../Input/curveconfig.xml"

def debugPrint(line):
	if debug:
		print(line)

def indent(elem, level=0):
  i = "\n" + level*"  "
  if len(elem):
    if not elem.text or not elem.text.strip():
      elem.text = i + "  "
    if not elem.tail or not elem.tail.strip():
      elem.tail = i
    for elem in elem:
      indent(elem, level+1)
    if not elem.tail or not elem.tail.strip():
      elem.tail = i
  else:
    if level and (not elem.tail or not elem.tail.strip()):
      elem.tail = i

def currencyDaycountConvention(curr, tenor=''):
    if curr == "EUR":
        return "ACT/360"
    if curr == "GBP":
        return "A365"
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
    debugPrint("Could not find frequency: " + freq)
    return ""

def importMarketData(mdFileLoc):
    mdFile = open(mdFileLoc, "r")
    for line in mdFile:
        info = line.split(" ")
        if info[0] == startDateStr:
            marketData[info[1]] = info[2].rstrip()

def addTenorToDate(date, inputTenorString):
    while date.weekday() > 4: # or date.weekday() < 1:
        date += datetime.timedelta(days=1)
    if len(inputTenorString) > 3:
        # aYbM -> (12*a + b)M
        # years = tenorString[0]
        # months = tenorString[2]
        # tenorString = str(12 * int(years) + int(months)) + "M"
        tenors = inputTenorString[0:2]
        tenors = [tenors]
        tenors.append(inputTenorString[2:4])

    else:
        tenors = [inputTenorString]


    for tenorString in tenors:
        endDay = date.day
        endMonth = date.month
        endYear = date.year
        dmy = tenorString[-1]
        num = int(tenorString[0:-1])
        if dmy == "Y":
            date = datetime.date(endYear + num, endMonth, endDay)
        elif dmy == "M":
            endMonth = date.month + num
            while endMonth > 12:
                endMonth -= 12
                endYear += 1
            date = datetime.date(endYear, endMonth, endDay)

        elif dmy == "W":
            date = date + datetime.timedelta(days=num * 7)
        elif dmy == "D":
            date = date + datetime.timedelta(days=num)
            if date.weekday() > 4:
                date = date + datetime.timedelta(days=num)


    return date

def returnConventionInfo(convention):
    # take in a convention of any type, return the necessary information
    return False

def CreateFloatingLeg(legroot, tradeType, tradeQuote, curve, details, basis=False, payer=False):

    #some default values
    notional = 1000000
    notionalExchange = False
    dayCounter = "ACT/360"
    payConvention = "MF"
    index = ""
    fixingDays = "2"
    startDate = datetime.datetime.strptime(startDateStr, "%Y%m%d").date()
    endDate = ""
    tenor = ""
    calendar = ""
    termConvention = ""
    spread = "0"
    rule = ""
    spotLag = ""
    paymentLag = "0"
    try:

        if(basis):
            if tradeType == "Cross Currency Basis Swap":
                flatCCY = details[2]
                flatTenor = details[3]
                spreadCCY = details[4]
                spreadTenor = details[5]
                maturity = details[6]
                if flatCCY not in ["EUR", "USD"]:
                    return False

                convention = xccyBasisConventions[curve]
                spotLag = convention.find("SettlementDays").text
                payerIndex = convention.find("FlatIndex").text
                spreadIndex = convention.find("SpreadIndex").text
                payConvention = convention.find("RollConvention").text
                calendar = convention.find("SettlementCalendar").text
                isResettable = False
                flatIndexIsResettable = False
                isResettableNode = convention.find("IsResettable")
                if isResettableNode is not None:
                    isResettable = True if (isResettableNode.text=="Y") else False
                    flatIndexIsResettableNode = convention.find("FlatIndexIsResettable")
                    flatIndexIsResettable = True if (flatIndexIsResettableNode.text=="Y") else False
                if (payer):
                    index = convention.find("FlatIndex").text
                    currency = flatCCY
                    tenor = flatTenor

                else:
                    index = convention.find("SpreadIndex").text
                    currency = spreadCCY
                    tenor = spreadTenor
                    try:
                        spread = marketData[tradeQuote]
                    except:
                        debugPrint("Could not find market data for " + tradeQuote)
                        return False
                fixingDays = getFixingDaysForCCY(currency)
                if(isResettable):
                    if((flatIndexIsResettable and payer) or ((not flatIndexIsResettable) and (not payer))):
                        notionalsNode = legroot.find("Notionals")
                        #fxResetNode = ET.SubElement(notionalsNode, "FXReset")
                        fxResetNode = ET.Element("FXReset")
                        notionalsNode.insert(1,fxResetNode)
                        fgnCcy = spreadCCY if flatIndexIsResettable else flatCCY
                        fgnCcyNode = ET.SubElement(fxResetNode, "ForeignCurrency")
                        fgnCcyNode.text = fgnCcy
                        fgnAmtNode = ET.SubElement(fxResetNode, "ForeignAmount")
                        fgnAmtNode.text = str(notional)
                        fxIdxNode = ET.SubElement(fxResetNode, "FXIndex")
                        fxIdxNode.text = "FX-ECB-" + spreadCCY + "-" + flatCCY
                        fixNode = ET.SubElement(fxResetNode, "FixingDays")
                        fixNode.text = fixingDays
                        fixCalNode = ET.SubElement(fxResetNode, "FixingCalendar")
                        fixCalNode.text = "TARGET,US,UK" # Temp

            else: #Tenor Basis Swap
                payConvention = "MF"
                currency = details[4]
                maturity = details [5]
                convention = basisConventions[curve]
                calendar = currency
                receiveFrequency = convention.find("ReceiveFrequency")
                fixingDays = getFixingDaysForCCY(currency)
                spotLag = fixingDays
                if (payer):
                    index = convention.find("ReceiveIndex").text
                    if receiveFrequency is not None:
                        tenor = receiveFrequency.text
                    else:
                        tenor = index.split("-")[2]
                        receiveFrequency = tenor
                    spread = marketData[tradeQuote]
                else:
                    index = convention.find("PayIndex").text
                    tenor = index.split("-")[2]
                    spread = "0"

                if (receiveFrequency is not None) and payer:
                    hasSubPeriods = "true" if receiveFrequency > tenor else "false"
                    subPeriodsCouponType =  convention.find("SubPeriodsCouponType").text \
                        if convention.find("SubPeriodsCouponType") is not None else "Compounding"
                    isAveraged = "true" if subPeriodsCouponType == "Average" else "false"
                    includeSpread = convention.find("IncludeSpread").text \
                        if convention.find("IncludeSpread") is not None else "false"
            dayCounter = currencyDaycountConvention(currency)
        else:
            spotLag = details[3][0:-1]
            currency = details[2]
            dayCounter = currencyDaycountConvention(currency)
            if tradeType == "Swap":
                convention = swapConventions[curve]
                calendar = convention.find("FixedCalendar").text
                fixingDays = spotLag
                tenor = details[4]
            elif tradeType == "OIS":
                if currency == "USD":
                    calendar = "US-FED"
                if currency == "JPY":
                    dayCounter = "A365F"
                convention = oisConventions[curve]
                rule = convention.find("Rule").text
                spotLag = convention.find("SpotLag").text
                paymentLag = convention.find("PaymentLag").text
                fixingDays = spotLag
                fixedFreq = convention.find("FixedFrequency").text
                tenor = freqToTenor(fixedFreq)
            else:
                debugPrint("Failed to find convention for " + curve)
                return False
            index = convention.find("Index").text

            payer = True

            payConvention = convention.find("FixedConvention").text
            maturity = details[5]


    except Exception as e:
        debugPrint("Error generating floating leg for " + tradeQuote + " - " + curve)
        return False
    if (currency != "EUR"):
        fxRate = float(marketData["FX/RATE/EUR/" + currency])
        notional *= fxRate

    startDate = addTenorToDate(startDate, spotLag + "D") 
    while startDate.weekday() > 4: #or startDate.weekday() < 1:
        startDate += datetime.timedelta(days=1)
    endDate = addTenorToDate(startDate, maturity)
    endDate = endDate.strftime('%Y%m%d')
    startDate = startDate.strftime("%Y%m%d")
    legroot.find("Payer").text = "true" if payer else "false"
    legroot.find("Notionals/Notional").text = str(notional)
    legroot.find("Currency").text = currency
    legroot.find("DayCounter").text = dayCounter
    legroot.find("PaymentConvention").text = payConvention
    legroot.find("PaymentLag").text = paymentLag
    # floatinglegdata
    # index
    legroot.find("FloatingLegData/Index").text = index
    legroot.find("FloatingLegData/FixingDays").text = str(fixingDays)
    legroot.find("FloatingLegData/Spreads/Spread").text = spread

    if tradeType == "Tenor Basis Swap" and payer:
        floatingLegData = legroot.find("FloatingLegData")
        floatingLegData.append(ET.Element("HasSubPeriods"))
        floatingLegData.append(ET.Element("IsAveraged"))
        floatingLegData.append(ET.Element("IncludeSpread"))

        floatingLegData.find("HasSubPeriods").text = hasSubPeriods
        floatingLegData.find("IsAveraged").text = isAveraged
        floatingLegData.find("IncludeSpread").text = includeSpread

    # scheduledata
    # rules

    legroot.find("ScheduleData/Rules/StartDate").text = startDate
    legroot.find("ScheduleData/Rules/EndDate").text = endDate
    legroot.find("ScheduleData/Rules/Tenor").text = tenor
    if rule != "":
        rules = legroot.find("ScheduleData/Rules")
        rules.append(ET.Element("Rule"))
        rules.find("Rule").text = rule
    legroot.find("ScheduleData/Rules/Calendar").text = calendar if calendar != "" else currency
    legroot.find("ScheduleData/Rules/Convention").text = payConvention
    legroot.find("ScheduleData/Rules/TermConvention").text = payConvention

    return True



def CreateFixedLeg(legroot, tradeType, tradeQuote, curve, details):
    currency = ""
    notional = 1000000
    dayCounter = ""
    payConvention = ""
    rate = ""
    startDate = datetime.datetime.strptime(startDateStr, "%Y%m%d").date()
    endDate = ""
    tenor = ""
    calendar = ""
    termConvention = ""
    payer = "false"
    rule = ""
    paymentLag = "0"
    currency = details[2]
    fixingDays = details[3]
    maturity = details[5]
    try:
        if tradeType == "Swap":
            convention = swapConventions[curve]
            calendar = convention.find("FixedCalendar").text
            fixingDays = getFixingDaysForCCY(currency)
        elif tradeType == "OIS":
            if currency == "USD":
                calendar = "US-FED"
            convention = oisConventions[curve]
            rule = convention.find("Rule").text
            paymentLag = convention.find("PaymentLag").text
            fixingDays = convention.find("SpotLag").text
        else:
            debugPrint("could not find convention for " + curve)
            return False
    except:
        debugPrint ("could not find convention for " + curve)
        return False


    try:
        rate = marketData[tradeQuote]
    except:
        debugPrint("cound not find market data for " + tradeQuote)
        return False

    startDate = addTenorToDate(startDate, fixingDays + "D")
    while startDate.weekday()  > 4: #or startDate.weekday() < 1:
        startDate += datetime.timedelta(days=1)
    endDate = addTenorToDate(startDate, maturity)
    endDate = endDate.strftime('%Y%m%d')
    startDate = startDate.strftime("%Y%m%d")
    termConvention = convention.find("FixedConvention").text
    dayCounter = convention.find("FixedDayCounter").text
    tenor = convention.find("FixedFrequency").text
    tenor = freqToTenor(tenor)
    if currency != "EUR":
        try:
            fxRate = float(marketData["FX/RATE/EUR/" + currency])
            notional *= fxRate
        except:
            debugPrint("FX rate not found for EUR/" + currency)
            return False
    legroot.find("Notionals/Notional").text = str(notional)
    legroot.find("Currency").text = currency
    legroot.find("DayCounter").text = dayCounter
    legroot.find("PaymentConvention").text = termConvention
    legroot.find("PaymentLag").text = paymentLag
    legroot.find("FixedLegData/Rates/Rate").text = rate
    legroot.find("ScheduleData/Rules/StartDate").text = startDate
    legroot.find("ScheduleData/Rules/EndDate").text = endDate
    legroot.find("ScheduleData/Rules/Tenor").text = tenor
    if rule != "":
        rules = legroot.find("ScheduleData/Rules")
        rules.append(ET.Element("Rule"))
        rules.find("Rule").text = rule
    legroot.find("ScheduleData/Rules/Calendar").text = calendar if calendar != "" else currency
    legroot.find("ScheduleData/Rules/Convention").text = termConvention
    legroot.find("ScheduleData/Rules/TermConvention").text = termConvention

    return True

def  addTradeToPortfolio(tradeRoot, oisPortfolio, cur=""):
    if oisPortfolio:
        oisTrades.append(tradeRoot)
    else:
        if cur == "EUR":
            EURxoisTrades.append(tradeRoot)
        else:
            USDxoisTrades.append(tradeRoot)

def getFixingDaysForCCY(currency):
    if currency == 'GBP':
        return "0"
    elif currency == 'CHF':
        return "2"
    else:
        return "2"



def CreateTrade(tradeType, tradeQuote, curve, ois):
    details = tradeQuote.split("/")

    if tradeType == "Swap":
        newRoot = copy.deepcopy(swapTemplate)
        id = "Swap_" + details[2] + "_" + details[4] + "_" + details[5]
        newRoot.set("id", id)
        legs = newRoot.findall(".//LegData")
        for leg in legs:
            legType = leg.find("LegType").text
            if legType == "Fixed":
                legCreated = CreateFixedLeg(leg, tradeType,tradeQuote, curve, details)
                if not legCreated:
                    return
            if legType == "Floating":
                legCreated = CreateFloatingLeg(leg, tradeType, tradeQuote, curve, details, basis=False)
                if not legCreated:
                    return
        try:

            addTradeToPortfolio(newRoot, ois)

        except Exception as e:
            debugPrint("Failed to append trade")
        return
    elif tradeType == "FX Forward":
        newRoot = copy.deepcopy(FXFWDTemplate)
        baseRateID = "/".join(details[0:-1])
        baseRateID = baseRateID.replace("FXFWD", "FX")
        tenor = details[4]

        try:
            convention = FXFWDConventions[curve]
        except:
            debugPrint("convention not found: " + curve)
            return
        try:
            pips = marketData[tradeQuote]
            baseRate = marketData[baseRateID]
        except:
            debugPrint("Market data not found for: " + startDateStr + " " + tradeQuote)
            return


        spotDays = convention.find("SpotDays").text
        sourceCCY = convention.find("SourceCurrency").text
        targetCCY = convention.find("TargetCurrency").text
        ptsFactor = convention.find("PointsFactor").text
        advCal = convention.find("AdvanceCalendar").text
        spotRel = convention.find("SpotRelative").text
        startDate = datetime.datetime.strptime(startDateStr, "%Y%m%d").date()
        startDate = addTenorToDate(startDate, spotDays+"D")
        valueDate = addTenorToDate(startDate, tenor).strftime('%Y-%m-%d')
        FXFWDRate = float(baseRate) + float(pips) / float(ptsFactor)
        id = "FXFWD_" + sourceCCY + "_" + targetCCY + "_" + tenor
        newRoot.set("id", id)
        newRoot.find("FxForwardData/ValueDate").text = valueDate
        newRoot.find("FxForwardData/BoughtCurrency").text = sourceCCY
        boughtAmount = newRoot.find("FxForwardData/BoughtAmount").text
        newRoot.find("FxForwardData/SoldCurrency").text = targetCCY
        newRoot.find("FxForwardData/SoldAmount").text = str(float(boughtAmount) * FXFWDRate)

        addTradeToPortfolio(newRoot, ois, sourceCCY)
    elif tradeType == "Deposit":
        currency = ""
        tenor = ""
        maturity = ""
        trade = ET.Element("Trade")
        trade.tag = r'id="TEST"'
    elif tradeType == "FRA":
        newRoot = copy.deepcopy(FRATemplate)
        notional = 1000000
        try:
     #       swapConvention = swapConventions[curve.replace("FRA", "SWAP")]
            fraConvention = FRAConventions[curve]
        except:
            debugPrint("convention not found: " + curve)
            return
        try:
            fixedRate = marketData[tradeQuote]
        except:
            debugPrint("Market data not found for: " + startDateStr + " " + tradeQuote)
            return
        currency = details[2]
        if currency != "EUR":
            fxRate = float(marketData["FX/RATE/EUR/"+currency])
            notional *= fxRate
        tenor = details[4]

        startDate = datetime.datetime.strptime(startDateStr, "%Y%m%d").date()
        forwardWait = details[3]
        index = fraConvention.find("Index").text
        fixingDays = getFixingDaysForCCY(currency)
        startDate = addTenorToDate(startDate, fixingDays + "D")
        startDate =  addTenorToDate(startDate, forwardWait)
        endDate = addTenorToDate(startDate, tenor)
        longshort = "Long"

        id = "FRA_" + currency + "_" + forwardWait + "_" + tenor

        newRoot.set("id", id)
        newRoot.find("ForwardRateAgreementData/StartDate").text = startDate.strftime('%Y-%m-%d')
        newRoot.find("ForwardRateAgreementData/EndDate").text = endDate.strftime('%Y-%m-%d')
        newRoot.find("ForwardRateAgreementData/Currency").text = currency
        newRoot.find("ForwardRateAgreementData/Index").text = index
        newRoot.find("ForwardRateAgreementData/LongShort").text = longshort
        newRoot.find("ForwardRateAgreementData/Strike").text = marketData[tradeQuote]
        newRoot.find("ForwardRateAgreementData/Notional").text = str(notional)
        addTradeToPortfolio(newRoot, ois)

    elif tradeType == "Cross Currency Basis Swap":
        newRoot = copy.deepcopy(xccyBasisTemplate)
        id = "xccyBasisSwap_" + "_".join(details[2:])
        newRoot.set("id", id)
        legs = newRoot.findall(".//LegData")
        legCreated = CreateFloatingLeg(legs[0], tradeType,tradeQuote, curve, details, basis=True, payer=True)
        if not legCreated:
            return
        legCreated = CreateFloatingLeg(legs[1], tradeType, tradeQuote, curve, details, basis=True, payer=False)
        if not legCreated:
            return
        try:
            baseCur = details[2]
            addTradeToPortfolio(newRoot, ois, baseCur)


        except Exception as e:
            debugPrint("Failed to append trade")
        return

    elif tradeType == "Tenor Basis Swap":
        newRoot = copy.deepcopy(basisTemplate)
        id = "BasisSwap_" + "_".join(details[2:])
        newRoot.set("id", id)
        legs = newRoot.findall(".//LegData")
        legCreated = CreateFloatingLeg(legs[0], tradeType,tradeQuote, curve, details, basis=True, payer=True)
        if not legCreated:
            return
        legCreated = CreateFloatingLeg(legs[1], tradeType, tradeQuote, curve, details, basis=True, payer=False)
        if not legCreated:
            return
        try:

            addTradeToPortfolio(newRoot, ois)

        except Exception as e:
            debugPrint("Failed to append trade")
        return

    elif tradeType == "OIS":
        newRoot = copy.deepcopy(swapTemplate)
        id = "ois_" + "_".join(details[2:])
        newRoot.set("id", id)
        legs = newRoot.findall(".//LegData")
        for leg in legs:
            legType = leg.find("LegType").text
            if legType == "Fixed":
                legCreated = CreateFixedLeg(leg, tradeType, tradeQuote, curve, details)
                if not legCreated:
                    return
            if legType == "Floating":
                legCreated = CreateFloatingLeg(leg, tradeType, tradeQuote, curve, details, basis=False)
                if not legCreated:
                    return
        try:

            addTradeToPortfolio(newRoot, ois)

        except Exception as e:
            debugPrint("Failed to append trade")
        return

    else:
        currency = ""
        tenor = ""
        maturity = ""
        trade = ET.Element("Trade")
        trade.tag = r'id="TEST"'




importMarketData(marketDataInputFile)


conventions = ET.parse(conventions_file)
conventions = conventions.getroot()

for conv in conventions:
    type = conv.tag
    ID = conv.find("./Id").text
    if type == "Swap":
        swapConventions[ID] = conv
    elif type == "FRA":
        FRAConventions[ID] = conv
    elif type == "FX":
        FXFWDConventions[ID] = conv
    elif type == "OIS":
        oisConventions[ID] = conv
    elif type == "CrossCurrencyBasis":
        xccyBasisConventions[ID] = conv
    elif type == "TenorBasisSwap":
        basisConventions[ID] = conv

configs = ET.parse(curve_config_file)
root = configs.getroot()
yield_curves = root.find('YieldCurves')


for child in list(yield_curves):
    currentCurve = child.find("CurveId").text
    if curveID == currentCurve or curveID == 'all':
        types = child.findall(".//Simple")
        types.extend(child.findall(".//TenorBasis"))
        for quoteType in types:
            conventionID = quoteType.find("Conventions").text
            type = quoteType.find("Type").text
            quotes = list(quoteType.find("Quotes"))
            for quote in quotes:
                CreateTrade(type,quote.text,quoteType.find(".//Conventions").text,ois=True)
        types = child.findall(".//CrossCurrency")
        for quoteType in types:
            conventionID = quoteType.find("Conventions").text
            type = quoteType.find("Type").text
            quotes = list(quoteType.find("Quotes"))
            for quote in quotes:

                CreateTrade(type,quote.text,quoteType.find(".//Conventions").text,ois=False)



outputfile = open(oisFile, "w")
xmlString = minidom.parseString(ET.tostring(oisTrades)).toprettyxml(indent="  ")
xmlString = '\n'.join([line for line in xmlString.split('\n') if line.strip()])
outputfile.write(xmlString)
outputfile.close()

outputfile = open(EURxoisFile, "w")
xmlString = minidom.parseString(ET.tostring(EURxoisTrades)).toprettyxml(indent="  ")
xmlString = '\n'.join([line for line in xmlString.split('\n') if line.strip()])
outputfile.write(xmlString)
outputfile.close()

outputfile = open(USDxoisFile, "w")
xmlString = minidom.parseString(ET.tostring(USDxoisTrades)).toprettyxml(indent="  ")
xmlString = '\n'.join([line for line in xmlString.split('\n') if line.strip()])
outputfile.write(xmlString)
outputfile.close()

debugPrint("Hello World!")




