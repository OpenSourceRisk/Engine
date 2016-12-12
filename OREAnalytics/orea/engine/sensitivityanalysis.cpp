/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/scenarioengine.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <ored/utilities/log.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/errors.hpp>
#include <boost/timer.hpp>

#include <iostream>
#include <iomanip>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

string SensitivityAnalysis::remove(const string& input, const string& ending) {
    string output = input;
    std::string::size_type i = output.find(ending);
    if (i != std::string::npos)
        output.erase(i, ending.length());
    return output;
}

SensitivityAnalysis::SensitivityAnalysis(boost::shared_ptr<ore::data::Portfolio>& portfolio,
                                         boost::shared_ptr<ore::data::Market>& market,
                                         const string& marketConfiguration,
                                         const boost::shared_ptr<ore::data::EngineData>& engineData,
                                         boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                         const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                                         const Conventions& conventions) {

    LOG("Build Sensitivity Scenario Generator");
    Date asof = market->asofDate();
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        boost::make_shared<SensitivityScenarioGenerator>(scenarioFactory, sensitivityData, simMarketData, asof, market);
    boost::shared_ptr<ScenarioGenerator> sgen(scenarioGenerator);

    LOG("Build Simulation Market");
    boost::shared_ptr<ScenarioSimMarket> simMarket =
        boost::make_shared<ScenarioSimMarket>(sgen, market, simMarketData, conventions);

    LOG("Build Engine Factory");
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration;
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(engineData, simMarket, configurations);

    LOG("Reset and Build Portfolio");
    portfolio->reset();
    portfolio->build(factory);

    LOG("Build the cube object to store sensitivities");
    boost::shared_ptr<NPVCube> cube = boost::make_shared<DoublePrecisionInMemoryCube>(
        asof, portfolio->ids(), vector<Date>(1, asof), scenarioGenerator->samples());

    LOG("Build Scenario Engine");
    ScenarioEngine engine(asof, simMarket, simMarketData->baseCcy());

    LOG("Run Sensitivity Scenarios");
    // no progress bar here
    engine.buildCube(portfolio, cube);

    string upString = sensitivityData->labelSeparator() + sensitivityData->shiftDirectionLabel(true);
    string downString = sensitivityData->labelSeparator() + sensitivityData->shiftDirectionLabel(false);

    // base NPV, up NPV, down NPV, delta, cross gamma
    baseNPV_.clear();
    for (Size i = 0; i < portfolio->size(); ++i) {
        Real npv0 = cube->getT0(i, 0);
        string id = portfolio->trades()[i]->id();
        trades_.insert(id);
        baseNPV_[id] = npv0;
        // single shift scenarios: up, down, delta
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            string label = scenarioGenerator->scenarios()[j]->label();
            if (sensitivityData->isSingleShiftScenario(label)) {
                Real npv = cube->get(i, 0, j, 0);
                if (sensitivityData->isUpShiftScenario(label)) {
                    string factor = sensitivityData->labelToFactor(label);
                    factors_.insert(factor);
                    pair<string, string> p(id, factor);
                    upNPV_[p] = npv;
                    delta_[p] = npv - npv0;
                } else if (sensitivityData->isDownShiftScenario(label)) {
                    string factor = sensitivityData->labelToFactor(label);
                    factors_.insert(factor);
                    pair<string, string> p(id, factor);
                    downNPV_[p] = npv;
                }
            }
        }
        // double shift scenarios: cross gamma
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            string label = scenarioGenerator->scenarios()[j]->label();
            // select cross scenarios here
            if (label.find("CROSS") == string::npos)
                continue;
            Real npv = cube->get(i, 0, j, 0);
            vector<string> tokens;
            boost::split(tokens, label, boost::is_any_of(":"));
            QL_REQUIRE(tokens.size() == 3, "expected 3 tokens, found " << tokens.size());
            string factor1up = tokens[1];
            string factor2up = tokens[2];
            QL_REQUIRE(factor1up.find("/UP") != string::npos, "scenario " << factor1up << " is not an up shift");
            QL_REQUIRE(factor2up.find("/UP") != string::npos, "scenario " << factor2up << " is not an up shift");
            string factor1 = remove(factor1up, "/UP");
            string factor2 = remove(factor2up, "/UP");
            std::pair<string, string> p1(id, factor1);
            std::pair<string, string> p2(id, factor2);
            // f_xy(x,y) = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
            Real base = baseNPV_[id];
            Real up1 = upNPV_[p1];
            Real up2 = upNPV_[p2];
            std::tuple<string, string, string> triple(id, factor1, factor2);
            crossGamma_[triple] = npv - up1 - up2 + base; // f_xy(x,y) * u * v
        }
    }

    vector<string> yieldFactors;
    for (auto f : factors_) {
        if (f.find("YIELD") != string::npos)
            yieldFactors.push_back(f);
    }

    // gamma
    for (auto data : upNPV_) {
        pair<string, string> p = data.first;
        Real u = data.second;
        string id = p.first;
        string factor = p.second;
        QL_REQUIRE(baseNPV_.find(id) != baseNPV_.end(), "base NPV not found for trade " << id);
        Real b = baseNPV_[id];
        QL_REQUIRE(downNPV_.find(p) != downNPV_.end(), "down shift result not found for trade " << id << ", factor "
                                                                                                << factor);
        Real d = downNPV_[p];
        // f_x(x) = (f(x+u) - f(x)) / u
        delta_[p] = u - b; // = f_x(x) * u
        // f_xx(x) = (f(x+u) - 2*f(x) + f(x-u)) / u^2
        gamma_[p] = u - 2.0 * b + d; // = f_xx(x) * u^2
    }

    ///////////////////////////////////////////////
    // Instrument fair rate sensitivity to z shifts

    scenarioGenerator->reset();
    simMarket->update(asof);

    map<string, vector<boost::shared_ptr<RateHelper> > > parInstruments;
    map<string, vector<Real> > parRatesBase;
    Size n_ten = sensitivityData->discountShiftTenors().size();
    QL_REQUIRE(sensitivityData->discountParInstruments().size() == n_ten,
               "number of tenors does not match number of discount curve par instruments");
    for (Size i = 0; i < simMarketData->ccys().size(); ++i) {
        string ccy = simMarketData->ccys()[i];
        parInstruments[ccy] = vector<boost::shared_ptr<RateHelper> >(n_ten);
        parRatesBase[ccy] = vector<Real>(n_ten);
        for (Size j = 0; j < n_ten; ++j) {
            Period term = sensitivityData->discountShiftTenors()[j];
            string instType = sensitivityData->discountParInstruments()[j];
            pair<string, string> p(ccy, instType);
            map<pair<string, string>, string> conventionsMap = sensitivityData->discountParInstrumentConventions();
            QL_REQUIRE(conventionsMap.find(p) != conventionsMap.end(),
                       "conventions not found for ccy " << ccy << " and instrument type " << instType);
            boost::shared_ptr<Convention> convention = conventions.get(conventionsMap[p]);
            string indexName = ""; // pick from conventions
            if (instType == "IRS")
                parInstruments[ccy][j] = makeSwap(ccy, indexName, term, simMarket, convention, true);
            else if (instType == "DEP")
                parInstruments[ccy][j] = makeDeposit(ccy, indexName, term, simMarket, convention, true);
            else if (instType == "FRA")
                parInstruments[ccy][j] = makeFRA(ccy, indexName, term, simMarket, convention, true);
            else if (instType == "OIS")
                parInstruments[ccy][j] = makeOIS(ccy, indexName, term, simMarket, convention, true);
            else
                QL_FAIL("Instrument type " << instType << " for par sensitivity conversin not recognised");
            parRatesBase[ccy][j] = parInstruments[ccy][j]->impliedQuote();
        }
    }

    QL_REQUIRE(sensitivityData->indexParInstruments().size() == n_ten,
               "number of tenors does not match number of index curve par instruments");
    for (Size i = 0; i < simMarketData->indices().size(); ++i) {
        string indexName = simMarketData->indices()[i];
        vector<string> tokens;
        boost::split(tokens, indexName, boost::is_any_of("-"));
        string ccy = tokens[0];
        QL_REQUIRE(ccy.length() == 3, "currency token not recognised");
        parInstruments[indexName] = vector<boost::shared_ptr<RateHelper> >(n_ten);
        parRatesBase[indexName] = vector<Real>(n_ten);
        for (Size j = 0; j < n_ten; ++j) {
            Period term = sensitivityData->indexShiftTenors()[j];
            string instType = sensitivityData->indexParInstruments()[j];
            pair<string, string> p(ccy, instType);
            map<pair<string, string>, string> conventionsMap = sensitivityData->indexParInstrumentConventions();
            QL_REQUIRE(conventionsMap.find(p) != conventionsMap.end(),
                       "conventions not found for ccy " << ccy << " and instrument type " << instType);
            boost::shared_ptr<Convention> convention = conventions.get(conventionsMap[p]);
            if (instType == "IRS")
                parInstruments[indexName][j] = makeSwap(ccy, indexName, term, simMarket, convention, false);
            else if (instType == "DEP")
                parInstruments[indexName][j] = makeDeposit(ccy, indexName, term, simMarket, convention, false);
            else if (instType == "FRA")
                parInstruments[indexName][j] = makeFRA(ccy, indexName, term, simMarket, convention, false);
            else if (instType == "OIS")
                parInstruments[indexName][j] = makeOIS(ccy, indexName, term, simMarket, convention, false);
            else
                QL_FAIL("Instrument type " << instType << " for par sensitivity conversin not recognised");
            parRatesBase[indexName][j] = parInstruments[indexName][j]->impliedQuote();
            LOG("Par instrument for index " << indexName << " ccy " << ccy << " tenor " << j << " base rate "
                                            << setprecision(4) << parRatesBase[indexName][j]);
        }
    }

    ofstream file;
    file.open("ratesensi.csv");
    vector<string> labels;

    map<pair<string, string>, vector<Real> > parRatesSensi, parRates;
    for (Size i = 1; i < scenarioGenerator->samples(); ++i) {
        string label = scenarioGenerator->scenarios()[i]->label();
        // use "UP" shift scenarios only
        simMarket->update(asof);

        if (label.find("/UP") != string::npos && label.find("CROSS") == string::npos &&
            (label.find("DISCOUNT") != string::npos || label.find("INDEX") != string::npos)) {

            for (Size j = 0; j < simMarketData->ccys().size(); ++j) {
                string ccy = simMarketData->ccys()[j];
                if (label.find(ccy) == string::npos)
                    continue;
                pair<string, string> key(ccy, remove(label, "/UP"));
                parRates[key] = vector<Real>(n_ten);
                parRatesSensi[key] = vector<Real>(n_ten);
                LOG("discountParRatesSensi.size() = " << parRatesSensi[key].size());
                file << label << "," << ccy;
                for (Size k = 0; k < n_ten; ++k) {
                    Real fair = parInstruments[ccy][k]->impliedQuote();
                    Real base = parRatesBase[ccy][k];
                    parRates[key][k] = fair;
                    parRatesSensi[key][k] = (fair - base) / sensitivityData->discountShiftSize();
                    file << "," << (fair - base) / sensitivityData->discountShiftSize();
                }
                file << endl;
            }

            for (Size j = 0; j < simMarketData->indices().size(); ++j) {
                string indexName = simMarketData->indices()[j];
                vector<string> tokens;
                boost::split(tokens, indexName, boost::is_any_of("-"));
                QL_REQUIRE(tokens.size() > 1, "expected 2 or 3 tokens, found " << tokens.size() << " in " << indexName);
                string ccy = tokens[0];
                if (label.find(ccy) == string::npos)
                    continue;
                pair<string, string> key(indexName, remove(label, "/UP"));
                parRates[key] = vector<Real>(n_ten);
                parRatesSensi[key] = vector<Real>(n_ten);
                LOG("indexParRatesSensi.size() = " << parRatesSensi[key].size());
                file << label << "," << indexName;
                for (Size k = 0; k < n_ten; ++k) {
                    Real fair = parInstruments[indexName][k]->impliedQuote();
                    Real base = parRatesBase[indexName][k];
                    parRates[key][k] = fair;
                    parRatesSensi[key][k] = (fair - base) / sensitivityData->indexShiftSize();
                    file << "," << (fair - base) / sensitivityData->indexShiftSize();
                }
                file << endl;
            }
        }
    }

    file.close();

    // Build Jacobi matrices for each curve
    ParSensitivityConverter jacobi(sensitivityData, yieldFactors, delta_, parRatesSensi);
    parDelta_ = jacobi.parDelta();
}

void SensitivityAnalysis::writeScenarioReport(string fileName, Real outputThreshold) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "#TradeId" << sep << "Scenario Label" << sep << "Up/Down" << sep << "Base NPV" << sep << "Scenario NPV"
         << sep << "Sensitivity" << endl;
    LOG("Write scenario output to " << fileName);
    for (auto data : upNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold)
            file << id << sep << factor << sep << "Up" << sep << base << sep << npv << sep << sensi << endl;
    }
    for (auto data : downNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold)
            file << id << sep << factor << sep << "Down" << sep << base << sep << npv << sep << sensi << endl;
    }
    file.close();
}

void SensitivityAnalysis::writeSensitivityReport(string fileName, Real outputThreshold) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "#TradeId" << sep << "Factor" << sep << "Base NPV" << sep << "Delta*Shift" << sep << "ParDelta*Shift" << sep
         << "Gamma*Shift^2" << sep << "ParGamma*Shift^2" << endl;
    LOG("Write sensitivity output to " << fileName);
    for (auto data : delta_) {
        pair<string, string> p = data.first;
        string id = data.first.first;
        string factor = data.first.second;
        Real delta = data.second;
        Real gamma = gamma_[p];
        Real base = baseNPV_[id];
        if (fabs(delta) > outputThreshold || fabs(gamma) > outputThreshold) {
            if (parDelta_.find(p) != parDelta_.end())
                file << id << sep << factor << sep << base << sep << delta << sep << parDelta_[p] << sep << gamma << sep
                     << "N/A" << endl;
            else
                file << id << sep << factor << sep << base << sep << delta << sep << "N/A" << sep << gamma << sep
                     << "N/A" << endl;
        }
    }
    file.close();
}

void SensitivityAnalysis::writeCrossGammaReport(string fileName, Real outputThreshold) {
    ofstream file;
    file.open(fileName.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << fileName);
    file.setf(ios::fixed, ios::floatfield);
    file.setf(ios::showpoint);
    char sep = ',';
    file << "#TradeId" << sep << "Factor 1" << sep << "Factor 2" << sep << "Base NPV" << sep << "CrossGamma*Shift^2"
         << sep << "ParCrossGamma*Shift^2" << endl;
    LOG("Write cross gamma output to " << fileName);
    for (auto data : crossGamma_) {
        string id = std::get<0>(data.first);
        string factor1 = std::get<1>(data.first);
        string factor2 = std::get<2>(data.first);
        Real crossGamma = data.second;
        Real base = baseNPV_[id];
        if (fabs(crossGamma) > outputThreshold)
            file << id << sep << factor1 << sep << factor2 << sep << base << sep << crossGamma << sep << "N/A" << endl;
    }
    file.close();
}

boost::shared_ptr<RateHelper> SensitivityAnalysis::makeSwap(string ccy, string indexName, Period term,
                                                            const boost::shared_ptr<ore::data::Market>& market,
                                                            const boost::shared_ptr<Convention>& conventions,
                                                            bool singleCurve) {
    boost::shared_ptr<IRSwapConvention> conv = boost::dynamic_pointer_cast<IRSwapConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected IRSwapConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    // LOG("Make Swap for ccy " << ccy << " index " << name);
    boost::shared_ptr<SwapRateHelper> helper;
    if (singleCurve) {
        helper = boost::make_shared<SwapRateHelper>(Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(0.05))),
                                                    term, conv->fixedCalendar(), conv->fixedFrequency(),
                                                    conv->fixedConvention(), conv->fixedDayCounter(), index);
        helper->setTermStructure(yts.currentLink().get());
    } else {
        helper =
            boost::make_shared<SwapRateHelper>(Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(0.05))), term,
                                               conv->fixedCalendar(), conv->fixedFrequency(), conv->fixedConvention(),
                                               conv->fixedDayCounter(), index, Handle<Quote>(), 0 * Days, yts);
        helper->setTermStructure(index->forwardingTermStructure().currentLink().get());
    }
    return helper;
}

boost::shared_ptr<RateHelper> SensitivityAnalysis::makeDeposit(string ccy, string indexName, Period term,
                                                               const boost::shared_ptr<ore::data::Market>& market,
                                                               const boost::shared_ptr<Convention>& conventions,
                                                               bool singleCurve) {
    boost::shared_ptr<DepositConvention> conv = boost::dynamic_pointer_cast<DepositConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected DepositConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName;
    // if empty, use conventions and append term
    if (name == "") {
        ostringstream o;
        o << conv->index() << "-" << term;
        name = o.str();
        boost::to_upper(name);
        LOG("Deposit index name = " << name);
    }
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    boost::shared_ptr<DepositRateHelper> helper = boost::make_shared<DepositRateHelper>(
        Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(0.05))), term, index->fixingDays(),
        index->fixingCalendar(), index->businessDayConvention(), index->endOfMonth(), index->dayCounter());
    if (singleCurve)
        helper->setTermStructure(yts.currentLink().get());
    else
        helper->setTermStructure(index->forwardingTermStructure().currentLink().get());
    return helper;
}

boost::shared_ptr<RateHelper> SensitivityAnalysis::makeFRA(string ccy, string indexName, Period term,
                                                           const boost::shared_ptr<ore::data::Market>& market,
                                                           const boost::shared_ptr<Convention>& conventions,
                                                           bool singleCurve) {
    boost::shared_ptr<FraConvention> conv = boost::dynamic_pointer_cast<FraConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected FraConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    QL_REQUIRE(term.units() == Months, "term unit must be Months");
    QL_REQUIRE(index->tenor().units() == Months, "index tenor unit must be Months");
    QL_REQUIRE(term.length() > index->tenor().length(), "term must be larger than index tenor");
    Natural monthsToStart = term.length() - index->tenor().length();
    boost::shared_ptr<FraRateHelper> helper = boost::make_shared<FraRateHelper>(
        Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(0.05))), monthsToStart, index);
    if (singleCurve)
        helper->setTermStructure(yts.currentLink().get());
    else
        helper->setTermStructure(index->forwardingTermStructure().currentLink().get());
    return helper;
}

boost::shared_ptr<RateHelper> SensitivityAnalysis::makeOIS(string ccy, string indexName, Period term,
                                                           const boost::shared_ptr<ore::data::Market>& market,
                                                           const boost::shared_ptr<Convention>& conventions,
                                                           bool singleCurve) {
    boost::shared_ptr<OisConvention> conv = boost::dynamic_pointer_cast<OisConvention>(conventions);
    QL_REQUIRE(conv, "convention not recognised, expected OisConvention");
    Handle<YieldTermStructure> yts = market->discountCurve(ccy);
    string name = indexName != "" ? indexName : conv->indexName();
    boost::shared_ptr<IborIndex> index = market->iborIndex(name).currentLink();
    boost::shared_ptr<OvernightIndex> overnightIndex = boost::dynamic_pointer_cast<OvernightIndex>(index);
    boost::shared_ptr<QuantLib::OISRateHelper> helper;
    if (singleCurve) {
        helper = boost::make_shared<QuantLib::OISRateHelper>(
            conv->spotLag(), term, Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(0.05))), overnightIndex);
        helper->setTermStructure(yts.currentLink().get());
    } else {
        helper = boost::make_shared<QuantLib::OISRateHelper>(
            conv->spotLag(), term, Handle<Quote>(boost::shared_ptr<Quote>(new SimpleQuote(0.05))), overnightIndex, yts);
        helper->setTermStructure(index->forwardingTermStructure().currentLink().get());
    }
    return helper;
}

void ParSensitivityConverter::buildJacobiMatrix() {
    vector<string> currencies = sensitivityData_->discountCurrencies();
    Size n_discountTenors = sensitivityData_->discountShiftTenors().size();
    vector<string> indices = sensitivityData_->indexNames();
    Size n_indexTenors = sensitivityData_->indexShiftTenors().size();
    Size n_zeroShifts = yieldFactors_.size();
    Size n_par = currencies.size() * n_discountTenors + indices.size() * n_indexTenors;
    jacobi_ = Matrix(n_par, n_zeroShifts, 0.0);

    LOG("Build Jacobi matrix");
    for (Size i = 0; i < yieldFactors_.size(); ++i)
        LOG("Yield factor " << i << " " << yieldFactors_[i]);

    // Check ordering of the yield curve factors
    vector<string> curves, types;
    for (auto f : yieldFactors_) {
        vector<string> tokens;
        boost::split(tokens, f, boost::is_any_of("/"));
        QL_REQUIRE(tokens.size() >= 2, "more than two toekns expected in " << f);
        QL_REQUIRE(tokens[0] == "YIELD_DISCOUNT" || tokens[0] == "YIELD_INDEX", "discount or index factors expected");
        string type = tokens[0];
        string curve = tokens[1];
        if (curves.size() == 0 || curve != curves.back()) {
            curves.push_back(curve);
            types.push_back(type);
        }
    }

    Size offset = 0;
    for (Size i = 0; i < curves.size(); ++i) {
        string curve = curves[i];
        LOG("Curve " << i << " " << curve);
        Size tenors = types[i] == "YIELD_DISCOUNT" ? n_discountTenors : n_indexTenors;
        for (Size k = 0; k < n_zeroShifts; ++k) {
            string factor = yieldFactors_[k];
            pair<string, string> p(curve, factor);
            vector<Real> v;
            if (parRateSensi_.find(p) == parRateSensi_.end())
                v = vector<Real>(tenors, 0.0);
            else
                v = parRateSensi_[p];
            for (Size j = 0; j < tenors; ++j) {
                jacobi_[offset + j][k] = v[j];
            }
        }
        offset += tenors;
    }

    LOG("Jacobi matrix dimension " << jacobi_.rows() << " x " << jacobi_.columns());

    jacobiInverse_ = inverse(jacobi_);

    LOG("Inverse Jacobi done");
}

void ParSensitivityConverter::convertSensitivity() {
    set<string> trades;
    for (auto d : delta_) {
        string trade = d.first.first;
        trades.insert(trade);
    }
    for (auto t : trades) {
        Array deltaArray(yieldFactors_.size(), 0.0);
        for (Size i = 0; i < yieldFactors_.size(); ++i) {
            pair<string, string> p(t, yieldFactors_[i]);
            if (delta_.find(p) != delta_.end())
                deltaArray[i] = delta_[p];
            if (t == "SWAP_EUR")
                LOG("delta " << yieldFactors_[i] << " " << deltaArray[i]);
        }
        Array parDeltaArray = transpose(jacobiInverse_) * deltaArray;
        for (Size i = 0; i < yieldFactors_.size(); ++i) {
            if (parDeltaArray[i] != 0.0) {
                pair<string, string> p(t, yieldFactors_[i]);
                parDelta_[p] = parDeltaArray[i];
                if (t == "SWAP_EUR")
                    LOG("par delta " << yieldFactors_[i] << " " << deltaArray[i] << " " << parDeltaArray[i]);
            }
        }
    }
}
}
}
