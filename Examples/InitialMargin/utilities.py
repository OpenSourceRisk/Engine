#!/usr/bin/env python

import os
import pandas as pd
import numpy as np
import csv
from lxml import etree as et

def scenarioToMarket(simulationXml, scenarioFile, outputDir):

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
    print("swaption_expiries", swaption_expiries)
    print("swaption_terms", swaption_terms)
    
    
    #############################################################################
    # Load the scenariodump.csv into a dataframe and map to ORE market data files

    print("read scenario file:", scenarioFile)
    data = pd.read_csv(scenarioFile)

    referenceDates = data['#Date'].unique()
    scenarios = data['Scenario'].unique()
    print("scenarios:", scenarios)
    print("reference dates:", referenceDates)

    # delete market scenario files since we append below
    for s in scenarios:
        file = outputDir + '/marketdata-' + str(s) + '.csv'
        if os.path.isfile(file):
            print ("remove file:", file)
            os.remove(file)
        
    # Convert each row into a market data file for the row's reference date and scenario
    # Actually, append all data per scenario into a single market data file

    for index, row in data.iterrows():

        refdate = row["#Date"]
        scenario = row["Scenario"]

        #marketFile = outputDir + '/marketdata-' + str(scenario) + '-' + refdate + '.csv'
        marketFile = outputDir + '/marketdata-' + str(scenario) + '.csv'
        print("append to market file:", marketFile)
        
        #with open(marketFile, 'w', newline='') as file1:
        with open(marketFile, 'a', newline='') as file1:
                writer = csv.writer(file1, delimiter=',')

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
                            #orekey = "SWAPTION/RATE_NVOL/" + ccy + "/" + swaption_expiries[i] + "/" + swaption_terms[j] + "/ATM"
                            #writer.writerow([refdate, orekey, 0.0065])

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
    fixingFile = outputDir + '/fixings-' + str(filterSample) + '.csv'
    print("write to fixing file:", fixingFile)
    with open(fixingFile, 'w', newline='') as file1:
        writer = csv.writer(file1, delimiter=',')
        with gzip.open(gzFileName, mode='rt') as file:
            reader = csv.reader(file, delimiter = ',', quotechar="'")
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

def npvComparison(undiscounted, discounted, numeraires):

    result = pd.concat([undiscounted, discounted, numeraires], axis=1)

    columns = ["TradeId", "Date", "NPV(Base)", "NPV(Base)/Numeraire", "NPV", "Difference", "DifferenceWithoutDiscounting"]
    rowlist = []
    
    for index, row in result.iterrows():
        undisc = float(row["NPV(Base)"])
        disc = float(row["NPV"])
        num = float(row["Numeraire"])
        date = row["asofDate"]
        trade = row["TradeId"]
        
        rowlist.append([trade, date,
                        '{:.6f}'.format(undisc),
                        '{:.6f}'.format(undisc/num),
                        '{:.6f}'.format(disc),
                        '{:.6f}'.format(disc - undisc/num),
                        '{:.6f}'.format(disc - undisc)])

    comparisonData = pd.DataFrame(rowlist, columns=columns)

    return comparisonData
