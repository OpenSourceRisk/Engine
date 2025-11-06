import requests
import json

url = "http://localhost:5001/api/analytics"

# request body is ore.xml in json form
body = {
    "setup": {
        "inputPath": "Input",
        "outputPath": "Output",
        "resultsUri": "http://localhost:5000/report",
        "nThreads": 1,
        "asofDate": "2016-02-05",
        "implyTodaysFixings": "N",
        "logFile": "log.txt",
        "observationModel": "Disable",
        "marketData": "http://localhost:5000/file/market_20160205.txt",
        "fixingData": "http://localhost:5000/file/fixings_20160205.txt",
        "conventionsUri": "http://localhost:5000/file/conventions.xml",
        "pricingEnginesUri": "http://localhost:5000/file/pricingengine.xml",
        "portfolioUri": "http://localhost:5000/file/portfolio.xml",
        "marketConfigUri": "http://localhost:5000/file/todaysmarket.xml",
        "curveConfigUri": "http://localhost:5000/file/curveconfig.xml",
        "observationModel": "Disable"
    },
    "markets": {
        "fxcalibration": "collateral_eur",
        "simulation": "collateral_eur",
        "pricing": "collateral_eur",
        "lgmcalibration": "collateral_inccy"
    },
    "analytics": {
        "npv": {
            "active": "Y",
            "baseCurrency": "EUR",
            "outputFileName": "npv.csv"
        },
        "cashflow": {
            "active": "Y",
            "outputFileName": "flow.csv"
        },
        "curves": {
            "active": "Y",
            "configuration": "default",
            "outputFileName": "curves.csv"
        },
        "sensitivity": {
            "active": "N"
        },
        "simulation": {
            "active": "Y",
            "simulationConfigUri": "http://localhost:5000/file/simulation.xml",
            "pricingEnginesUri": "http://localhost:5000/file/pricingengine.xml",
            "baseCurrency": "EUR",
            "scenariodump": "scenariodump.csv",
            "CubeUri": "cube.csv.gz",
            "storeFlows": "N",
            "storeSurvivalProbabilities": "N"
        },
        "stress": {
            "active": "N"
        },
        "parametricVar": {
            "active": "N"
        },
        "simm": {
            "active": "N"
        },
        "xva": {
            "active": "Y",
            "csaUri": "http://localhost:5000/file/netting.xml",
            "cubeFile": "cube.csv.gz",
            "scenarioFile": "scenariodata.csv.gz",
            "baseCurrency": "EUR",
            "exposureProfiles": "Y",
            "exposureProfilesByTrade": "Y",
            "quantile": 0.95,
            "calculationType": "Symmetric",
            "allocationMethod": "None",
            "marginalAllocationLimit": 1.0,
            "exerciseNextBreak": "N",
            "cva": "Y",
            "dva": "N",
            "dvaName": "BANK",
            "fva": "N",
            "fvaBorrowingCurve": "BANK_EUR_BORROW",
            "fvaLendingCurve": "BANK_EUR_LEND",
            "colva": "N",
            "collateralFloor": "N",
            "rawCubeOutputFile": "rawcube.csv",
            "netCubeOutputFile": "netcube.csv"
        }
    }
}

response = requests.post(url, json=body)

print("Status:", response.status_code)

print("Response:", response.text)
