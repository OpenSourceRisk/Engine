/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/app/hwhistoricalcalibrationdataloader.hpp>
#include <orea/scenario/scenariofilereader.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/scenarioloader.hpp>

#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>

#include <sstream>
#include <vector>
#include <map>
#include <string>

namespace ore {
namespace analytics {

using namespace QuantLib;
using namespace ore::data;

HwHistoricalCalibrationDataLoader::HwHistoricalCalibrationDataLoader(const std::string& baseCurrency,
                                                                     const std::vector<std::string>& foreignCurrency,
                                                                     const std::vector<Period>& curveTenors,
                                                                     const Date& startDate, const Date& endDate)
    : baseCurrency_(baseCurrency), foreignCurrency_(foreignCurrency), tenors_(curveTenors), startDate_(startDate),
      endDate_(endDate) {}

void HwHistoricalCalibrationDataLoader::loadFromScenarioFile(const std::string& fileName) {
    LOG("Load Historical time series data from scenario file " << fileName);

    ext::shared_ptr<SimpleScenarioFactory> scenarioFactory = ext::make_shared<SimpleScenarioFactory>(false);
    ext::shared_ptr<ScenarioFileReader> scenarioReader =
        ext::make_shared<ScenarioFileReader>(fileName, scenarioFactory);
    ext::shared_ptr<HistoricalScenarioLoader> historicalScenarioLoader =
        ext::make_shared<HistoricalScenarioLoader>(scenarioReader, startDate_, endDate_, NullCalendar());

    QL_REQUIRE(historicalScenarioLoader->scenarios().size() == 1,
               "Only one scenario allowed for HW historical calibration.");
    std::map<Date, ext::shared_ptr<Scenario>> scenarioMap = historicalScenarioLoader->scenarios()[0];
    QL_REQUIRE(scenarioMap.size() > 0, "Scenario file is empty.");
    const auto& keys = scenarioMap.begin()->second->keys();
    for (const auto& [date, scenario] : scenarioMap) {
        for (const auto& key : keys) {
            if (key.keytype == RiskFactorKey::KeyType::IndexCurve) {
                std::string ccy = parseCurrency(key.name);
                if (ccy == baseCurrency_ ||
                    std::find(foreignCurrency_.begin(), foreignCurrency_.end(), ccy) != foreignCurrency_.end()) {
                    loadIr(key.name, key.index, date, scenario->get(key));
                }
            } else if (key.keytype == RiskFactorKey::KeyType::FXSpot) {
                for (const auto& foreignCcy : foreignCurrency_) {
                    std::string pair1 = foreignCcy + baseCurrency_;
                    std::string pair2 = baseCurrency_ + foreignCcy;
                    Real fxRate;
                    if (key.name == pair1)
                        fxRate = scenario->get(key);
                    else if (key.name == pair2)
                        fxRate = 1.0 / scenario->get(key);
                    else
                        continue;
                    loadFx(pair1, date, fxRate);
                    break;
                }
            }
        }
    }
    cleanData();
}

void HwHistoricalCalibrationDataLoader::loadPCAFromCsv(const std::vector<std::string>& fileNames) {
    for (const auto& fileName : fileNames) {
        LOG("Load PCA data from file " << fileName);
        CSVFileReader dataReader(fileName, false);
        dataReader.next();
        QL_REQUIRE(dataReader.numberOfColumns() == tenors_.size() + 1,
                   "Number of columns in pca file must be number of tenor + 1.");
        QL_REQUIRE(dataReader.get(0) == "EigenValue", "EigenValue column must be the first column in the data file.");
        QL_REQUIRE(dataReader.get(1) == "EigenVector", "EigenVector column must exist in the data file.");
        QL_REQUIRE(dataReader.get(2) == "Currency", "Currency column must exist in the data file.");
        std::string ccy = dataReader.get(3);
        Size lines = 0;
        // std::vector<std::string> remove_chars = { "\"", "[", "]", " " };
        vector<Real> eig_val;
        vector<vector<Real>> eig_vec;
        while (dataReader.next()) {
            ++lines;
            vector<Real> eig_vec_i;
            eig_val.push_back(parseReal(dataReader.get(0)));
            for (Size i = 0; i < tenors_.size(); i++) {
                eig_vec_i.push_back(parseReal(dataReader.get(i + 1)));
            }
            eig_vec.push_back(eig_vec_i);
        }
        Array eigenValue = Array(lines, 0.0);
        Matrix eigenVector = Matrix(lines, tenors_.size(), 0.0);
        for (Size i = 0; i < lines; i++) {
            eigenValue[i] = eig_val[i];
            for (Size j = 0; j < tenors_.size(); j++) {
                eigenVector[i][j] = eig_vec[i][j];
            }
        }
        loadEigenValue(ccy, eigenValue);
        loadEigenVector(ccy, eigenVector);
    }
}

void HwHistoricalCalibrationDataLoader::loadIr(const std::string& curveId, const Size& index, const Date& d,
                                                      const Real& df) {
    std::map<Date, std::vector<Real>>& dateMap = irCurves_[curveId];
    std::vector<Real>& discountFactors = dateMap[d];
    if (discountFactors.empty()) {
        discountFactors.resize(tenors_.size(), 0.0);
    }
    QL_REQUIRE(index < tenors_.size(), "Tenor index " << index << " out of range for curve " << curveId << " on date "
                                                      << d << " (max: " << tenors_.size() << ")");
    discountFactors[index] = df;
}

void HwHistoricalCalibrationDataLoader::loadFx(const std::string& curveId, const Date& d, const Real& fxSpot) {
    // Check if the date is within the start and end date specified in ore.xml
    if (fxSpots_.find(curveId) != fxSpots_.end()) {
        if (fxSpots_[curveId].find(d) != fxSpots_[curveId].end()) {
            ALOG("Encounter duplicated records for curveId " << curveId << ", date " << d << " in the input file.");
            return;
        }
    }
    fxSpots_[curveId][d] = fxSpot;
}

void HwHistoricalCalibrationDataLoader::loadEigenValue(const std::string& ccy, const Array& eigenValue) {
    eigenValue_[ccy] = eigenValue;
    principalComponent_[ccy] = eigenValue.size();
}

void HwHistoricalCalibrationDataLoader::loadEigenVector(const std::string& ccy, const Matrix& eigenVector) {
    eigenVector_[ccy] = transpose(eigenVector);
}

void HwHistoricalCalibrationDataLoader::cleanData() {
    // Check if all required currency exist in irCurves_
    std::vector<std::string> requiredCcy = foreignCurrency_;
    requiredCcy.push_back(baseCurrency_);

    for (const auto& [curveId, dataMap] : irCurves_) {
        auto position = std::find(requiredCcy.begin(), requiredCcy.end(), parseCurrency(curveId));
        if (position != requiredCcy.end()) {
            requiredCcy.erase(position);
        }
    }

    std::string missingCcy;
    for (auto& c : requiredCcy)
        missingCcy += c + " ";
    QL_REQUIRE(requiredCcy.size() == 0, "Discount factor for " << missingCcy << "are not found in input file.");

    // Check if all required fx spot exist in fxSpots
    std::vector<std::string> missingFxPairs;
    for (const auto& foreignCcy : foreignCurrency_) {
        std::string expectedPair = foreignCcy + baseCurrency_;

        if (fxSpots_.find(expectedPair) == fxSpots_.end()) {
            missingFxPairs.push_back(expectedPair);
        }
    }
    missingCcy = "";
    for (auto& c : missingFxPairs)
        missingCcy += c + " ";
    QL_REQUIRE(missingFxPairs.size() == 0, "FX spot for " << missingCcy << "are not found in input file.");
}

std::string HwHistoricalCalibrationDataLoader::parseCurrency(const std::string& curveId) {
    vector<string> tokens;
    split(tokens, curveId, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3,
               "Two or three tokens required in " << curveId << ": CCY-INDEX or CCY-INDEX-TERM");
    return tokens[0];
}

} // namespace analytics
} // namespace ore
