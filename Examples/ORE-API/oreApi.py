import json
import urllib.request
import urllib.parse
import ORE as ore
import pandas as pd
import requests
from flask import make_response


class MyCustomException(Exception):
    pass

class oreApi():
    def __init__(self, request):
        self.request = request

    def get_data(self):
        # get JSON data from postman
        json_data = self.request.get_data(as_text=True)

        # Parse the JSON data string into a dictionary
        data_dict = json.loads(json_data)

        return data_dict

    def format_report(self, report):
        # Format Headers into columns
        report_headers = [[i, report.header(i), report.columnType(i)] for i in range(report.columns())]

        formatted_report = dict()
        # Loop over headers and put them into the appropriate column
        for i, column_name, data_type in report_headers:
            if data_type == 0:
                formatted_report[column_name] = list(report.dataAsSize(i))
            if data_type == 1:
                formatted_report[column_name] = list(report.dataAsReal(i))
            if data_type == 2:
                formatted_report[column_name] = list(report.dataAsString(i))
            if data_type == 3:
                formatted_report[column_name] = [d.ISO() for d in report.dataAsDate(i)]
            if data_type == 4:
                formatted_report[column_name] = list(report.dataAsPeriod(i))

        formatted_report = pd.DataFrame(formatted_report)

        return formatted_report

    def postCSV(self, resultsUri, ore_app):
        # Get Names for Reports output
        outputs = list(ore_app.getReportNames())
        for outputName in outputs:
            # Set Report for each Report Name
            report = ore_app.getReport(outputName)
            # Format report
            formatted_report = self.format_report(report)
            # Get CSV for the formatted report
            df = pd.DataFrame.from_dict(formatted_report)
            csv_string = df.to_csv(index=False)
            # Set data of the body to be posted
            payload = {'data': csv_string}
            # Send post request with the csv as a body
            response = requests.post(f"{resultsUri}/{outputName}.csv", data=payload)

    def findEvalDate(self, asofDate):
        start_delimiter = "-"
        end_delimiter = "-"
        start_index = asofDate.find(start_delimiter) + 1
        end_index = asofDate.find(end_delimiter, start_index)
        day = int(asofDate[start_index:end_index])
        year = int(asofDate[:start_index - 1])
        month = int(asofDate[end_index + 1:])
        # Convert to ql.Date
        evalDate = ore.Date(day, month, year)
        ore.Settings.instance().evaluationDate = evalDate

    def setSimpleParameter(self, data_dict, lookupname, lookuplocation, param=None, paramtype=None):
        if lookuplocation in data_dict:
            if lookupname in data_dict[lookuplocation]:
                lookup_value = data_dict[lookuplocation][lookupname]
                # Set Parameter and check for types
                if param is not None:
                    if paramtype is not None:
                        param(paramtype(lookup_value))
                    else:
                        param(lookup_value)


    def setUriParameter(self, data_dict, lookupname, lookuplocation, param):
        if lookupname in data_dict[lookuplocation]:
            # Fetch xml url from the json
            lookup_value = data_dict[lookuplocation][lookupname]
            try:
                response = requests.get(lookup_value)
                response.raise_for_status()  # Raise an exception for non-2xx response codes
                xml_data = response.text
                param(xml_data)

            except Exception as e:
                error_message = str(e)
                raise MyCustomException(f"HTTPConnectionPool error: {error_message}") from e

    def setInputName(self, data_dict,lookupname, lookuplocation):
        if lookupname in data_dict[lookuplocation]:
            lookup_value = data_dict[lookuplocation][lookupname]
        else:
            lookup_value = ""
        return lookup_value

    def setupAnalytics(self, data_dict, params, requestedAnalytic):
        if requestedAnalytic == "npv":
            self.setupNPV(data_dict, params)
        elif requestedAnalytic == "cashflow":
            self.setupCashflow(data_dict, params)
        elif requestedAnalytic == "curves":
            self.setupCurves(data_dict, params)
        elif requestedAnalytic == "sensitivity":
            self.setupSensitivity(data_dict, params)
        elif requestedAnalytic == "stress":
            self.setupStress(data_dict, params)
        elif requestedAnalytic == "parametricVar":
            self.setupParametricVar(data_dict, params)
        elif requestedAnalytic == "simm":
            self.setupSimm(data_dict, params)
        elif requestedAnalytic == "xva":
            self.setupXVA(data_dict, params)
        elif requestedAnalytic == "simulation":
            self.setupExposure(data_dict, params)
        else:
            print("unknown analytic")


    def setupNPV(self, data_dict, params):
        print("Loading Analytics NPV variables")
        params.insertAnalytic("NPV")

        if "baseCurrency" in data_dict["analytics"]["npv"]:
            baseCurrency = data_dict["analytics"]["npv"]["baseCurrency"]
            params.setBaseCurrency(baseCurrency)

        if "additionalResults" in data_dict["analytics"]["npv"]:
            additionalResults = bool(data_dict["analytics"]["npv"]["additionalResults"])
            params.setOutputAdditionalResults(additionalResults)

        if "outputFileName" in data_dict["analytics"]["npv"]:
            outputFileName = data_dict["analytics"]["npv"]["outputFileName"]
            # params.setOutputCurves(outputFileName)

    def setupCashflow(self, data_dict, params):
        print("Loading Analytics cashflow variables")
        params.insertAnalytic("CASHFLOW")
        if "includePastCashflows" in data_dict["analytics"]["cashflow"]:
            params.setIncludePastCashflows(bool(data_dict["analytics"]["cashflow"]["includePastCashflows"]))
        else:
            params.setIncludePastCashflows(False)

    def setupCurves(self, data_dict, params):
        print("Loading Curves variables")
        params.setOutputCurves(True)
        self.setSimpleParameter(data_dict["analytics"], "grid", "curves", params.setCurvesGrid)
        self.setSimpleParameter(data_dict["analytics"], "configuration", "curves", params.setCurvesMarketConfig)
        self.setSimpleParameter(data_dict["analytics"], "outputTodaysMarketCalibration", "curves",
                                params.setOutputTodaysMarketCalibration, bool)

    def setupSensitivity(self, data_dict, params):
        print("Loading Sensitivity variables")
        params.insertAnalytic("SENSITIVITY")
        self.setSimpleParameter(data_dict["analytics"], "paraSensitivity", "sensitivity", params.setParSensi, bool)
        self.setSimpleParameter(data_dict["analytics"], "outputJacobi", "sensitivity", params.setOutputJacobi, bool)
        self.setSimpleParameter(data_dict["analytics"], "alignPillars", "sensitivity", params.setAlignPillars, bool)
        self.setUriParameter(data_dict["analytics"], "marketConfigUri", "sensitivity", params.setSensiSimMarketParams)
        self.setUriParameter(data_dict["analytics"], "sensitivityConfigUri", "sensitivity", params.setSensiScenarioData)
        self.setUriParameter(data_dict["analytics"], "pricingEnginesUri", "sensitivity", params.setSensiPricingEngine)
        self.setSimpleParameter(data_dict["analytics"], "outputSensitivityThreshold", "sensitivity", params.setSensiThreshold, float)
        
    def setupStress(self, data_dict, params):
        # Stress variables
        print("Loading Stress variables")
        params.insertAnalytic("STRESS")
        self.setUriParameter(data_dict, "pricingEnginesUri", "setup", params.setStressPricingEngine)
        self.setUriParameter(data_dict["analytics"], "pricingEnginesUri", "stress", params.setStressPricingEngine)
        self.setUriParameter(data_dict["analytics"], "marketConfigUri", "stress", params.setStressSimMarketParams)
        self.setUriParameter(data_dict["analytics"], "stressConfigUri", "stress", params.setStressScenarioData)
        self.setSimpleParameter(data_dict["analytics"], "outputThreshold", "stress", params.setStressThreshold, float)
    def setupParametricVar(self, data_dict, params):
        print("Loading parametricVar variables")
        params.insertAnalytic("VAR")
        self.setSimpleParameter(data_dict["analytics"], "quantiles", "parametricVar", params.setVarQuantiles)
        self.setSimpleParameter(data_dict["analytics"], "breakdown", "parametricVar", params.setVarBreakDown)
        self.setSimpleParameter(data_dict["analytics"], "portfolioFilter", "parametricVar", params.setVarBreakDown)
        self.setSimpleParameter(data_dict["analytics"], "method", "parametricVar", params.setVarBreakDown)
        self.setSimpleParameter(data_dict["analytics"], "mcSamples", "parametricVar", params.setVarBreakDown, int)
        self.setSimpleParameter(data_dict["analytics"], "mcSeed", "parametricVar", params.setVarBreakDown, int)
        # TODO: create param for Covariance and Sensitivity (754)

    def setupSimm(self, data_dict, params):
        print("Loading simm variables")
        params.insertAnalytic("SIMM")
        # TODO: set params for version, crif, and currencys (775)

    def setupXVA(self, data_dict, params):
        print("Loading XVA variables")
        params.insertAnalytic("XVA")
        self.setSimpleParameter(data_dict["analytics"], "baseCurrency", "xva", params.setXvaBaseCurrency)
        self.setUriParameter(data_dict["analytics"], "csaUri", "xva", params.setNettingSetManager)
        if params.setLoadCube(True) and "nettingSetCubeUri" in data_dict["analytics"]["xva"]:
            self.setUriParameter(data_dict["analytics"], "nettingSetCubeUri", "xva", params.setNettingSetManager)
        self.setSimpleParameter(data_dict["analytics"], "flipViewXVA", "xva", params.setFlipViewXVA, bool)
        self.setSimpleParameter(data_dict["analytics"], "fullInitialCollateralisation", "xva", params.setFullInitialCollateralisation,
                           bool)


        self.setSimpleParameter(data_dict["analytics"], "exposureProfilesByTrade", "xva", params.setExposureProfilesByTrade, bool)
        self.setSimpleParameter(data_dict["analytics"], "exposureProfiles", "xva", params.setExposureProfiles, bool)
        self.setSimpleParameter(data_dict["analytics"], "quantile", "xva", params.setPfeQuantile, float)
        self.setSimpleParameter(data_dict["analytics"], "calculationType", "xva", params.setCollateralCalculationType, str)
        self.setSimpleParameter(data_dict["analytics"], "allocationMethod", "xva", params.setExposureAllocationMethod, str)
        self.setSimpleParameter(data_dict["analytics"], "marginalAllocationLimit", "xva", params.setMarginalAllocationLimit, float)
        self.setSimpleParameter(data_dict["analytics"], "exerciseNextBreak", "xva", params.setExerciseNextBreak, bool)
        self.setSimpleParameter(data_dict["analytics"], "cva", "xva", params.setCvaAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "dva", "xva", params.setDvaAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "fva", "xva", params.setFvaAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "colva", "xva", params.setColvaAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "collateralFloor", "xva", params.setCollateralFloorAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "dim", "xva", params.setDimAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "mva", "xva", params.setMvaAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "kva", "xva", params.setKvaAnalytic, bool)
        self.setSimpleParameter(data_dict["analytics"], "dynamicCredit", "xva", params.setDynamicCredit, bool)
        self.setSimpleParameter(data_dict["analytics"], "cvaSensi", "xva", params.setCvaSensi, bool)
        self.setSimpleParameter(data_dict["analytics"], "cvaSensiGrid", "xva", params.setCvaSensiGrid, str)
        self.setSimpleParameter(data_dict["analytics"], "cvaSensiShiftSize", "xva", params.setCvaSensiShiftSize, float)
        self.setSimpleParameter(data_dict["analytics"], "dvaName", "xva", params.setDvaName, str)
        self.setUriParameter(data_dict["analytics"], "rawCubeOutputUri", "xva", params.setRawCubeOutput)
        self.setUriParameter(data_dict["analytics"], "netCubeOutputUri", "xva", params.setNetCubeOutput)

        # FVA variables
        self.setSimpleParameter(data_dict["analytics"], "fvaBorrowingCurve", "xva", params.setFvaBorrowingCurve)
        self.setSimpleParameter(data_dict["analytics"], "fvaLendingCurve", "xva", params.setFvaLendingCurve)
        self.setSimpleParameter(data_dict["analytics"], "flipViewBorrowingCurvePostfix", "xva",
                                params.setFlipViewBorrowingCurvePostfix)
        self.setSimpleParameter(data_dict["analytics"], "flipViewLendingCurvePostfix", "xva", params.setFlipViewLendingCurvePostfix)


        # DIM variables
        #  Fill Time series param
        self.setUriParameter(data_dict["analytics"], "deterministicInitialMarginUri", "xva", params.setDeterministicInitialMargin)
        self.setSimpleParameter(data_dict["analytics"], "dimQuantile", "xva", params.setDimQuantile, float)
        self.setSimpleParameter(data_dict["analytics"], "dimHorizonCalendarDays", "xva", params.setDimHorizonCalendarDays, int)
        self.setSimpleParameter(data_dict["analytics"], "dimRegressionOrder", "xva", params.setDimRegressionOrder, int)
        self.setSimpleParameter(data_dict["analytics"], "dimRegressors", "xva", params.setDimRegressors, str)
        self.setSimpleParameter(data_dict["analytics"], "dimOutputGridPoints", "xva", params.setDimOutputGridPoints, str)
        self.setSimpleParameter(data_dict["analytics"], "dimOutputNettingSet", "xva", params.setDimOutputNettingSet, str)
        self.setSimpleParameter(data_dict["analytics"], "dimLocalRegressionEvaluations", "xva", params.setDimLocalRegressionEvaluations,
                           int)
        self.setSimpleParameter(data_dict["analytics"], "dimLocalRegressionBandwidth", "xva", params.setDimLocalRegressionBandwidth,
                           float)

        # KVA Variables
        self.setSimpleParameter(data_dict["analytics"], "kvaCapitalDiscountRate", "xva", params.setKvaCapitalDiscountRate, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaAlpha", "xva", params.setKvaAlpha, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaRegAdjustment", "xva", params.setKvaRegAdjustment, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaCapitalHurdle", "xva", params.setKvaCapitalHurdle, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaOurPdFloor", "xva", params.setKvaOurPdFloor, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaTheirPdFloor", "xva", params.setKvaTheirPdFloor, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaOurCvaRiskWeight", "xva", params.setKvaOurCvaRiskWeight, float)
        self.setSimpleParameter(data_dict["analytics"], "kvaTheirCvaRiskWeight", "xva", params.setKvaTheirCvaRiskWeight, float)

        
        # Credit Simulation Variables
        # TODO: set credit simulation parameters once added to swig (1167)
        '''
        if "creditMigration" in data_dict["xva"]:
            params.setCreditMigrationAnalytic(bool(data_dict["xva"]["creditMigration"]))
    
        if "creditMigrationDistributionGrid" in data_dict["xva"]:
            params.setCreditMigrationDistributionGrid(data_dict["xva"]["creditMigrationDistributionGrid"])
    
        if "creditMigrationTimeSteps" in data_dict["xva"]:
            params.setCreditMigrationTimeSteps(data_dict["xva"]["creditMigrationTimeSteps"])
        
        if "creditMigrationConfigUri" in data_dict["xva"]:
            # Fetch xml url from the json
            creditMigrationConfigUri = data_dict["xva"]["creditMigrationConfigUri"]

            # Decode the URL
            decoded_url = urllib.parse.unquote(creditMigrationConfigUri)

            # Fetch the XML data
            response = urllib.request.urlopen(decoded_url)
            xml_data = response.read().decode('utf-8')
            # TODO: set this parameter once added: params.setCreditSimulationParameters(xml_data)
        '''
    
    def setupExposure(self, data_dict, params):
        print("Loading Simulation variables")
        params.insertAnalytic("EXPOSURE")
        self.setUriParameter(data_dict["analytics"], "simulationConfigUri", "simulation", params.setExposureSimMarketParams)
        self.setUriParameter(data_dict["analytics"], "simulationConfigUri", "simulation", params.setCrossAssetModelData)
        self.setUriParameter(data_dict["analytics"], "simulationConfigUri", "simulation", params.setScenarioGeneratorData)
        if data_dict["analytics"]["xva"]["active"] != "Y":
            params.setLoadCube(True)  # might need to reset cube here
        response = urllib.request.urlopen(data_dict["setup"]["pricingEnginesUri"])
        xml_data = response.read().decode('utf-8')
        params.setSimulationPricingEngine(xml_data)
        self.setSimpleParameter(data_dict["analytics"], "amc", "simulation", params.setAmc, bool)
        self.setSimpleParameter(data_dict["analytics"], "amcTradeTypes", "simulation", params.setAmc)
        self.setUriParameter(data_dict["analytics"], "pricingEnginesUri", "simulation", params.setSimulationPricingEngine)
        self.setUriParameter(data_dict["analytics"], "amcPricingEnginesUri", "simulation", params.setAmcPricingEngine)
        self.setSimpleParameter(data_dict["analytics"], "baseCurrency", "simulation", params.setExposureBaseCurrency)
        if "observationModel" in data_dict["analytics"]["simulation"]:
            params.setExposureObservationModel(data_dict["analytics"]["simulation"]["observationModel"])
        else:
            params.setExposureObservationModel(data_dict["setup"]["observationModel"])
        if "storeFlows" in data_dict["analytics"]["simulation"]:
            if "Y" in data_dict["analytics"]["simulation"]["storeFlows"]:
                params.setStoreFlows(True)
        if "storeSurvivalProbabilities" in data_dict["analytics"]["simulation"]:
            if "Y" in data_dict["analytics"]["simulation"]["storeSurvivalProbabilities"]:
                params.setStoreSurvivalProbabilities(True)
        self.setSimpleParameter(data_dict["analytics"], "nettingSetId", "simulation", params.setNettingSetId)
        if "cubeFile" in data_dict["analytics"]["simulation"]:
            params.setWriteCube(True)
        if "scenariodump" in data_dict["analytics"]["simulation"]:
            params.setWriteScenarios(True)
        

    def buildInputParameters(self, data_dict):

        # Set params from ORE
        params = ore.InputParameters()

        # Load in variables
        print("Loading parameters...")

        # switch default for backward compatibility
        params.setEntireMarket(True)
        params.setAllFixings(True)
        params.setEomInflationFixings(False)
        params.setUseMarketDataFixings(False)
        params.setBuildFailedTrades(False)

        # TODO: Calendar assignments need to be set when they are added to ORE Swig

        # Setup Variables
        print("Loading Setup variables")
        if("setup" not in data_dict):
            error_message = "parameter group 'setup' missing"
            print("Error message: ", error_message)
            response = make_response(f"error: {error_message}")
            response.status_code = 404
            return response
        
        if "asofDate" in data_dict["setup"]:
            asofDate = data_dict["setup"]["asofDate"]
            params.setAsOfDate(asofDate)
            # Find Eval Date
            self.findEvalDate(asofDate)

        # Set inputPath name
        inputPath = self.setInputName(data_dict,"inputPath","setup")

        # Set Output Path
        self.setSimpleParameter(data_dict, "outputPath", "setup", params.setResultsPath, str)

        # Set Market Data Fixings
        self.setSimpleParameter(data_dict, "useMarketDataFixings", "setup", params.setUseMarketDataFixings, bool)

        # Set Dry Run
        self.setSimpleParameter(data_dict, "dryRun", "setup", params.setDryRun, bool)

        # Set reportNaString
        self.setSimpleParameter(data_dict, "reportNaString", "setup", params.setReportNaString)

        # Set eomInflationFixings
        self.setSimpleParameter(data_dict, "eomInflationFixings", "setup", params.setEomInflationFixings, bool)

        # Set nThreads
        self.setSimpleParameter(data_dict, "nThreads", "setup", params.setThreads, int)

        # Set entireMarket
        self.setSimpleParameter(data_dict, "entireMarket", "setup", params.setEntireMarket, bool)

        # Set iborFallbackOverride
        self.setSimpleParameter(data_dict, "iborFallbackOverride", "setup", params.setIborFallbackOverride, bool)

        # Set continueOnError
        self.setSimpleParameter(data_dict, "continueOnError", "setup", params.setContinueOnError, bool)

        # Set lazyMarket
        self.setSimpleParameter(data_dict, "lazyMarketBuilding", "setup", params.setLazyMarketBuilding)

        # Set build Trades
        self.setSimpleParameter(data_dict, "buildFailedTrades", "setup", params.setBuildFailedTrades, bool)

        # Set implyTodaysFixings
        self.setSimpleParameter(data_dict, "implyTodaysFixings", "setup", params.setImplyTodaysFixings, bool)

        # Set referenceData
        try:
            self.setUriParameter(data_dict, "referenceDataUri", "setup", params.setRefDataManager)
        except Exception as e:
            error_type = type(e).__name__
            error_message = str(e)
            response = make_response({'error_type': error_type, 'error_message': error_message})
            response.status_code = 500
            return response

        # Set iborFallbackConfig
        self.setUriParameter(data_dict, "iborFallbackConfigUri", "setup", params.setIborFallbackConfig)

        # Set CurveConfigs
        try:
            self.setUriParameter(data_dict, "curveConfigUri", "setup", params.setCurveConfigs)

        except Exception as e:
            error_message = "Curve Config not found"
            print("Exception occurred:", str(e))
            print("Error message:", error_message)
            response = make_response(f"error: {str(e)}")
            response.status_code = 404
            return response

        # Set conventions
        self.setUriParameter(data_dict, "conventionsUri", "setup", params.setConventions)

        # Set marketConfig
        self.setUriParameter(data_dict, "marketConfigUri", "setup", params.setTodaysMarketParams)

        # Set pricingEngines
        self.setUriParameter(data_dict, "pricingEnginesUri", "setup", params.setPricingEngine)

        # Set portfolio
        self.setUriParameter(data_dict, "portfolioUri", "setup", params.setPortfolio)

        # Set observationModel
        self.setSimpleParameter(data_dict, "observationModel", "setup", params.setObservationModel)

        print("Loaded Setup Variables")
        # Markets variables
        if "markets" in data_dict:
            print("Loading Market variables")
            pricing = data_dict["markets"]
            marketMap = ore.StringStringMap(pricing)
            # Load all market parameters into a map
            for m, k in marketMap.items():
                params.setMarketConfig(k, m)

        # Insert analytics
        for requestedAnalytic, value in data_dict["analytics"].items():
            if data_dict["analytics"][requestedAnalytic]["active"] == "Y":
                self.setupAnalytics(data_dict, params, str(requestedAnalytic))
            elif requestedAnalytic == "curves":
                params.setOutputCurves(False)
            else:
                print("Requested analytic ", requestedAnalytic, " not active")

        print("Done with analytics")
        # TODO: CPTY and Market cube not needed for any of the runs (966)

        # cashflow npv and dynamic backtesting
        self.setSimpleParameter(data_dict, "cashFlowHorizon", "cashflow", params.setCashflowHorizon)
        self.setSimpleParameter(data_dict, "portfolioFilterDate", "cashflow", params.setPortfolioFilterDate)

        # TODO: Check if Analytics is empty
        ''' 
        if len(params.analytics()) == 0:
            params.insertAnalytic("MARKETDATA")
            params.setOutputTodaysMarketCalibration(True)
            if params.lazyMarketBuilding(True):
                params.setLazyMarketBuilding(False)
        '''
        print("buildInputParameters done")

        return params


    def runORE(self):

        # Retrieve the data_dict from the request
        data_dict = self.get_data()
        
        # Set logFile name
        logFile = self.setInputName(data_dict, "logFile", "setup")

        # set up input parameters
        inputParams = self.buildInputParameters(data_dict)

        # load market data to run the app
        market_data = data_dict["setup"]["marketData"]
        response = requests.get(market_data)
        market_data = response.text
        # Split by new line
        market_data = market_data.splitlines()

        # load market fixings data to run the app
        fixings_data = data_dict["setup"]["fixingData"]
        response = requests.get(fixings_data)
        fixings_data = response.text
        # Split by new line
        fixings_data = fixings_data.splitlines()

        print("Retrieved fixings data")

        print("Creating OREApp...")
        ore_app = ore.OREApp(inputParams, logFile, 63, True)

        print("Running ORE process...")
        try:
            ore_app.run(ore.StrVector(market_data), ore.StrVector(fixings_data))  # try catch error needed (500 error)
        except Exception as e:
            error_message = "Internal server error from ore app failing"
            error = str(e.args[0])
            response = make_response(f"error: {error_message} :  {error}")
            response.status_code = 500
            return response

        # find local host to send to based on the json
        resultsUri = self.setInputName(data_dict, "resultsUri", "setup")

        # Post the Reports to the correct Results path
        self.postCSV(resultsUri, ore_app)

        print("Run time: %.6f sec" % ore_app.getRunTime())

        print("ORE process done")

        return True

