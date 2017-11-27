/*
  Copyright (C) 2017 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <boost/make_shared.hpp>

#include <orea/engine/sensitivitydata.hpp>

#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/errors.hpp>
#include <ql/utilities/null.hpp>

using QuantLib::Null;

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

boost::shared_ptr<RiskFactorKey> parseRiskFactorKey(const std::string& str, std::vector<std::string>& addTokens) {
    std::vector<std::string> tokens;
    boost::split(tokens, str, boost::is_any_of("/"), boost::token_compress_off);
    QL_REQUIRE(tokens.size() >= 3, "parseRiskFactorKey: at least 3 tokens required, string is \"" << str << "\"");
    RiskFactorKey::KeyType type = parseRiskFactorKeyType(tokens[0]);
    boost::shared_ptr<RiskFactorKey> key =
        boost::make_shared<RiskFactorKey>(type, tokens[1], ore::data::parseInteger(tokens[2]));
    for (Size i = 3; i < tokens.size(); ++i) {
        addTokens.emplace_back(tokens[i]);
    }
    return key;
} // parseRiskFactorKey

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

bool SensitivityDataInMemory::next() {
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

} // namespace analytics
} // namespace oreplus
