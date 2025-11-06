#!/usr/bin/env python

import sys
import os
import pandas as pd
import numpy as np
import csv
import datetime
from dateutil import parser
from lxml import etree as et
from io import StringIO

def checkErrorsAndRunTime(app):
    errors = app.getErrors()
    print ("Run time: %.2f sec" % app.getRunTime())       
    print ("Errors: %d" % len(errors))
    if len(errors) > 0:
        for e in errors:
            print(e)        

def writeList(lst):
    print()
    for r in lst:
        print("-", r)

def getSamples(simulationXml):
    tree = et.parse(simulationXml)
    root = tree.getroot()
    samples = int(root.find("Parameters/Samples").text.replace(' ', ''))
    return samples
    
def getAsOfDate(oreXml):
    doc = et.parse(oreXml)
    nodes = doc.xpath('//ORE/Setup/Parameter[@name="asofDate"]')
    asof = nodes[0].text
    return asof

def getTimeDifference(fromDateString, toDateString):   
    d0 = parser.parse(fromDateString) 
    d1 = parser.parse(toDateString) 
    #print("d0 =", d0)
    #print("d1 =", d1)
    delta = d1 - d0
    delta_fraction = delta.days / 365.25
    return delta_fraction

def scenarioToMarket(simulationXml, scenarioFile, filterSample, outputDir):

    ###############################################################################################
    # Scan the simulation.xml to extract data structures
    # This should match the content of the scenario dump by construction of the exposure simulation

    print("read simulation file:", simulationXml) 
    tree = et.parse(simulationXml)
    root = tree.getroot()

    baseCcy = root.find("Market/BaseCurrency").text.replace(' ', '')
    print("base currency:", baseCcy)

    #currencies = [ "EUR", "USD" ]
    #fxspots = [ "USDEUR" ]
    currencies = []
    fxspots = []
    for ccy in root.findall("Market/Currencies/Currency"):
        currencies.append(ccy.text)
        if ccy.text != baseCcy:
            fxspots.append(ccy.text.replace(' ', '') + baseCcy)
    print("currencies:", currencies)
    print("fxspots:", fxspots)
    
    #indices = [ "EUR-EURIBOR-6M", "EUR-EURIBOR-3M", "EUR-ESTER", "USD-LIBOR-3M", "USD-SOFR" ]
    indices = []
    for index in root.findall("Market/Indices/Index"):
        indices.append(index.text.replace(' ', '')) 
    print("indices:", indices)

    #fxvols = [ "USDEUR" ]
    fxvols = []
    for pair in root.findall("Market/FxVolatilities/CurrencyPairs/CurrencyPair"):
        fxvols.append(pair.text.replace(' ', '')) 
    print("fxvols:", fxvols)

    #swaptionvols = [ "EUR" ]
    swaptionvols = []
    for ccy in root.findall("Market/SwaptionVolatilities/Currencies/Currency"):
        swaptionvols.append(ccy.text.replace(' ', '')) 
    print("swaptionvols:", swaptionvols)

    #curve_tenors = ["3M", "6M", "1Y", "2Y", "3Y", "4Y", "5Y", "7Y", "10Y", "12Y", "15Y", "20Y" ]
    tmp = root.find("Market/YieldCurves/Configuration/Tenors").text
    curve_tenors = tmp.replace(' ', '').split(',')
    print("curve_tenors:", curve_tenors)

    #fxvol_expiries = [ "6M", "1Y", "2Y", "3Y", "4Y", "5Y", "7Y", "10Y" ]
    tmp = root.find("Market/FxVolatilities/Expiries").text
    fxvol_expiries = tmp.replace(' ', '').split(',')
    print("fxvol_expiries:", fxvol_expiries)

    #swaption_expiries = [ "6M", "1Y", "2Y", "3Y", "5Y", "10Y", "12Y", "15Y", "20Y" ]
    #swaption_terms = [ "1Y", "2Y", "3Y", "4Y" , "5Y", "7Y", "10Y", "15Y", "20Y", "30Y"]
    tmp = root.find("Market/SwaptionVolatilities/Expiries").text
    swaption_expiries = tmp.replace(' ', '').split(',')
    tmp = root.find("Market/SwaptionVolatilities/Terms").text
    swaption_terms = tmp.replace(' ', '').split(',')
    #tmp = root.find("Market/SwaptionVolatilities/StrikeSpreads").text
    #swaption_spreads = tmp.replace(' ', '').split(',')
    print("swaption_expiries", swaption_expiries)
    print("swaption_terms", swaption_terms)
    #print("swaption_spreads", swaption_spreads)

    ###############################################################################
    # Load the scenariodump.csv into a dataframe and map to an ORE market data file

    print("read scenario file:", scenarioFile)
    data = pd.read_csv(scenarioFile)

    referenceDates = data['#Date'].unique()
    scenarios = data['Scenario'].unique()
    print("scenarios:", scenarios)
    print("reference dates:", referenceDates)

    # delete market file since we append below
    file = outputDir + '/marketdata.csv'
    if os.path.isfile(file):
        print ("remove file:", file)
        os.remove(file)
        
    # Convert each row into a market data file for the row's reference date and scenario
    # Actually, append all data (dates) per scenario into a single market data file

    marketFile = outputDir + '/marketdata.csv'

    for index, row in data.iterrows():

        refdate = row["#Date"]
        scenario = int(row["Scenario"])
        
        if scenario != filterSample :
            continue
            
        print("append", refdate, "to market file", marketFile)
        
        with open(marketFile, 'a', newline='') as file1:
                writer = csv.writer(file1, delimiter=',')
                writer.writerow(["#Sample=" + str(filterSample)])

                # Discount curve data
                for ccy in currencies:
                    for i in range(len(curve_tenors)):
                        key = "DiscountCurve/" + ccy + "/" + str(i)
                        value = row[key]
                        orekey = "DISCOUNT/RATE/" + ccy + "/" + ccy + "/" + curve_tenors[i]
                        writer.writerow([refdate, orekey, value])

                # Index curves
                for index in indices:
                    for i in range(len(curve_tenors)):
                        key = "IndexCurve/" + index + "/" + str(i)
                        value = row[key]
                        ccy = index[0:3]
                        orekey = "DISCOUNT/RATE/" + ccy + "/" + index + "/" + curve_tenors[i]
                        writer.writerow([refdate, orekey, value])

                # FX Spot
                for pair in fxspots:
                    key = "FXSpot/" + pair + '/0'
                    value = float(row[key])
                    ccy1 = pair[0:3]
                    ccy2 = pair[3:6]
                    orekey = "FX/RATE/" + ccy1 + "/" + ccy2
                    writer.writerow([refdate, orekey, value])
                    orekey = "FX/RATE/" + ccy2 + "/" + ccy1
                    writer.writerow([refdate, orekey, 1.0/value])
                    
                # FX Vols
                for pair in fxvols:
                    for i in range(len(fxvol_expiries)):
                        key = "FXVolatility/" + pair + "/" + str(i)
                        value = row[key]
                        ccy1 = pair[0:3]
                        ccy2 = pair[3:6]
                        orekey = "FX_OPTION/RATE_LNVOL/" + ccy1 + "/" + ccy2 + "/" + fxvol_expiries[i] + "/ATM"
                        writer.writerow([refdate, orekey, value])

                # Swaption Vols
                for ccy in swaptionvols:
                    for i in range(len(swaption_expiries)):
                        for j in range(len(swaption_terms)):
                            index = i * len(swaption_terms) + j;
                            key = "SwaptionVolatility/" + ccy + "/" + str(index)
                            value = row[key]
                            orekey = "SWAPTION/RATE_NVOL/" + ccy + "/" + swaption_expiries[i] + "/" + swaption_terms[j] + "/ATM"
                            writer.writerow([refdate, orekey, value])

    return referenceDates

def scenarioToFixings(gzFileName, asofDates, filterSample, outputDir):
    import gzip
    import csv
    dateSet = set()
    sampleSet = set()
    keySet = set()

    keyDict = {}
    keyCount = 0

    # row list with Date and Numeraire for the selected filterSample
    numeraireColumns = ["#asofDate", "Numeraire"]
    numeraireRows = []
    
    # The samples in the rawcube.csv start with sample 1, in the scenariodata with sample 0
    # We expect that the 'filterSample' argument follows the rawcube convention, so we
    # subtract 1 to match with the content in scenariodata here.
    # The file appendix uses the argument filterSample, to match the market data file convention.
    fixingFile = outputDir + '/fixings.csv'
    print("write to fixing file:", fixingFile)
    with open(fixingFile, 'w', newline='') as file1:
        writer = csv.writer(file1, delimiter=',')
        with gzip.open(gzFileName, mode='rt') as file:
            reader = csv.reader(file, delimiter = ',', quotechar="'")
            writer.writerow(["#Sample=" + str(filterSample)])
            for row in reader:
                if (row[0].startswith('#') and len(row)==2):
                    type = row[0].strip('#').strip(' ')
                    indexName = row[1]
                    if indexName == '':
                        indexName = 'NUMERAIRE'
                    keyDict.update({str(keyCount) : indexName})
                    keyCount = keyCount + 1

                if (not row[0].startswith('#')):
                    # dates start with index 1 in scenariodata.csv.gz,
                    # so subtract 1 to match with the asofDate list passed
                    date = int(row[0]) - 1
                    asof = asofDates[date]
                    sample = row[1]
                    key = row[2]
                    value = row[3]
                    # this is just to report unique lists below, see the print statements
                    dateSet.add(int(date))
                    sampleSet.add(int(sample))
                    keySet.add(int(key))
                    # write out the sected sample
                    if sample == str(filterSample - 1):
                        index = keyDict[key]
                        # Skip the three digit FX indices here (foreign ccy code)
                        if len(index) > 3 and index != "NUMERAIRE":
                            writer.writerow([asof, index, value])
                        if index == "NUMERAIRE":
                            numeraireRows.append([asof, value])

            print("dates:  ", len(dateSet))
            print("samples:", len(sampleSet))
            print("keys:   ", len(keySet))
            print("keyDict:", keyDict)

    numeraireData = pd.DataFrame(numeraireRows, columns=numeraireColumns)

    return numeraireData


def num(asof, numeraireData):
    for index, row in numeraireData.iterrows():
        d = row["#asofDate"]
        n = float(row["Numeraire"])
        if d == asof:
            return n

def rawCubeFilter(rawCubeFileName, filterSample):

    print("read raw cube file:", rawCubeFileName)
    data = pd.read_csv(rawCubeFileName)

    columns = ["TradeId", "Date", "NPV"]
    rowlist = []

    for index, row in data.iterrows():

        trade = row["#Id"]
        date = row["Date"]
        sample = row["Sample"]
        depth = row["Depth"]
        value = float(row["Value"])

        if depth == 0 and sample == filterSample:
            rowlist.append([trade, date, '{:.6f}'.format(value)])

    cubeData = pd.DataFrame(rowlist, columns=columns)

    return cubeData

def netCubeFilter(cubeFileName, nettingSetId, filterSample, asof):

    print("read net cube file:", cubeFileName)
    data = pd.read_csv(cubeFileName)

    columns = ["Id", "Date", "Time", "NPV"]
    rowlist = []

    for index, row in data.iterrows():

        nid = row["#Id"]
        date = row["Date"]
        time = getTimeDifference(asof, date)
        sample = row["Sample"]
        depth = row["Depth"]
        value = float(row["Value"])

        if depth == 0 and sample == filterSample and nid == nettingSetId:
            rowlist.append([nid, date, '{:.6f}'.format(time), '{:.6f}'.format(value)])

        #print (index, row)
    cubeData = pd.DataFrame(rowlist, columns=columns)

    return cubeData

def npvComparison(undiscounted, discounted, numeraires):

    result = pd.concat([undiscounted, discounted, numeraires], axis=1)

    columns = ["Id", "Date", "NPV(Base)", "NPV(Base)/Numeraire", "NPV", "Difference", "DifferenceWithoutDiscounting"]
    rowlist = []
    
    for index, row in result.iterrows():
        undisc = float(row["NPV(Base)"])
        disc = float(row["NPV"])
        date = row["asofDate"]
        nid = row["Id"]
        num = float(row["Numeraire"])
        
        rowlist.append([nid, date,
                        '{:.6f}'.format(undisc),
                        '{:.6f}'.format(undisc/num),
                        '{:.6f}'.format(disc),
                        '{:.6f}'.format(disc - undisc/num),
                        '{:.6f}'.format(disc - undisc)])

    comparisonData = pd.DataFrame(rowlist, columns=columns)

    return comparisonData

def expectedSimmEvolution(simmCubeFile, outputFile):
    print("read simm cube file:", simmCubeFile)
    data = pd.read_csv(simmCubeFile)

    samples = data["Sample"].unique()
    
    print ("number of samples:", len(samples))
    print ("unique samples:", samples)
    
    print("add dataframe for sample", 0)
    df = data[data["Sample"] == samples[0]]    
    df = df.drop("Sample", axis=1)
    df = df.set_index(["AsOfDate","Time","Currency","SimmSide","Portfolio"])
    #print(df)
    
    for i in range(1,len(samples)):
        print("add dataframe for sample", i)
        df2 = data[data["Sample"] == samples[i]]
        df2 = df2.drop("Sample", axis=1)
        df2 = df2.set_index(["AsOfDate","Time","Currency","SimmSide","Portfolio"])
        df = df + df2
        #print(df)
    
    df["InitialMargin"] = df["InitialMargin"] / len(samples)

    #print()
    #print(df)

    df.to_csv(outputFile, sep=',')

def simmEvolution(sample, simmCubeFile, outputFile, depth):
    print("read simm cube file:", simmCubeFile)
    data = pd.read_csv(simmCubeFile)

    #samples = data["Sample"].unique()
    #print ("number of samples:", len(samples))
    #print ("unique samples:", samples)

    df = data[data["Sample"] == sample]
    df2 = df[df["Depth"] == depth]
    df3 = df2.drop("Sample", axis=1)
    df4 = df3[df3["Time"] < 10.1]
    df4 = df4.set_index(["AsOfDate","Time","Currency","SimmSide"])

    df4.to_csv(outputFile, sep=',')

def fxVegaEvolution(sample, simmCubeFile, outputFile, depth):
    print("read simm cube file:", simmCubeFile)
    data = pd.read_csv(simmCubeFile)

    df = data[data["Sample"] == sample]
    df2 = df[df["Depth"] == depth]
    df3 = df2.drop("Sample", axis=1)
    df4 = df3[df3["Time"] < 10.1]
    df4 = df4.set_index(["AsOfDate","Time","Currency","SimmSide"])

    df4.to_csv(outputFile, sep=',')
    
def expiryDate(filterSample, trade, rawCubeFile, expiryDepth):

    data = pd.read_csv(rawCubeFile)

    for index, row in data.iterrows():
        
        trade = row["#Id"]
        date = row["Date"]
        sample = row["Sample"]
        depth = row["Depth"]
        value = row["Value"]

        if depth == expiryDepth and sample == filterSample and not np.isnan(value):
            return date

    return ""
