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

#include <boost/lexical_cast.hpp>
#include <boost/timer.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/scenarioengine.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/report/csvreport.hpp>
#include <ql/errors.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/pricingengines/depositengine.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/pricingengines/discountingfxforwardengine.hpp>

#include <iomanip>
#include <iostream>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace ore {
namespace analytics {

SensitivityAnalysis::SensitivityAnalysis(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                                         const boost::shared_ptr<ore::data::Market>& market,
                                         const string& marketConfiguration,
                                         const boost::shared_ptr<ore::data::EngineData>& engineData,
                                         const boost::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                                         const boost::shared_ptr<SensitivityScenarioData>& sensitivityData,
                                         const Conventions& conventions)
    : market_(market), marketConfiguration_(marketConfiguration), asof_(market->asofDate()),
      simMarketData_(simMarketData), sensitivityData_(sensitivityData), conventions_(conventions) {

    LOG("Build Sensitivity Scenario Generator");
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    scenarioGenerator_ = boost::make_shared<SensitivityScenarioGenerator>(scenarioFactory, sensitivityData_,
                                                                          simMarketData_, asof_, market_);
    boost::shared_ptr<ScenarioGenerator> sgen(scenarioGenerator_);

    LOG("Build Simulation Market");
    simMarket_ = boost::make_shared<ScenarioSimMarket>(sgen, market_, simMarketData_, conventions_);

    LOG("Build Engine Factory");
    map<MarketContext, string> configurations;
    configurations[MarketContext::pricing] = marketConfiguration;
    boost::shared_ptr<EngineFactory> factory =
        boost::make_shared<EngineFactory>(engineData, simMarket_, configurations);

    LOG("Reset and Build Portfolio");
    portfolio->reset();
    portfolio->build(factory);

    LOG("Build the cube object to store sensitivities");
    boost::shared_ptr<NPVCube> cube = boost::make_shared<DoublePrecisionInMemoryCube>(
        asof_, portfolio->ids(), vector<Date>(1, asof_), scenarioGenerator_->samples());

    LOG("Build Scenario Engine");
    ScenarioEngine engine(asof_, simMarket_, simMarketData_->baseCcy());

    LOG("Run Sensitivity Scenarios");
    // no progress bar here
    engine.buildCube(portfolio, cube);

    /***********************************************
     * Collect results
     * - base NPVs,
     * - NPVs after single factor up shifts,
     * - NPVs after single factor down shifts
     * - deltas, gammas and cross gammas
     */
    baseNPV_.clear();
    for (Size i = 0; i < portfolio->size(); ++i) {

        Real npv0 = cube->getT0(i, 0);
        string id = portfolio->trades()[i]->id();
        trades_.insert(id);
        baseNPV_[id] = npv0;

        // single shift scenarios: up, down, delta
        for (Size j = 0; j < scenarioGenerator_->samples(); ++j) {
            string label = scenarioGenerator_->scenarios()[j]->label();
            if (sensitivityData_->isSingleShiftScenario(label)) {
                Real npv = cube->get(i, 0, j, 0);
                string factor = sensitivityData_->labelToFactor(label);
                pair<string, string> p(id, factor);
                if (sensitivityData_->isUpShiftScenario(label)) {
                    upNPV_[p] = npv;
                    delta_[p] = npv - npv0;
                } else if (sensitivityData_->isDownShiftScenario(label)) {
                    downNPV_[p] = npv;
                } else
                    continue;
                factors_.insert(factor);
            }
        }

        // double shift scenarios: cross gamma
        for (Size j = 0; j < scenarioGenerator_->samples(); ++j) {
            string label = scenarioGenerator_->scenarios()[j]->label();
            // select cross scenarios here
            if (sensitivityData_->isCrossShiftScenario(label)) {
                Real npv = cube->get(i, 0, j, 0);
                string f1up = sensitivityData_->getCrossShiftScenarioLabel(label, 1);
                string f2up = sensitivityData_->getCrossShiftScenarioLabel(label, 2);
                QL_REQUIRE(sensitivityData_->isUpShiftScenario(f1up), "scenario " << f1up << " not an up shift");
                QL_REQUIRE(sensitivityData_->isUpShiftScenario(f2up), "scenario " << f2up << " not an up shift");
                string f1 = sensitivityData_->labelToFactor(f1up);
                string f2 = sensitivityData_->labelToFactor(f2up);
                std::pair<string, string> p1(id, f1);
                std::pair<string, string> p2(id, f2);
                // f_xy(x,y) = (f(x+u,y+v) - f(x,y+v) - f(x+u,y) + f(x,y)) / (u*v)
                Real base = baseNPV_[id];
                Real up1 = upNPV_[p1];
                Real up2 = upNPV_[p2];
                std::tuple<string, string, string> triple(id, f1, f2);
                crossGamma_[triple] = npv - up1 - up2 + base; // f_xy(x,y) * u * v
            }
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

    LOG("Sensitivity analysis done");
}

void SensitivityAnalysis::writeScenarioReport(string fileName, Real outputThreshold) {

    CSVFileReport report(fileName);

    report.addColumn("#TradeId", string());
    report.addColumn("ScenarioLabel", string());
    report.addColumn("Up/Down", string());
    report.addColumn("Base NPV", double(), 2);
    report.addColumn("Scenario NPV", double(), 2);
    report.addColumn("Sensitivity", double(), 2);

    for (auto data : upNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold) {
            report.next();
            report.add(id);
            report.add(factor);
            report.add("Up");
            report.add(base);
            report.add(npv);
            report.add(sensi);
        }
    }

    for (auto data : downNPV_) {
        string id = data.first.first;
        string factor = data.first.second;
        Real npv = data.second;
        Real base = baseNPV_[id];
        Real sensi = npv - base;
        if (fabs(sensi) > outputThreshold) {
            report.next();
            report.add(id);
            report.add(factor);
            report.add("Down");
            report.add(base);
            report.add(npv);
            report.add(sensi);
        }
    }

    report.end();
}

void SensitivityAnalysis::writeSensitivityReport(string fileName, Real outputThreshold) {
    CSVFileReport report(fileName);

    report.addColumn("#TradeId", string());
    report.addColumn("Factor", string());
    report.addColumn("Base NPV", double(), 2);
    report.addColumn("Delta*Shift", double(), 2);
    report.addColumn("ParDelta*Shift", double(), 2);
    report.addColumn("Gamma*Shift^2", double(), 2);
    report.addColumn("ParGamma*Shift^2", double(), 2); // TODO

    for (auto data : delta_) {
        pair<string, string> p = data.first;
        string id = data.first.first;
        string factor = data.first.second;
        Real delta = data.second;
        Real gamma = gamma_[p];
        Real base = baseNPV_[id];
        if (fabs(delta) > outputThreshold || fabs(gamma) > outputThreshold) {
            report.next();
            report.add(id);
            report.add(factor);
            report.add(base);
            report.add(delta);
            if (parDelta_.find(p) != parDelta_.end())
                report.add(parDelta_[p]);
            else
                report.add(Null<Real>());
            report.add(gamma);
            report.add(Null<Real>()); // TODO
        }
    }
    report.end();
}

void SensitivityAnalysis::writeCrossGammaReport(string fileName, Real outputThreshold) {
    CSVFileReport report(fileName);

    report.addColumn("#TradeId", string());
    report.addColumn("Factor 1", string());
    report.addColumn("Factor 2", string());
    report.addColumn("Base NPV", double(), 2);
    report.addColumn("CrossGamma*Shift^2", double(), 2);
    report.addColumn("ParCrossGamma*Shift^2", double(), 2); // TODO

    for (auto data : crossGamma_) {
        string id = std::get<0>(data.first);
        string factor1 = std::get<1>(data.first);
        string factor2 = std::get<2>(data.first);
        Real crossGamma = data.second;
        Real base = baseNPV_[id];
        if (fabs(crossGamma) > outputThreshold) {
            report.next();
            report.add(id);
            report.add(factor1);
            report.add(factor2);
            report.add(base);
            report.add(crossGamma);
            report.add(Null<Real>()); // TODO
        }
    }
    report.end();
}

void SensitivityAnalysis::writeParRateSensitivityReport(string fileName) {
    CSVFileReport report(fileName);

    report.addColumn("#ParInstrumentType", string());
    report.addColumn("ParCurveName", string());
    report.addColumn("Bucket", Size());
    report.addColumn("Factor", string());
    report.addColumn("ParSensitivityVector", string());

    char sep = ',';
    LOG("Write sensitivity output to " << fileName);
    for (auto data : parRatesSensi_) {
        pair<string, string> p = data.first;
        string curveName = data.first.first;
        string factor = data.first.second;
        vector<Real> sensi = data.second;
        Size bucket = 0;
        report.next();
        report.add("YieldCurve");
        report.add(curveName);
        report.add(bucket);
        report.add(factor);
        ostringstream o;
        for (Size i = 0; i < sensi.size(); ++i)
            o << sep << sensi[i];
        report.add(o.str());
    }
    for (auto data : flatCapVolSensi_) {
        std::tuple<string, Size, string> p = data.first;
        string curveName = std::get<0>(p);
        string factor = std::get<2>(p);
        Size bucket = std::get<1>(p);
        vector<Real> sensi = data.second;
        report.next();
        report.add("CapFloor");
        report.add(curveName);
        report.add(bucket);
        report.add(factor);
        ostringstream o;
        for (Size i = 0; i < sensi.size(); ++i)
            o << sep << sensi[i];
        report.add(o.str());
    }
    report.end();
}
}
}
