/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>

#include <orea/engine/sensitivitydata.hpp>

#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/errors.hpp>
#include <ql/utilities/null.hpp>

using QuantLib::Null;
using namespace ore::data;
using namespace std;

namespace ore {
namespace analytics {

namespace {

template <class T> T getData(const std::vector<T>& data, const Size index) {
    QL_REQUIRE(index > 0, "index is still zero, need call to next()");
    QL_REQUIRE(index <= data.size(),
               "index " << index << " is bigger than available data size (" << data.size() << ")");
    return data.at(index - 1);
} // getData

} // namespace

void SensitivityDataInMemory::add(const std::string& tradeId, const std::string& factor, const std::string& factor2,
                                  const double value, const double value2) {

    std::vector<std::string> addTokens1, addTokens2;
    boost::shared_ptr<RiskFactorKey> key1 = parseRiskFactorKey(factor, addTokens1);
    boost::shared_ptr<RiskFactorKey> key2 = factor2 == "" ? nullptr : parseRiskFactorKey(factor2, addTokens2);

    tradeId_.emplace_back(tradeId);
    addTokens1_.emplace_back(addTokens1);
    addTokens2_.emplace_back(addTokens2);
    keys1_.emplace_back(key1);
    keys2_.emplace_back(key2);
    value_.push_back(value);
    value2_.push_back(value2);
};

bool SensitivityDataInMemory::next() const {
    if (index_ < tradeId_.size()) {
        ++index_;
        return true;
    }
    return false;
}

void SensitivityDataInMemory::reset() { index_ = 0; }

std::string SensitivityDataInMemory::tradeId() const { return getData(tradeId_, index_); }

bool SensitivityDataInMemory::isCrossGamma() const { return getData(keys2_, index_) != nullptr; }

boost::shared_ptr<RiskFactorKey> SensitivityDataInMemory::factor1() const { return getData(keys1_, index_); }
boost::shared_ptr<RiskFactorKey> SensitivityDataInMemory::factor2() const { return getData(keys2_, index_); }

std::vector<std::string> SensitivityDataInMemory::additionalTokens1() const { return getData(addTokens1_, index_); }
std::vector<std::string> SensitivityDataInMemory::additionalTokens2() const { return getData(addTokens2_, index_); }

double SensitivityDataInMemory::value() const { return getData(value_, index_); }
double SensitivityDataInMemory::value2() const { return getData(value2_, index_); }

bool SensitivityDataInMemory::hasFactor(const RiskFactorKey& key) const {
    for (auto const& k : keys1_) {
        if (k != nullptr && key == *k)
            return true;
    }
    for (auto const& k : keys2_) {
        if (k != nullptr && key == *k)
            return true;
    }
    return false;
}

void loadSensitivityDataFromCsv(SensitivityDataInMemory& data, const std::string& fileName, const char delim) {
    LOG("Load Sensitivity Data from file " << fileName);
    ore::data::CSVFileReader reader(fileName, true);
    bool delta = reader.hasField("#TradeId") && reader.hasField("Factor") &&
                 (reader.hasField("Delta") || reader.hasField("ParDelta"));
    bool gamma = reader.hasField("#TradeId") && reader.hasField("Factor 1") && reader.hasField("Factor 2") &&
                 (reader.hasField("CrossGamma") || reader.hasField("ParCrossGamma"));
    QL_REQUIRE(delta || gamma,
               "loadSensitivityDataFromCsv: file " << fileName << " not recognised as either delta or crossgamma file");
    std::string valueField, valueField2;
    if (delta) {
        valueField = reader.hasField("ParDelta") ? "ParDelta" : "Delta";
        valueField2 = reader.hasField("ParGamma") ? "ParGamma" : "Gamma";
    } else
        valueField = reader.hasField("ParCrossGamma") ? "ParCrossGamma" : "CrossGamma";
    Size errorLines = 0, validLines = 0;
    while (reader.next()) {
        try {
            if (delta) {
                data.add(reader.get("#TradeId"), reader.get("Factor"), "", ore::data::parseReal(reader.get(valueField)),
                         ore::data::parseReal(reader.get(valueField2)));
            } else if (gamma) {
                data.add(reader.get("#TradeId"), reader.get("Factor 1"), reader.get("Factor 2"),
                         ore::data::parseReal(reader.get(valueField)), Null<Real>());
            }
            ++validLines;
        } catch (const std::exception& e) {
            ++errorLines;
            WLOG("skipping line " << reader.currentLine() << ": " << e.what());
        }
    }
    LOG("Read " << validLines << " valid data lines, skipped " << errorLines << " invalid data lines in file "
                << fileName);
} // loadSensitivityDataFromCsv

void loadMappingTableFromCsv(std::map<string, string>& data, const std::string& fileName, const char delim) {
    LOG("Load Mapping Data from file " << fileName);
    ore::data::CSVFileReader reader(fileName, false);
    Size count = 0;
    while (reader.next()) {
        data[reader.get(0)] = reader.get(1);
        ++count;
    }
    LOG("Read " << count << " valid data lines in file " << fileName);
} // loadMappingTableFromCsv

SensitivityDataInMemory sensitivityDataFromParReport(InMemoryReport& parSensiReport) {
    
    SensitivityDataInMemory sensitivityData;

    // Get number of columns and rows in the report
    auto numColumns = parSensiReport.columns();
    auto numRows = parSensiReport.data(0).size();

    // Check that we have a par sensi report
    QL_REQUIRE(numColumns == 5, "Par sensitivity report should have five columns");
    QL_REQUIRE(parSensiReport.header(0) == "TradeId", "Par sensitivity report's first column should be TradeId");
    QL_REQUIRE(parSensiReport.header(1) == "Factor", "Par sensitivity report's second column should be Factor");
    QL_REQUIRE(parSensiReport.header(3) == "ParDelta", "Par sensitivity report's fourth column should be ParDelta");
    QL_REQUIRE(parSensiReport.header(4) == "ParGamma", "Par sensitivity report's fifth column should be ParGamma");

    // Fill sensitivity data object
    for (Size i = 0; i < numRows; i++) {
        string tradeId = boost::get<string>(parSensiReport.data(0)[i]);
        string factor = boost::get<string>(parSensiReport.data(1)[i]);
        Real parDelta = boost::get<Real>(parSensiReport.data(3)[i]);
        Real parGamma = boost::get<Real>(parSensiReport.data(4)[i]);
        sensitivityData.add(tradeId, factor, "", parDelta, parGamma);
    }

    return sensitivityData;
}

map<string, string> mappingFromReport(InMemoryReport& report) {
    
    map<string, string> mapping;

    // Get number of columns and rows in the report
    auto numColumns = report.columns();
    QL_REQUIRE(numColumns == 2, "Mapping report should have two columns");
    auto numRows = report.data(0).size();

    // If no data, return empty map
    if (numRows == 0) {
        return mapping;
    }

    // Fill the map
    for (Size i = 0; i < numRows; i++) {
        string key = boost::get<string>(report.data(0)[i]);
        string value = boost::get<string>(report.data(1)[i]);
        mapping[key] = value;
    }

    return mapping;
}

} // namespace analytics
} // namespace ore
