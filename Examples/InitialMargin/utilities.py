#!/usr/bin/env python

import pandas as pd
import numpy as np
import csv
from lxml import etree as et

def scenarioToMarket(subdir='Dim2'):

    #####################################################
    # Scan the simulation.xml to extract data structures
    # This should match the content of the scenario dump

    simulationFile = 'Input/' + subdir + '/simulation.xml'
    print("read simulation file:", simulationFile) 
    tree = et.parse(simulationFile)
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
    
    
    ############################################
    # load the scenariodump.csv into a dataframe
    # and map to ORE market data files

    scenarioFile = 'Output/' + subdir + '/scenariodump.csv'
    print("read scenario file:", scenarioFile)
    data = pd.read_csv(scenarioFile)

    referenceDates = data['#Date'].unique()
    scenarios = data['Scenario'].unique()
    print("scenarios:", scenarios)
    print("reference dates:", referenceDates)

    # Convert each row into a market data file for the row's reference date and scenario

    for index, row in data.iterrows():

        refdate = row["#Date"]
        scenario = row["Scenario"]

        marketFile = 'Input/DimValidation/marketdata-' + str(scenario) + '-' + refdate + '.csv'
        print("write market file:", marketFile)
        
        with open(marketFile, 'w', newline='') as file1:
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
                    value = row[key]
                    ccy1 = pair[0:3]
                    ccy2 = pair[3:6]
                    orekey = "FX/RATE/" + ccy1 + "/" + ccy2
                    writer.writerow([refdate, orekey, value])
                    
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
                            orekey = "SWAPTION/RATE_LNVOL/" + ccy + "/" + swaption_expiries[i] + "/" + swaption_terms[j] + "/ATM"
                            writer.writerow([refdate, orekey, value])

