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

string remove(const string& input, const string& ending) {
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
            // skip cross scenarios here
            if (label.find("CROSS") != string::npos)
                continue;
            Real npv = cube->get(i, 0, j, 0);
            if (label.find("/UP") != string::npos) {
                string factor = remove(label, "/UP");
                factors_.insert(factor);
                pair<string, string> p(id, factor);
                upNPV_[p] = npv;
                delta_[p] = npv - npv0;
            } else if (label.find("/DOWN") != string::npos) {
                string factor = remove(label, "/DOWN");
                factors_.insert(factor);
                pair<string, string> p(id, factor);
                downNPV_[p] = npv;
            } else if (label != "BASE")
                QL_FAIL("label ending /UP or /DOWN expected in: " << label);
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
    file << "#TradeId" << sep << "Factor" << sep << "Base NPV" << sep << "Delta*Shift" << sep << "Gamma*Shift^2"
         << endl;
    LOG("Write sensitivity output to " << fileName);
    for (auto data : delta_) {
        pair<string, string> p = data.first;
        string id = data.first.first;
        string factor = data.first.second;
        Real delta = data.second;
        Real gamma = gamma_[p];
        Real base = baseNPV_[id];
        if (fabs(delta) > outputThreshold || fabs(gamma) > outputThreshold)
            file << id << sep << factor << sep << base << sep << delta << sep << gamma << endl;
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
         << endl;
    LOG("Write cross gamma output to " << fileName);
    for (auto data : crossGamma_) {
        string id = std::get<0>(data.first);
        string factor1 = std::get<1>(data.first);
        string factor2 = std::get<2>(data.first);
        Real crossGamma = data.second;
        Real base = baseNPV_[id];
        if (fabs(crossGamma) > outputThreshold)
            file << id << sep << factor1 << sep << factor2 << sep << base << sep << crossGamma << endl;
    }
    file.close();
}
}
}
