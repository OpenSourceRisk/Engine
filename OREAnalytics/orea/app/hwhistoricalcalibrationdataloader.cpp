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

#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>

#include <sstream>
#include <vector>
#include <map>
#include <string>


using namespace QuantLib;
using namespace ore::data;

namespace ore {
namespace analytics {

HwHistoricalCalibrationDataLoader::HwHistoricalCalibrationDataLoader(const std::string& baseCurrency,
                                                                     const std::vector<std::string>& foreignCurrency,
                                                                     const std::vector<Period>& curveTenors,
                                                                     const Date& startDate, const Date& endDate)
    : baseCurrency_(baseCurrency), foreignCurrency_(foreignCurrency), tenors_(curveTenors), startDate_(startDate),
      endDate_(endDate) {}

void HwHistoricalCalibrationDataLoader::loadHistoricalCurveDataFromCsv(const std::string& fileName) {
    LOG("Load Historical time series data from file " << fileName);
    CSVFileReader dataReader(fileName, true);
    QL_REQUIRE(dataReader.hasField("date"), "Date column is not found in the time series data file.");
    QL_REQUIRE(dataReader.hasField("curve"), "Curve column is not found in the time series data file.");
    QL_REQUIRE(dataReader.hasField("data"), "Data column is not found in the time series data file.");
    Size lines = 0;
    std::vector<std::string> remove_chars = {"\"", "[", "]", " "};
    while (dataReader.next()) {
        ++lines;
        Date d = parseDate(dataReader.get("date"));
        string curveId = dataReader.get("curve");
        string dataStr = dataReader.get("data");
        // Remove all irrelavent chars in the data string
        for (Size i = 0; i < remove_chars.size(); i++) {
            boost::erase_all(dataStr, remove_chars[i]);
        }
        std::vector<std::string> tokens;
        boost::split(tokens, dataStr, boost::is_any_of(",;\t"), boost::token_compress_off);
        if (tokens.size() != tenors_.size()) {
            ALOG("Number of discount rates in line " << lines << " is not the same as tenors given.");
            continue;
        }
        std::vector<Real> discountFactor;
        for (Size i = 0; i < tenors_.size(); i++) {
            discountFactor.push_back(parseReal(tokens[i]));
        }
        loadIr(curveId, d, discountFactor);
    }
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

void HwHistoricalCalibrationDataLoader::loadFixings(const std::string& fileName) {
    LOG("Load fixings from file " << fileName);
    CSVFileReader dataReader(fileName, false);
    dataReader.next();
    QL_REQUIRE(dataReader.numberOfColumns() == 3, "Number of columns in fixings file must be 3.");
    Size lines = 0;
    // std::vector<std::string> remove_chars = { "\"", "[", "]", " " };
    while (dataReader.next()) {
        ++lines;
        Date d = parseDate(dataReader.get(0));
        string curveId = dataReader.get(1);
        Real dataReal = parseReal(dataReader.get(2));
        std::vector<std::string> tokens;
        boost::split(tokens, curveId, boost::is_any_of("-"), boost::token_compress_off);
        if (tokens[0] == "FX")
            loadFx(curveId, d, dataReal);
    }
}

void HwHistoricalCalibrationDataLoader::loadIr(const std::string& curveId, const Date& d, const std::vector<Real>& df) {
    // Check if the date is within the start and end date specified in ore.xml
    if (!(d >= startDate_ && d <= endDate_)) {
        return;
    }
    if (irCurves_.find(curveId) != irCurves_.end()) {
        if (irCurves_[curveId].find(d) != irCurves_[curveId].end()) {
            ALOG("Encounter duplicated records for curveId " << curveId << ", date " << d << " in the input file.");
            return;
        }
    }
    irCurves_[curveId][d] = df;
}

void HwHistoricalCalibrationDataLoader::loadFx(const std::string& curveId, const Date& d, const Real& fxSpot) {
    // Check if the date is within the start and end date specified in ore.xml
    if (!(d >= startDate_ && d <= endDate_)) {
        return;
    }
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
    std::vector<string> erase_list;
    for (auto const& outer : irCurves_) {
        std::vector<std::string> tokens;
        boost::split(tokens, outer.first, boost::is_any_of("/-"), boost::token_compress_off);
        QL_REQUIRE(tokens.size() >= 2, "Curve should be in the format of IndexCurve/CCY-NAME-TENOR or IndexCurve/CCY");
        auto position = std::find(requiredCcy.begin(), requiredCcy.end(), tokens[1]);
        if (position == requiredCcy.end()) {
            erase_list.push_back(outer.first);
        } else {
            requiredCcy.erase(position);
            // Change map key to currency code
            auto handler = irCurves_.extract(outer.first);
            handler.key() = tokens[1];
            irCurves_.insert(std::move(handler));
        }
    }
    // Remove all the currencies that are not needed
    for (auto const& index : erase_list) {
        irCurves_.erase(index);
    }
    std::string missingCcy;
    for (auto& c : requiredCcy)
        missingCcy += c + " ";
    QL_REQUIRE(requiredCcy.size() == 0, "Discount factor for " << missingCcy << "are not found in input file.");

    // Check if all required fx spot exist in fxSpots
    requiredCcy = foreignCurrency_;
    erase_list.clear();
    for (auto const& outer : fxSpots_) {
        // Extract currency pair name
        std::vector<std::string> tokens;
        boost::split(tokens, outer.first, boost::is_any_of("/-"), boost::token_compress_off);
        QL_REQUIRE(tokens.size() >= 4, "Curve should be in the format of FX-NAME-CCY1-CCY2");
        string ccyPair;
        if (tokens[2] == baseCurrency_) {
            auto position = std::find(requiredCcy.begin(), requiredCcy.end(), tokens[3]);
            if (position != requiredCcy.end()) {
                ccyPair = tokens[3] + tokens[2];
                // Inverse the fx spot rates
                for (const auto& inner : outer.second)
                    fxSpots_[outer.first][inner.first] = 1.0 / inner.second;
                requiredCcy.erase(position);
            } else {
                erase_list.push_back(outer.first);
                continue;
            }
        } else if (tokens[3] == baseCurrency_) {
            auto position = std::find(requiredCcy.begin(), requiredCcy.end(), tokens[2]);
            if (position != requiredCcy.end()) {
                ccyPair = tokens[2] + tokens[3];
                requiredCcy.erase(position);
            } else {
                erase_list.push_back(outer.first);
                continue;
            }
        } else {
            erase_list.push_back(outer.first);
            continue;
        }
        // Change map key to currency pair code
        auto handler = fxSpots_.extract(outer.first);
        handler.key() = ccyPair;
        fxSpots_.insert(std::move(handler));
    }
    // Remove all the currencies that are not needed
    for (auto const& index : erase_list) {
        fxSpots_.erase(index);
    }
    missingCcy = "";
    for (auto& c : requiredCcy)
        missingCcy += c + "-" + baseCurrency_ + " ";
    QL_REQUIRE(requiredCcy.size() == 0, "FX spot for " << missingCcy << "are not found in input file.");

    // Master date set
    std::set<Date> masterDates;
    for (auto const& outer : fxSpots_)
        for (auto const& inner : outer.second)
            masterDates.insert(inner.first);
    for (auto const& outer : irCurves_)
        for (auto const& inner : outer.second)
            masterDates.insert(inner.first);

    // Forward-fill missing scalar (FX) and vector (IR) series
    for (auto& outer : fxSpots_) {
        //fillScalar(kv.second);
        if (outer.second.empty())
            continue;
        Real first = outer.second.begin()->second;
        Real last = first;
        for (auto d : masterDates) {
            auto it = outer.second.find(d);
            if (it == outer.second.end()) {
                outer.second[d] = (d < outer.second.begin()->first ? first : last);
                LOG("Add missing fx spot rate on date " << d << ", Currency: " << outer.first
                    << ", Value: " << outer.second[d]);
            } else {
                last = it->second;
            }
        }
    }
    for (auto& outer : irCurves_) {
        if (outer.second.empty())
            continue;
        std::vector<Real> first = outer.second.begin()->second;
        std::vector<Real> last = first;
        for (auto d : masterDates) {
            auto it = outer.second.find(d);
            if (it == outer.second.end()) {
                outer.second[d] = (d < outer.second.begin()->first ? first : last);
                std::ostringstream oss;
                for (Size k = 0; k < outer.second[d].size(); ++k) {
                    if (k > 0)
                        oss << ", ";
                    oss << outer.second[d][k];
                }
                LOG("Add missing IR curve rate on date " << d << ", Currency: " << outer.first
                                                         << ", Value: " << oss.str());
            } else {
                last = it->second;
            }
        }
    }
}

} // namespace analytics
} // namespace ore