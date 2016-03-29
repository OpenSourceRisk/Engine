/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2011 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <iostream>

#include <qlw/utilities/log.hpp>
#include <qlw/wrap.hpp>
#include <qlw/engine/valuationengine.hpp>
#include <ql/time/calendars/all.hpp>
#include <boost/timer.hpp>

#include "ore.hpp"
#include "timebomb.hpp"

using namespace std;
using namespace openxva::model;
using namespace openxva::scenario;
using namespace openxva::portfolio;
using namespace openxva::marketdata;
using namespace openxva::utilities;
using namespace openxva::engine;
using namespace openxva::simulation;
using namespace openxva::cube;
using namespace openxva::aggregation;

void writeNpv(const Parameters& params,
              boost::shared_ptr<Market> market,
              boost::shared_ptr<Portfolio> portfolio);

void writeCashflow(const Parameters& params,
                   boost::shared_ptr<Market> market,
                   boost::shared_ptr<Portfolio> portfolio);

void writeCurves(const Parameters& params,
                 const TodaysMarketParameters& marketConfig,
                 const boost::shared_ptr<Market>& market);

void writeTradeExposures(const Parameters& params,
                         boost::shared_ptr<PostProcess> postProcess);

void writeNettingSetExposures(const Parameters& params,
                              boost::shared_ptr<PostProcess> postProcess);

void writeCva(const Parameters& params,
              boost::shared_ptr<Portfolio> portfolio,
              boost::shared_ptr<PostProcess> postProcess);

int main(int argc, char** argv) {

    ORE_CHECK_TIMEBOMB
    
    boost::timer timer;
    
    try {
        std::cout << "ORE starting" << std::endl;

        Size tab = 40;
        
        if (argc != 2) {
            std::cout << endl << "usage: ORE path/to/ore.xml" << endl << endl;
            return -1;
        }
    
        string inputFile(argv[1]);
        Parameters params;
        params.fromFile(inputFile);
        
        string outputPath = params.get("setup", "outputPath"); 
        string logFile =  outputPath + "/" + params.get("setup", "logFile");
        
        Log::instance().registerLogger(boost::make_shared<FileLogger>(logFile));
        Log::instance().switchOn();
        
        LOG("ORE starting");
        params.log();

        string asofString = params.get("setup", "asofDate");
        Date asof = parseDate(asofString);
        Settings::instance().evaluationDate() = asof; 
        
        /*******************************
         * Market and fixing data loader
         */
        cout << setw(tab) << left << "Market data loader... "; fflush(stdout); 
        string inputPath = params.get("setup", "inputPath"); 
        string marketFile = inputPath + "/" + params.get("setup", "marketDataFile");
        string fixingFile = inputPath + "/" + params.get("setup", "fixingDataFile");
        CSVLoader loader(marketFile, fixingFile);
        cout << "OK" << endl;
        
        /*************
         * Conventions
         */
        cout << setw(tab) << left << "Conventions... "; fflush(stdout); 
        Conventions conventions;
        string conventionsFile = inputPath + "/" + params.get("setup", "conventionsFile");
        conventions.fromFile(conventionsFile);
        cout << "OK" << endl;

        /**********************
         * Curve configurations
         */
        cout << setw(tab) << left << "Curve configuration... "; fflush(stdout);
        CurveConfigurations curveConfigs;
        string curveConfigFile = inputPath + "/" + params.get("setup", "curveConfigFile");
        curveConfigs.fromFile(curveConfigFile);
        cout << "OK" << endl;
                
        /********
         * Market
         */
        cout << setw(tab) << left << "Market... "; fflush(stdout);
        TodaysMarketParameters marketParameters;
        string marketConfigFile = inputPath + "/" + params.get("setup", "marketConfigFile");
        marketParameters.fromFile(marketConfigFile);
        
        boost::shared_ptr<Market> market = boost::make_shared<TodaysMarket>
            (asof, marketParameters, loader, curveConfigs, conventions);
        cout << "OK" << endl;

        /************************
         * Pricing Engine Factory
         */
        cout << setw(tab) << left << "Engine factory... "; fflush(stdout);
        boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
        string pricingEnginesFile = inputPath + "/" + params.get("setup", "pricingEnginesFile");
        engineData->fromFile(pricingEnginesFile);
        
        boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(market, engineData);
        cout << "OK" << endl;
        
        /******************************
         * Load and Build the Portfolio
         */
        cout << setw(tab) << left << "Portfolio... "; fflush(stdout);
        boost::shared_ptr<Portfolio> portfolio = boost::make_shared<Portfolio>();
        string portfolioFile = inputPath + "/" + params.get("setup", "portfolioFile");
        portfolio->load(portfolioFile);
        portfolio->build(factory);
        cout << "OK" << endl;
        
        /************
         * Curve dump
         */
        cout << setw(tab) << left << "Curve Report... "; fflush(stdout);
        if (params.hasGroup("curves") &&
            params.get("curves", "active") == "Y") {
            writeCurves(params, marketParameters, market);
            cout << "OK" << endl;
        }
        else {
            LOG("skip curve report");
            cout << "SKIP" << endl;
        }
        
        /*********************
         * Portfolio valuation
         */
        cout << setw(tab) << left << "NPV Report... "; fflush(stdout);
        if (params.hasGroup("npv") &&
            params.get("npv", "active") == "Y") {
            writeNpv(params, market, portfolio);
            cout << "OK" << endl;
        }
        else {
            LOG("skip portfolio valuation");
            cout << "SKIP" << endl;
        }
        
        /**********************
         * Cash flow generation
         */
        cout << setw(tab) << left << "Cashflow Report... "; fflush(stdout);
        if (params.hasGroup("cashflow") &&
            params.get("cashflow", "active") == "Y") {
            writeCashflow(params, market, portfolio);          
            cout << "OK" << endl;
        }
        else {
            LOG("skip cashflow generation");
            cout << "SKIP" << endl;
        }
        
        /******************************************
         * Simulation: Scenario and Cube Generation
         */

        if (params.hasGroup("simulation") &&
            params.get("simulation", "active") == "Y") {
            
            cout << setw(tab) << left << "Simulation Setup... "; 
            fflush(stdout);
            LOG("Build Simulation Model");
            string simulationModelFile = inputPath + "/" + params.get("simulation", "simulationModelFile");
            boost::shared_ptr<CrossAssetModelData> modelData = boost::make_shared<CrossAssetModelData>();
            modelData->fromFile(simulationModelFile);
            CrossAssetModelBuilder modelBuilder(market);
            boost::shared_ptr<QuantExt::CrossAssetModel> model = modelBuilder.build(modelData);
            
            LOG("Load Simulation Market Parameters");
            string simulationMarketFile = inputPath + "/" + params.get("simulation", "simulationMarketFile");
            boost::shared_ptr<ScenarioSimMarketParameters> simMarketData(new ScenarioSimMarketParameters);
            simMarketData->fromFile(simulationMarketFile);
            
            LOG("Load Simulation Parameters");
            string simulationParamFile = inputPath + "/" + params.get("simulation", "simulationParamFile");
            ScenarioGeneratorBuilder sb;
            sb.fromFile(simulationParamFile);
            boost::shared_ptr<ScenarioGenerator> sg = sb.build(model, simMarketData, asof);
            boost::shared_ptr<openxva::simulation::DateGrid> grid = sb.dateGrid();
            
            LOG("Build Simulation Maret");
            boost::shared_ptr<openxva::simulation::SimMarket> simMarket
                = boost::make_shared<ScenarioSimMarket>(sg, market, simMarketData);
            
            LOG("Build engine factory for pricing under scenarios, linked to sim market");
            boost::shared_ptr<EngineData> simEngineData = boost::make_shared<EngineData>();
            string simPricingEnginesFile = inputPath + "/" + params.get("simulation", "pricingEnginesFile");
            simEngineData->fromFile(simPricingEnginesFile);
            boost::shared_ptr<EngineFactory>
                simFactory = boost::make_shared<EngineFactory>(simMarket, simEngineData);
            
            LOG("Build portfolio linked to sim market");
            boost::shared_ptr<Portfolio> simPortfolio = boost::make_shared<Portfolio>();
            simPortfolio->load(portfolioFile);
            simPortfolio->build(simFactory);
            QL_REQUIRE(simPortfolio->size() == portfolio->size(),
                       "portfolio size mismatch, check simulation market setup");
            cout << "OK" << endl;
            
            LOG("Build valuation cube engine");
            Size samples = sb.samples();
            string baseCurrency = params.get("simulation", "baseCurrency");
            ValuationEngine engine(asof, grid, samples, baseCurrency, simMarket);
            Size xdim = simPortfolio->size();
            Size ydim = grid->dates().size();
            Size zdim = samples;

            ostringstream o;
            o << "Additional Scenario Data " << ydim << " x " << zdim << "... ";
            cout << setw(tab) << o.str(); fflush(stdout);
            boost::shared_ptr<AdditionalScenarioData>
                inMemoryAdditionalScenarioData = boost::make_shared<InMemoryAdditionalScenarioData>(ydim,zdim);
            cout << "OK" << endl;
            
            o.str("");
            o << "Build Cube " << xdim << " x " << ydim << " x " << zdim << "... ";
            cout << setw(tab) << o.str(); fflush(stdout);
            LOG("Build cube");
            boost::shared_ptr<Cube>
                inMemoryCube = boost::make_shared<SinglePrecisionInMemoryCube>(xdim,ydim,zdim);
            engine.buildCube(simPortfolio, inMemoryCube, inMemoryAdditionalScenarioData);
            cout << "OK" << endl;

            cout << setw(tab) << left << "Write Cube... "; fflush(stdout);
            LOG("Write cube");
            string cubeFileName = outputPath + "/" + params.get("simulation", "cubeFile");
            inMemoryCube->save(cubeFileName);
            cout << "OK" << endl;

            cout << setw(tab) << left << "Write Additional Scenario Data... "; fflush(stdout);
            LOG("Write scenario data");
            string outputFileNameAddScenData = outputPath + "/" + params.get("simulation", "additionalScenarioDataFileName");
            inMemoryAdditionalScenarioData->save(outputFileNameAddScenData);
            cout << "OK" << endl;
        }
        else {
            LOG("skip simulation");
            cout << setw(tab) << left << "Simulation... ";
            cout << "SKIP" << endl;
        }

        /*****************************
         * Aggregation and XVA Reports
         */
        cout << setw(tab) << left << "Aggregation and XVA Reports... "; fflush(stdout);
        if (params.hasGroup("xva") &&
            params.get("xva", "active") == "Y") {

            // We reset this here because the date grid building below depends on it.
            Settings::instance().evaluationDate() = asof; 

            string csaFile = inputPath + "/" + params.get("xva", "csaFile");
            boost::shared_ptr<NettingSetManager> netting = boost::make_shared<NettingSetManager>();
            netting->fromFile(csaFile);

            string cubeFile = outputPath + "/" + params.get("xva", "cubeFile");
            boost::shared_ptr<SinglePrecisionInMemoryCube>
                cube = boost::make_shared<SinglePrecisionInMemoryCube>();
            cube->load(cubeFile);

            string gridString = params.get("xva", "grid"); 
            boost::shared_ptr<DateGrid> grid = boost::make_shared<DateGrid>(gridString);

            QL_REQUIRE(cube->dimX() == portfolio->size(),
                       "cube x dimension does not match portfolio size");
            QL_REQUIRE(cube->dimY() == grid->size(),
                       "cube y dimension does not match date grid size");
                       
            string scenarioFile = outputPath + "/" + params.get("xva", "scenarioFile");
            boost::shared_ptr<AdditionalScenarioData>
                scenarioData = boost::make_shared<InMemoryAdditionalScenarioData>();
            scenarioData->load(scenarioFile);

            QL_REQUIRE(scenarioData->dimDates() == grid->size(),
                       "scenario dates do not match grid size");
            QL_REQUIRE(scenarioData->dimSamples() == cube->dimZ(),
                       "scenario sample size does not match cube sample size");
            
            map<string,bool> analytics;
            analytics["exposureProfiles"] = parseBool(params.get("xva", "exposureProfiles"));
            analytics["cva"] = parseBool(params.get("xva", "cva"));
            analytics["dva"] = parseBool(params.get("xva", "dva"));
            analytics["fva"] = parseBool(params.get("xva", "fva"));
            analytics["colva"] = parseBool(params.get("xva", "colva"));
            
            string baseCurrency = params.get("xva", "baseCurrency");
            string calculationType = params.get("xva", "calculationType");
            string allocationMethod = params.get("xva", "allocationMethod");
            Real marginalAllocationLimit = parseReal(params.get("xva", "marginalAllocationLimit"));
            Real quantile = parseReal(params.get("xva", "quantile"));
            string dvaName = params.get("xva", "dvaName");
            
            boost::shared_ptr<PostProcess> postProcess = boost::make_shared<PostProcess>
                (portfolio, netting, market, grid, cube, scenarioData, analytics,
                 baseCurrency, allocationMethod, marginalAllocationLimit, quantile, calculationType, dvaName);

            writeTradeExposures(params, postProcess);
            writeNettingSetExposures(params, postProcess);
            writeCva(params, portfolio, postProcess);
            
            cout << "OK" << endl;
        }
        else {
            LOG("skip XVA reports");
            cout << "SKIP" << endl;
        }

        /****************
         * Initial Margin
         */
        cout << setw(tab) << left << "Initial Margin Report... "; fflush(stdout);
        if (params.hasGroup("initialMargin") &&
            params.get("initialMargin", "active") == "Y") {
            QL_FAIL("Initial Margin reports not implemented yet");
            cout << "OK" << endl;
        }
        else {
            LOG("skip initial margin reports");
            cout << "SKIP" << endl;
        }

    } catch (std::exception& e) {
        ALOG("Error: " << e.what());
        cout << "Error: " << e.what() << endl;
    }

    cout << "run time: " << setprecision(2) << timer.elapsed() << " sec" << endl;
    cout << "ORE done." << endl;
        
    LOG("ORE done.");

    return 0;
}

void writeNpv(const Parameters& params,
              boost::shared_ptr<Market> market,
              boost::shared_ptr<Portfolio> portfolio) {
    LOG("portfolio valuation");
    //Date asof = Settings::instance().evaluationDate();
    string outputPath = params.get("setup", "outputPath"); 
    string npvFile = outputPath + "/" + params.get("npv", "outputFileName");
    string baseCurrency = params.get("npv", "baseCurrency");
    ofstream file;
    file.open(npvFile.c_str());
    file.setf (ios::fixed, ios::floatfield);
    file.setf (ios::showpoint);
    char sep = ',';
    QL_REQUIRE(file.is_open(), "error opening file " << npvFile);
    file << "#TradeId,TradeType,Maturity,NPV,NpvCurrency,NPV(Base),BaseCurrency" << endl;  
    for (auto trade : portfolio->trades()) {
        string npvCcy = trade->npvCurrency();
        Real fx = 1.0;
        if (npvCcy != baseCurrency)
            fx = market->fxSpot(npvCcy + baseCurrency)->value();
        file << trade->envelope().id() << sep
             << trade->className() << sep
             << io::iso_date(trade->maturity()) << sep;
        try {
            Real npv = trade->instrument()->NPV();
            file << npv << sep
                 << npvCcy << sep
                 << npv * fx << sep
                 << baseCurrency << endl;
        } catch (...) {
            file << "#NA" << sep << "#NA" << sep << "#NA" << sep << "#NA" << endl;
        }
    }
    file.close();
    LOG("NPV file written to " << npvFile);
}

void writeCashflow(const Parameters& params,
                   boost::shared_ptr<Market> market,
                   boost::shared_ptr<Portfolio> portfolio) {
    Date asof = Settings::instance().evaluationDate();
    string outputPath = params.get("setup", "outputPath"); 
    string fileName = outputPath + "/" + params.get("cashflow", "outputFileName");
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf (ios::fixed, ios::floatfield);
    file.setf (ios::showpoint);
    char sep = ',';
    
    LOG("Writing cashflow report to " << fileName << " for " << asof);

    file << "#ID" << sep << "Type" << sep << "LegNo" << sep
         << "PayDate" << sep
         << "Amount" << sep
         << "Currency" << sep
         << "Coupon" << sep
         << "Accrual" << sep
         << "fixingDate" << sep
         << "fixingValue" << sep
         << endl;
    
    const vector<boost::shared_ptr<Trade> >& trades = portfolio->trades();
    
    for (Size k = 0; k < trades.size(); k++) {
        if (trades[k]->className() == "Swaption" ||
            trades[k]->className() == "CapFloor") {
            WLOG("cashflow for " << trades[k]->className() << " "
                 << trades[k]->envelope().id() << " skipped");
            continue;
        }
        try {
            const vector<Leg>& legs = trades[k]->legs();
            for (size_t i=0; i<legs.size(); i++){
                const QuantLib::Leg& leg = legs[i];
                bool payer = trades[k]->legPayers()[i];
                string ccy = trades[k]->legCurrencies()[i];
                for (size_t j=0; j<leg.size(); j++){
                    boost::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];       
                    Date payDate = ptrFlow->date();
                    if (payDate >= asof) {
                        file << setprecision(0) << trades[k]->envelope().id() << sep
                             << trades[k]->envelope().type() << sep << i << sep
                             << QuantLib::io::iso_date(payDate) << sep;
                        Real amount = ptrFlow->amount();
                        if (payer)
                            amount *= -1.0;
                        file << setprecision(4) << amount << sep << ccy << sep;
                        
                        std::string ccy = trades[k]->legCurrencies()[i];
                        
                        boost::shared_ptr<QuantLib::Coupon>
                            ptrCoupon = boost::dynamic_pointer_cast<QuantLib::Coupon>(ptrFlow);
                        if (ptrCoupon) {
                            Real coupon = ptrCoupon->rate();
                            Real accrual = ptrCoupon->accrualPeriod();
                            file << setprecision(10) << coupon << sep << setprecision(10) << accrual << sep;
                        } else
                            file << sep << sep;
                        
                        boost::shared_ptr<QuantLib::FloatingRateCoupon>
                            ptrFloat = boost::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(ptrFlow);
                        if (ptrFloat) {
                            Date fixingDate = ptrFloat->fixingDate();
                            Real fixingValue = ptrFloat->index()->fixing(fixingDate);
                            file << QuantLib::io::iso_date(fixingDate) << sep
                                 << fixingValue << endl;
                        } else
                            file << sep << sep << endl;
                    }
                }
            }
        } catch(std::exception& e) {
            LOG("Exception writing to " << fileName << " : " << e.what());
        } catch (...) {
            LOG("Exception writing to " << fileName << " : Unkown Exception");
        }
    }
    file.close();    
    LOG("Cashflow report written to " << fileName);
}

void writeCurves(const Parameters& params,
                 const TodaysMarketParameters& marketConfig,
                 const boost::shared_ptr<Market>& market) {
    LOG("Write yield curve discount factors... ");
        
    string outputPath = params.get("setup", "outputPath"); 
    string fileName = outputPath + "/" + params.get("curves", "outputFileName");
    ofstream file(fileName.c_str());
    QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
    file.precision(15);
    char sep = ',';

    string configID = params.get("curves", "configuration"); 
    QL_REQUIRE(marketConfig.hasConfiguration(configID),
               "curve configuration " << configID << " not found");
    
    map<string, string> discountCurves = marketConfig.discountingCurves(configID);
    map<string, string> indexCurves = marketConfig.indexForwardingCurves(configID);
    string gridString = params.get("curves", "grid");
    DateGrid grid(gridString); 

    vector<Handle<YieldTermStructure> > yieldCurves;

    file << sep;
    for (auto it : discountCurves) {
        file << sep << it.first; //it.second;
        yieldCurves.push_back(market->discountCurve(it.first));
    }
    for (auto it : indexCurves) {
        file << sep << it.first; //it.second;
        yieldCurves.push_back(market->iborIndex(it.first)->forwardingTermStructure());
    }
    file << endl;

    // Output the discount factors for each tenor in turn 
    for (Size j = 0; j < grid.size(); ++j) {
        Date date = grid[j];
        file << grid.tenors()[j] << sep << QuantLib::io::iso_date(date);
        for (Size i = 0; i < yieldCurves.size(); ++i)
            file << sep << yieldCurves[i]->discount(date);
        file << endl;
    }

    file.close();
}

void writeTradeExposures(const Parameters& params,
                         boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    const vector<Time> times = postProcess->dateGrid()->times();
    for (Size i = 0; i < postProcess->tradeIds().size(); ++i) {
        string tradeId = postProcess->tradeIds()[i];
        ostringstream o;
        o << outputPath << "/exposure_trade_" << tradeId  << ".csv";
        string fileName = o.str();
        ofstream file(fileName.c_str());
        QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
        const vector<Real>& epe = postProcess->tradeEPE(tradeId);
        const vector<Real>& ene = postProcess->tradeENE(tradeId);
        const vector<Real>& aepe = postProcess->allocatedTradeEPE(tradeId);
        const vector<Real>& aene = postProcess->allocatedTradeENE(tradeId);
        file << "#TradeId,Period,Time,EPE,ENE,AllocatedEPE,AllocatedENE" << endl;
        for (Size j = 0; j < epe.size(); ++j)
            file << tradeId << ","
                 << j << ","
                 << times[j] << ","
                 << epe[j] << ","
                 << ene[j] << ","
                 << aepe[j] << ","
                 << aene[j] << endl;
        file.close();
    }
}
                           
void writeNettingSetExposures(const Parameters& params,
                              boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    const vector<Time> times = postProcess->dateGrid()->times();
    for (auto n : postProcess->nettingSetIds()) {
        ostringstream o;
        o << outputPath << "/exposure_nettingset_" << n << ".csv";
        string fileName = o.str();
        ofstream file(fileName.c_str());
        QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
        const vector<Real>& epe = postProcess->netEPE(n);
        const vector<Real>& ene = postProcess->netENE(n);
        const vector<Real>& ecb = postProcess->expectedCollateral(n);
        file << "#TradeId,Period,Time,EPE,ENE,ExpectedCollateral" << endl;
        for (Size j = 0; j < epe.size(); ++j)
            file << n << ","
                 << j << ","
                 << times[j] << ","
                 << epe[j] << ","
                 << ene[j] << ","
                 << ecb[j] << endl;
        file.close();
    }
}

void writeCva(const Parameters& params,
              boost::shared_ptr<Portfolio> portfolio,
              boost::shared_ptr<PostProcess> postProcess) {
    string outputPath = params.get("setup", "outputPath");
    string allocationMethod = params.get("xva", "allocationMethod");
    const vector<Time> times = postProcess->dateGrid()->times();
    string fileName = outputPath + "/xva.csv";
    ofstream file(fileName.c_str());
    QL_REQUIRE(file.is_open(), "Error opening file " << fileName);
    file << "#TradeId,NettingSetId,CVA,AllocatedCVA,DVA,AllocatedDVA,AllocationMethod" << endl; 
    for (auto n : postProcess->nettingSetIds()) {
        file << ","
             << n << ","
             << postProcess->nettingSetCVA(n) << ","
             << postProcess->nettingSetCVA(n) << ","
             << postProcess->nettingSetDVA(n) << ","
             << postProcess->nettingSetDVA(n) << "," << endl;
        for (Size k = 0; k < portfolio->trades().size(); ++k) {
            string tid = portfolio->trades()[k]->envelope().id();
            string nid = portfolio->trades()[k]->envelope().nettingSetId();
            if (nid != n) continue;
            file << tid << ","
                 << nid << ","
                 << postProcess->tradeCVA(tid) << ","
                 << postProcess->allocatedTradeCVA(tid) << ","
                 << postProcess->tradeDVA(tid) << ","
                 << postProcess->allocatedTradeDVA(tid) << ","
                 << allocationMethod << endl;
        }
    }
    file.close();
}

bool Parameters::hasGroup(const string& groupName) const {
    return (data_.find(groupName) != data_.end());
}

bool Parameters::has(const string& groupName, const string& paramName) const {
    QL_REQUIRE(hasGroup(groupName), "param group '" << groupName << "' not found");
    auto it = data_.find(groupName);
    return (it->second.find(paramName) != it->second.end());
}

string Parameters::get(const string& groupName, const string& paramName) const {
    QL_REQUIRE(has(groupName, paramName),
               "parameter " << paramName << " not found in param group " << groupName);
    auto it = data_.find(groupName);
    return it->second.find(paramName)->second;
}

void Parameters::fromFile(const string& fileName) {
    LOG("load ORE configuration from " << fileName);
    clear();
    XMLDocument doc(fileName);
    fromXML(doc.getFirstNode("OpenRiskEngine"));
    LOG("load ORE configuration from " << fileName << " done.");
}

void Parameters::clear() {
    data_.clear();
}

void Parameters::fromXML(XMLNode *node) {
    XMLUtils::checkNode(node, "OpenRiskEngine");
    
    XMLNode* setupNode = XMLUtils::getChildNode(node, "Setup");
    
    map<string,string> setupMap;
    for (XMLNode* child = XMLUtils::getChildNode(setupNode); child;
         child = XMLUtils::getNextSibling(child)) {
        string key = XMLUtils::getAttribute(child, "name"); 
        string value = XMLUtils::getNodeValue(child);
        setupMap[key] = value;
    }
    data_["setup"] = setupMap; 
    
    XMLNode* analyticsNode = XMLUtils::getChildNode(node, "Analytics");
    for (XMLNode* child = XMLUtils::getChildNode(analyticsNode); child;
         child = XMLUtils::getNextSibling(child)) {
        string groupName = XMLUtils::getAttribute(child, "type");
        map<string,string> analyticsMap;
        for (XMLNode* paramNode = XMLUtils::getChildNode(child); paramNode;
             paramNode = XMLUtils::getNextSibling(paramNode)) {
            string key = XMLUtils::getAttribute(paramNode, "name");
            string value = XMLUtils::getNodeValue(paramNode);
            analyticsMap[key] = value;
        }
        data_[groupName] = analyticsMap; 
    }
}

XMLNode* Parameters::toXML(XMLDocument& doc) {
    XMLNode *node = doc.allocNode("OpenRiskEngine");
    QL_FAIL("Parameters::toXML not implemented yet");
    return node;
}

void Parameters::log() {
    LOG("Parameters:");
    for (auto p : data_) 
        for (auto pp : p.second) 
            LOG("group = " << p.first << " : " << pp.first << " = " << pp.second);
}

