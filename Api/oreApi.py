import json
import urllib.request
import urllib.parse
import ORE as ql
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

    def postCSV(self, resultsUri, ore):
        # Get Names for Reports output
        outputs = list(ore.getReportNames())
        for outputName in outputs:
            # Set Report for each Report Name
            report = ore.getReport(outputName)
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
        evalDate = ql.Date(day, month, year)
        ql.Settings.instance().evaluationDate = evalDate

    def setSimpleParameter(self, data_dict, lookupname, lookuplocation, param=None, paramtype=None):
        if lookupname in data_dict[lookuplocation]:
            lookup_value = data_dict[lookuplocation][lookupname]
            # Set Parameter and check for types
            if param is not None:
                if paramtype is not None:
                    param(paramtype(lookup_value))
                else:
                    param(lookup_value)

    def setSimpleAnalyticParameter(self, data_dict, lookupname, lookuplocation, param=None, paramtype=None):
        if lookupname in data_dict["analytics"][lookuplocation]:
            lookup_value = data_dict["analytics"][lookuplocation][lookupname]
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

    def setAnalyticUriParameter(self, data_dict, lookupname, lookuplocation, param):
        if lookupname in data_dict["analytics"][lookuplocation]:
            # Fetch xml url from the json
            lookup_value = data_dict["analytics"][lookuplocation][lookupname]
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

    def buildInputParameters(self):

        # Retrieve the data_dict from the request
        data_dict = self.get_data()

        # Set params from ORE
        params = ql.InputParameters()

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
        if "asofDate" in data_dict["setup"]:
            asofDate = data_dict["setup"]["asofDate"]
            params.setAsOfDate(asofDate)

            # Find Eval Date
            self.findEvalDate(asofDate)

        # Set inputPath name
        inputPath = self.setInputName(data_dict,"inputPath","setup")

        # Set Output Path
        self.setSimpleParameter(data_dict, "outputPath", "setup", params.setResultsPath, str)

        # Set logFile name
        logFile = self.setInputName(data_dict, "logFile", "setup")

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

        # Markets variables
        if "markets" in data_dict:
            print("Loading Market variables")
            pricing = data_dict["markets"]
            marketMap = ql.StringStringMap(pricing)
            # Load all market parameters into a map
            for m, k in marketMap.items():
                params.setMarketConfig(k, m)

        # Analytics npv variables
        if "Y" in data_dict["analytics"]["npv"]["active"]:
            print("Loading analytics/npv variables")
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
        else:
            print("npv analytic is not requested")

        # Analytics cashflow variables
        if "Y" in data_dict["analytics"]["cashflow"]["active"]:
            print("Loading analytics/cashflow variables")
            params.insertAnalytic("CASHFLOW")
        else:
            print("cashflow analytic is not requested")
        if "includePastCashflows" in data_dict["analytics"]["cashflow"]:
            params.setIncludePastCashflows(bool(data_dict["analytics"]["cashflow"]["includePastCashflows"]))
        else:
            params.setIncludePastCashflows(False)
            
        # Curve variables
        if "active" in data_dict["analytics"]["curves"]:
            print("Loading analytics/curves variables")
            params.setOutputCurves(True)
        else:
            print("curve output is not requested")
            params.setOutputCurves(False)

        self.setSimpleAnalyticParameter(data_dict, "grid", "curves", params.setCurvesGrid)
        self.setSimpleAnalyticParameter(data_dict, "configuration", "curves", params.setCurvesMarketConfig)
        self.setSimpleAnalyticParameter(data_dict, "outputTodaysMarketCalibration", "curves",
                                params.setOutputTodaysMarketCalibration, bool)

        # Sensitivity variables
        if "Y" in data_dict["analytics"]["sensitivity"]["active"]:
            print("Loading analytics/sensitivity variables")
            params.insertAnalytic("SENSITIVITY")
            self.setSimpleAnalyticParameter(data_dict, "paraSensitivity", "sensitivity", params.setParSensi, bool)
            self.setSimpleAnalyticParameter(data_dict, "outputJacobi", "sensitivity", params.setOutputJacobi, bool)
            self.setSimpleAnalyticParameter(data_dict, "alignPillars", "sensitivity", params.setAlignPillars, bool)
            self.setAnalyticUriParameter(data_dict, "marketConfigUri", "sensitivity", params.setSensiSimMarketParams)
            self.setAnalyticUriParameter(data_dict, "sensitivityConfigUri", "sensitivity", params.setSensiScenarioData)
            self.setAnalyticUriParameter(data_dict, "pricingEnginesUri", "sensitivity", params.setSensiPricingEngine)
            self.setSimpleAnalyticParameter(data_dict, "outputSensitivityThreshold", "sensitivity", params.setSensiThreshold)
        else:
            print("sensitivity analytic is not requested")
            
        # Stress variables
        if "Y" in data_dict["analytics"]["stress"]["active"]:
            print("Loading analytics/stress variables")
            params.insertAnalytic("STRESS")
            self.setUriParameter(data_dict, "pricingEnginesUri", "setup", params.setStressPricingEngine)
            self.setAnalyticUriParameter(data_dict, "marketConfigUri", "stress", params.setStressSimMarketParams)
            self.setAnalyticUriParameter(data_dict, "stressConfigUri", "stress", params.setStressScenarioData)
            self.setSimpleAnalyticParameter(data_dict, "outputSensitivityThreshold", "stress", params.setStressThreshold)
        else:
            print("stress analytic is not requested")

        # parametricVar variables
        if "Y" in data_dict["analytics"]["parametricVar"]["active"]:
            print("Loading analytics/parametricVar variables")
            params.insertAnalytic("VAR")
            self.setSimpleAnalyticParameter(data_dict, "salvageCovarianceMatrix", "parametricVar", params.setSalvageCovariance,
                                    bool)
            self.setSimpleAnalyticParameter(data_dict, "quantiles", "parametricVar", params.setVarQuantiles)
            self.setSimpleAnalyticParameter(data_dict, "breakdown", "parametricVar", params.setVarBreakDown)
            self.setSimpleAnalyticParameter(data_dict, "portfolioFilter", "parametricVar", params.setVarBreakDown)
            self.setSimpleAnalyticParameter(data_dict, "method", "parametricVar", params.setVarBreakDown)
            self.setSimpleAnalyticParameter(data_dict, "mcSamples", "parametricVar", params.setVarBreakDown, int)
            self.setSimpleAnalyticParameter(data_dict, "mcSeed", "parametricVar", params.setVarBreakDown, int)
            # TODO: create param for Covariance and Sensitivity (754)
        else:
            print("parametricVar analytic is not requested")

        if "Y" in data_dict["analytics"]["simm"]["active"]:
            print("Loading analytics/simm variables")
            params.insertAnalytic("SIMM")
            # TODO: set params for version, crif, and currencys (775)
        else:
            print("simm analytic is not requested")
            
        if "Y" in data_dict["analytics"]["simulation"]["active"]:
            print("Loading analytics/simulation variables")
            params.insertAnalytic("EXPOSURE")

            self.setSimpleAnalyticParameter(data_dict, "salvageCorrelationMatrix", "simulation", params.setSalvageCovariance, bool)
            self.setSimpleAnalyticParameter(data_dict, "amc", "simulation", params.setAmc, bool)
            self.setSimpleAnalyticParameter(data_dict, "amcTradeTypes", "simulation", params.setAmc)
            self.setAnalyticUriParameter(data_dict, "simulationConfigUri", "simulation", params.setExposureSimMarketParams)
            self.setAnalyticUriParameter(data_dict, "simulationConfigUri", "simulation", params.setCrossAssetModelData)
            self.setAnalyticUriParameter(data_dict, "simulationConfigUri", "simulation", params.setScenarioGeneratorData)
            self.setAnalyticUriParameter(data_dict, "pricingEnginesUri", "simulation", params.setSimulationPricingEngine)
            self.setAnalyticUriParameter(data_dict, "amcPricingEnginesUri", "simulation", params.setAmcPricingEngine)
            self.setSimpleAnalyticParameter(data_dict, "baseCurrency", "simulation", params.setExposureBaseCurrency)
            if "observationModel" in data_dict["analytics"]["simulation"]:
                params.setExposureObservationModel(data_dict["analytics"]["simulation"]["observationModel"])
            else:
                params.setExposureObservationModel(data_dict["setup"]["observationModel"])
                if "Y" in data_dict["analytics"]["simulation"]["storeFlows"]:
                    params.setStoreFlows(True)
                if "Y" in data_dict["analytics"]["simulation"]["storeSurvivalProbabilities"]:
                    params.setStoreSurvivalProbabilities(True)
                self.setSimpleAnalyticParameter(data_dict, "nettingSetId", "simulation", params.setNettingSetId)
                if "cubeUri" in data_dict["analytics"]["simulation"]:
                    params.setWriteCube(True)
                if "scenariodump" in data_dict["analytics"]["simulation"]:
                    params.setWriteScenarios(True)
        else:
            print("simulation analytic is not requested")

        if "Y" in data_dict["analytics"]["xva"]["active"]:
            print("Loading analytics/xva variables")
            params.insertAnalytic("XVA")

            # XVA specifically
            self.setSimpleAnalyticParameter(data_dict, "baseCurrency", "xva", params.setXvaBaseCurrency)
            if "Y" in data_dict["analytics"]["simulation"]["active"] and "Y" not in data_dict["analytics"]["xva"]["active"]:
                params.setLoadCube(True)  # might need to reset cube here
            if "Y" in data_dict["analytics"]["simulation"]["active"] or "Y" in data_dict["analytics"]["xva"]["active"]:
                self.setAnalyticUriParameter(data_dict, "csaUri", "xva", params.setNettingSetManager)
            if params.setLoadCube(True) and "nettingSetCubeUri" in data_dict["analytics"]["xva"]:
                self.setUriParameter(data_dict, "nettingSetCubeUri", "xva", params.setNettingSetManager)
            # TODO: CPTY and Market cube not needed for any of the runs (966)
            # Set Xva params
            self.setSimpleAnalyticParameter(data_dict, "flipViewXVA", "xva", params.setFlipViewXVA, bool)
            self.setSimpleAnalyticParameter(data_dict, "fullInitialCollateralisation", "xva", params.setFullInitialCollateralisation, bool)
            self.setSimpleAnalyticParameter(data_dict, "exposureProfilesByTrade", "xva", params.setExposureProfilesByTrade, bool)
            self.setSimpleAnalyticParameter(data_dict, "exposureProfiles", "xva", params.setExposureProfiles, bool)
            self.setSimpleAnalyticParameter(data_dict, "quantile", "xva", params.setPfeQuantile, float)
            self.setSimpleAnalyticParameter(data_dict, "calculationType", "xva", params.setCollateralCalculationType, str)
            self.setSimpleAnalyticParameter(data_dict, "allocationMethod", "xva", params.setExposureAllocationMethod, str)
            self.setSimpleAnalyticParameter(data_dict, "marginalAllocationLimit", "xva", params.setMarginalAllocationLimit, float)
            self.setSimpleAnalyticParameter(data_dict, "exerciseNextBreak", "xva", params.setExerciseNextBreak, bool)
            self.setSimpleAnalyticParameter(data_dict, "cva", "xva", params.setCvaAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "dva", "xva", params.setDvaAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "fva", "xva", params.setFvaAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "colva", "xva", params.setColvaAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "collateralFloor", "xva", params.setCollateralFloorAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "dim", "xva", params.setDimAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "mva", "xva", params.setMvaAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "kva", "xva", params.setKvaAnalytic, bool)
            self.setSimpleAnalyticParameter(data_dict, "dynamicCredit", "xva", params.setDynamicCredit, bool)
            self.setSimpleAnalyticParameter(data_dict, "cvaSensi", "xva", params.setCvaSensi, bool)
            self.setSimpleAnalyticParameter(data_dict, "cvaSensiGrid", "xva", params.setCvaSensiGrid, str)
            self.setSimpleAnalyticParameter(data_dict, "cvaSensiShiftSize", "xva", params.setCvaSensiShiftSize, float)
            self.setSimpleAnalyticParameter(data_dict, "dvaName", "xva", params.setDvaName, str)
            self.setAnalyticUriParameter(data_dict, "rawCubeOutputUri", "xva", params.setRawCubeOutput)
            self.setAnalyticUriParameter(data_dict, "netCubeOutputUri", "xva", params.setNetCubeOutput)

            # FVA variables
            self.setSimpleAnalyticParameter(data_dict, "fvaBorrowingCurve", "xva", params.setFvaBorrowingCurve)
            self.setSimpleAnalyticParameter(data_dict, "fvaLendingCurve", "xva", params.setFvaLendingCurve)
            self.setSimpleAnalyticParameter(data_dict, "flipViewBorrowingCurvePostfix", "xva",
                                            params.setFlipViewBorrowingCurvePostfix)
            self.setSimpleAnalyticParameter(data_dict, "flipViewLendingCurvePostfix", "xva", params.setFlipViewLendingCurvePostfix)

            # DIM variables
            #  Fill Time series param
            self.setAnalyticUriParameter(data_dict, "deterministicInitialMarginUri", "xva", params.setDeterministicInitialMargin)
            self.setSimpleAnalyticParameter(data_dict, "dimQuantile", "xva", params.setDimQuantile, float)
            self.setSimpleAnalyticParameter(data_dict, "dimHorizonCalendarDays", "xva", params.setDimHorizonCalendarDays, int)
            self.setSimpleAnalyticParameter(data_dict, "dimRegressionOrder", "xva", params.setDimRegressionOrder, int)
            self.setSimpleAnalyticParameter(data_dict, "dimRegressors", "xva", params.setDimRegressors, str)
            self.setSimpleAnalyticParameter(data_dict, "dimOutputGridPoints", "xva", params.setDimOutputGridPoints, str)
            self.setSimpleAnalyticParameter(data_dict, "dimOutputNettingSet", "xva", params.setDimOutputNettingSet, str)
            self.setSimpleAnalyticParameter(data_dict, "dimLocalRegressionEvaluations", "xva", params.setDimLocalRegressionEvaluations, int)
            self.setSimpleAnalyticParameter(data_dict, "dimLocalRegressionBandwidth", "xva", params.setDimLocalRegressionBandwidth, float)

            # KVA Variables
            self.setSimpleAnalyticParameter(data_dict, "kvaCapitalDiscountRate", "xva", params.setKvaCapitalDiscountRate, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaAlpha", "xva", params.setKvaAlpha, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaRegAdjustment", "xva", params.setKvaRegAdjustment, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaCapitalHurdle", "xva", params.setKvaCapitalHurdle, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaOurPdFloor", "xva", params.setKvaOurPdFloor, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaTheirPdFloor", "xva", params.setKvaTheirPdFloor, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaOurCvaRiskWeight", "xva", params.setKvaOurCvaRiskWeight, float)
            self.setSimpleAnalyticParameter(data_dict, "kvaTheirCvaRiskWeight", "xva", params.setKvaTheirCvaRiskWeight, float)
            
            # Credit Simulation Variables
            # TODO: set credit simulation parameters once added to swig (1167)
            '''
            if "creditMigration" in data_dict["analytics"]["xva"]:
                params.setCreditMigrationAnalytic(bool(data_dict["xva"]["creditMigration"]))
    
            if "creditMigrationDistributionGrid" in data_dict["analytics"]["xva"]:
                params.setCreditMigrationDistributionGrid(data_dict["xva"]["creditMigrationDistributionGrid"])
    
            if "creditMigrationTimeSteps" in data_dict["analytics"]["xva"]:
                params.setCreditMigrationTimeSteps(data_dict["xva"]["creditMigrationTimeSteps"])
        
            if "creditMigrationConfigUri" in data_dict["analytics"]["xva"]:
                # Fetch xml url from the json
                creditMigrationConfigUri = data_dict["analytics"]["xva"]["creditMigrationConfigUri"]

                # Decode the URL
                decoded_url = urllib.parse.unquote(creditMigrationConfigUri)

                # Fetch the XML data
                response = urllib.request.urlopen(decoded_url)
                xml_data = response.read().decode('utf-8')
                # TODO: set this parameter once added: params.setCreditSimulationParameters(xml_data)
            '''

        else:
            print("xva analytic is not requested")

        self.setSimpleParameter(data_dict, "observationModel", "setup", params.setExposureObservationModel)
        response = urllib.request.urlopen(data_dict["setup"]["pricingEnginesUri"])
        xml_data = response.read().decode('utf-8')
        params.setSimulationPricingEngine(xml_data)
        
        # cashflow npv and dynamic backtesting
        self.setSimpleAnalyticParameter(data_dict, "cashFlowHorizon", "cashflow", params.setCashflowHorizon)
        self.setSimpleAnalyticParameter(data_dict, "portfolioFilterDate", "cashflow", params.setPortfolioFilterDate)

        # TODO: Check if Analytics is empty
        ''' 
        if len(params.analytics()) == 0:
            params.insertAnalytic("MARKETDATA")
            params.setOutputTodaysMarketCalibration(True)
            if params.lazyMarketBuilding(True):
                params.setLazyMarketBuilding(False)
        '''

        print("Creating OREApp...")
        ore = ql.OREApp(params, logFile, 31, True)

        # load market data to run the app
        market_data = data_dict["setup"]["marketData"]
        response = requests.get(market_data)
        market_data = response.text
        # Split by new line
        market_data = market_data.splitlines()

        # load market fixings data to run the app
        fixings_data = data_dict["setup"]["fixingsData"]
        response = requests.get(fixings_data)
        fixings_data = response.text
        # Split by new line
        fixings_data = fixings_data.splitlines()

        print("Running ORE process...")
        try:
            ore.run(ql.StrVector(market_data), ql.StrVector(fixings_data))  # try catch error needed (500 error)
        except Exception as e:
            error_message = "Internal server error from ore app failing"
            response = make_response(f"error: {error_message}")
            response.status_code = 500
            return response

        # find local host to send to based on the json
        resultsUri = self.setInputName(data_dict, "resultsUri", "setup")

        # Post the Reports to the correct Results path
        self.postCSV(resultsUri, ore)

        print("Run time: %.6f sec" % ore.getRunTime())

        print("ORE process done")

        return True


