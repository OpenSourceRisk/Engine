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

    // first pass: fill base, up and down NPVs
    baseNPV_.clear();
    for (Size i = 0; i < portfolio->size(); ++i) {
        Real npv0 = cube->getT0(i, 0);
        string id = portfolio->trades()[i]->id();
        trades_.insert(id);
        baseNPV_[id] = npv0;
        for (Size j = 0; j < scenarioGenerator->samples(); ++j) {
            Real npv = cube->get(i, 0, j, 0);
            string label = scenarioGenerator->scenarios()[j]->label();
            if (label.find("/UP") != string::npos) {
                string factor = remove(label, "/UP");
                factors_.insert(factor);
                pair<string, string> p(id, factor);
                upNPV_[p] = npv;
            } else if (label.find("/DOWN") != string::npos) {
                string factor = remove(label, "/DOWN");
                factors_.insert(factor);
                pair<string, string> p(id, factor);
                downNPV_[p] = npv;
            } else if (label != "BASE")
                QL_FAIL("label ending /UP or /DOWN expected in: " << label);
        }
    }

    // second pass: fill delta and gamma
    for (auto data : upNPV_) {
        pair<string, string> p = data.first;
        Real u = data.second;
        string id = p.first;
        string factor = p.second;
        QL_REQUIRE(baseNPV_.find(id) != baseNPV_.end(), "base NPV not found for trade " << id);
        Real b = baseNPV_[id];
        QL_REQUIRE(downNPV_.find(p) != downNPV_.end(), "down shift result not found for trade " << id << ", factor"
                                                                                                << factor);
        Real d = downNPV_[p];
        // f'(x) = (f(x+s) - f(x)) / s
        delta_[p] = u - b; // f'(x) * s
        // f''(x) = (f'(x) - f'(x-s)) / s = ((f(x+s) - f(x))/s - (f(x)-f(x-s))/s ) / s = ( f(x+s) - 2*f(x) + f(x-s) ) /
        // s^2
        gamma_[p] = u - 2.0 * b + d; // f''(x) * s^2
    }
    //
    // f(x,y):
    // \partial_y \partial_x f(x,y) = \partial_y (f(x+u,y) - f(x,y)) / u)
    // = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
    //
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
}
}
