/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

#include <orea/simm/utilities.hpp>
#include <orea/simm/simmconfigurationisdav1_0.hpp>
#include <orea/simm/simmconfigurationisdav1_3.hpp>
#include <orea/simm/simmconfigurationisdav1_3_38.hpp>
#include <orea/simm/simmconfigurationisdav2_0.hpp>
#include <orea/simm/simmconfigurationisdav2_1.hpp>
#include <orea/simm/simmconfigurationisdav2_2.hpp>
#include <orea/simm/simmconfigurationisdav2_3.hpp>
#include <orea/simm/simmconfigurationisdav2_3_8.hpp>
#include <orea/simm/simmconfigurationisdav2_5.hpp>
#include <orea/simm/simmconfigurationisdav2_5a.hpp>
#include <orea/simm/simmconfigurationisdav2_6.hpp>
#include <orea/simm/simmconfigurationisdav2_6_5.hpp>
#include <orea/simm/simmconfigurationisdav2_7_2412_1.hpp>
#include <orea/simm/simmconfigurationcalibration.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondoption.hpp>
#include <ored/portfolio/bondposition.hpp>
#include <ored/portfolio/bondrepo.hpp>
#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/compositetrade.hpp>
#include <ored/portfolio/convertiblebond.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/portfolio/trs.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/scriptedtrade.hpp>

#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/utilities/null.hpp>

#include <boost/make_shared.hpp>
#include <boost/regex.hpp>

#include <fstream>

using std::map;
using std::set;
using std::string;
using namespace ore::data;

namespace ore {
namespace analytics {

using QuantLib::Null;
using QuantLib::Size;

std::vector<std::string> loadFactorList(const std::string& inputFileName, const char delim) {
    LOG("Load factor list from file " << inputFileName);
    std::ifstream file;
    file.open(inputFileName);
    QL_REQUIRE(file.is_open(), "error opening file " << inputFileName);
    std::vector<string> result;
    try {
        while (!file.eof()) {
            std::string line;
            getline(file, line, delim);
            if (line.size() == 0)
                continue;
            result.push_back(line);
        }
    } catch (std::exception& e) {
        file.close();
        QL_FAIL("error during reading file: " << e.what());
    }

    LOG("Loaded factor list of size " << result.size());
    return result;
} // loadFactorList

std::vector<std::vector<double>> loadScenarios(const std::string& inputFileName, const char delim) {
    LOG("Load scenarios from file " << inputFileName);
    std::ifstream file;
    file.open(inputFileName);
    QL_REQUIRE(file.is_open(), "error opening file " << inputFileName);
    std::vector<std::vector<double>> result;
    try {
        while (!file.eof()) {
            Size currentScenario = Null<Size>(); //, currentFactor;// = Null<Size>();
            std::string line;
            getline(file, line, delim);
            if (line.size() == 0)
                continue;
            std::vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_off);
            QL_REQUIRE(tokens.size() == 3, "loadScenarios, expected 3 tokens in line: " << line);
            Size tmpSc = parseInteger(tokens[0]);
            // Size tmpFc = parseInteger(tokens[1]);
            if (currentScenario != tmpSc) {
                result.push_back(std::vector<double>());
                currentScenario = tmpSc;
            }
            // currentFactor = tmpFc;
            double move = parseReal(tokens[2]);
            result.back().push_back(move);
        }
    } catch (std::exception& e) {
        file.close();
        QL_FAIL("error during reading file: " << e.what());
    }

    LOG("Loaded " << result.size() << " scenarios, first entry contains " << result.front().size() << " factors");
    return result;
} // loadScenarios

Matrix loadCovarianceMatrix(const std::string& inputFileName, const char delim) {
    LOG("Load covariance matrix from file " << inputFileName);
    std::ifstream file;
    file.open(inputFileName);
    QL_REQUIRE(file.is_open(), "error opening file " << inputFileName);
    std::vector<std::pair<Size, Size>> pos;
    std::vector<double> val;
    Size maxI = 0, maxJ = 0;
    try {
        while (!file.eof()) {
            std::string line;
            getline(file, line, delim);
            if (line.size() == 0)
                continue;
            std::vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_off);
            QL_REQUIRE(tokens.size() == 3, "loadCovarianceMatrix, expected 3 tokens in line: " << line);
            Size i = parseInteger(tokens[0]);
            Size j = parseInteger(tokens[1]);
            double tmp = parseReal(tokens[2]);
            pos.push_back(std::make_pair(i, j));
            val.push_back(tmp);
            maxI = std::max(maxI, i);
            maxJ = std::max(maxJ, j);
        }
    } catch (std::exception& e) {
        file.close();
        QL_FAIL("error during reading file: " << e.what());
    }

    LOG("Loaded " << val.size() << " data points, dimension of matrix is " << maxI + 1 << "x" << maxJ + 1);
    QL_REQUIRE(maxI == maxJ, "Expected quadratic matrix");

    Matrix result(maxI + 1, maxI + 1);
    for (Size i = 0; i < pos.size(); ++i) {
        result[pos[i].first][pos[i].second] = val[i];
        result[pos[i].second][pos[i].first] = val[i];
    }

    // log the eigenvalues
    QuantLib::SymmetricSchurDecomposition ssd(result);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        LOG("Eigenvalue " << i << " =  " << ssd.eigenvalues()[i]);
    }
    return result;
} // load CovarianceMatrix

SimmVersion parseSimmVersion(const string& version) {
    static map<string, SimmVersion> versionMap = {{"1.0", SimmVersion::V1_0},
                                                  {"1.1", SimmVersion::V1_1},
                                                  {"1.2", SimmVersion::V1_2},
                                                  {"1.3", SimmVersion::V1_3},
                                                  {"1.3.38", SimmVersion::V1_3_38},
                                                  {"2.0", SimmVersion::V2_0},
                                                  {"2.1", SimmVersion::V2_1},
                                                  {"2.2", SimmVersion::V2_2},
                                                  {"2.3", SimmVersion::V2_3},
                                                  {"2.3.8", SimmVersion::V2_3_8},
                                                  {"2.5", SimmVersion::V2_5},
                                                  {"2.5A", SimmVersion::V2_5A},
                                                  {"2.5.6", SimmVersion::V2_6},
                                                  {"2.6.5", SimmVersion::V2_6_5},
						                          // alias
                                                  {"2.4", SimmVersion::V2_3_8},
                                                  {"2.6", SimmVersion::V2_6},
                                                  {"2.7", SimmVersion::V2_6_5},
                                                  {"2.7.2412.1", SimmVersion::V2_7_2412_1},
                                                  // old names for backwards compatibility
                                                  {"ISDA_V315", SimmVersion::V1_0},
                                                  {"ISDA_V329", SimmVersion::V1_3},
                                                  {"ISDA_V338", SimmVersion::V1_3_38},
                                                  {"ISDA_V344", SimmVersion::V2_0}};

    QL_REQUIRE(versionMap.count(version) > 0,
               "Could not parse SIMM version string " << version << " to a valid version");

    return versionMap.at(version);
}

QuantLib::ext::shared_ptr<SimmConfiguration> buildSimmConfiguration(const string& simmVersion,
                                                            const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                            const QuantLib::ext::shared_ptr<SimmCalibrationData>& simmCalibrationData,
                                                            const Size& mporDays) {

    // Check first if the SIMM calibration has the requested simmVersion
    if (simmCalibrationData) {
        const auto& simmCalibration = simmCalibrationData->getBySimmVersion(simmVersion);
        if (simmCalibration) {
            auto simmConfiguration = QuantLib::ext::make_shared<SimmConfigurationCalibration>(simmBucketMapper, simmCalibration, mporDays);
            return simmConfiguration;
        }
    }

    auto version = parseSimmVersion(simmVersion);

    switch (version) {
    case SimmVersion::V1_0:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V1_0>(simmBucketMapper);
        break;
    case SimmVersion::V1_3:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V1_3>(simmBucketMapper);
        break;
    case SimmVersion::V1_3_38:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V1_3_38>(simmBucketMapper);
        break;
    case SimmVersion::V2_0:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_0>(simmBucketMapper);
        break;
    case SimmVersion::V2_1:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_1>(simmBucketMapper);
        break;
    case SimmVersion::V2_2:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_2>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_3:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_3>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_3_8:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_3_8>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_5:
	    return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_5>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_5A:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_5A>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_6:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_6>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_6_5:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_6_5>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_7_2412_1:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_7_2412_1>(simmBucketMapper, mporDays);
        break;
    default:
        break;
    }

    QL_FAIL("SIMM configuration for version '" << simmVersion << "' cannot be built");
}

std::string escapeCommaSeparatedList(const std::string& str, const char& csvQuoteChar) {
    std::string result = str;
    if (result.find(',') != string::npos && csvQuoteChar == '\0') {
        result = '\"' + result + '\"';
    }
    return result;
}


CrifRecord::ProductClass simmProductClassFromOreTrade(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {
    auto pc = scheduleProductClassFromOreTrade(trade);
    if (pc == CrifRecord::ProductClass::Rates || pc == CrifRecord::ProductClass::FX)
        pc = CrifRecord::ProductClass::RatesFX;
    return pc;
}

namespace {

// Logic for assigning a Schedule product class for bond derivatives
template <typename BondDerivative>
CrifRecord::ProductClass
productClassBond(QuantLib::ext::shared_ptr<const BondDerivative> bondDerivative,
                 CrifRecord::ProductClass creditFreeProductClass = CrifRecord::ProductClass::Rates) {
    // From ISDA SIMM FAQ and Implementation Questions, 24 Jan 2018, Question F.1
    if (bondDerivative->bondData().hasCreditRisk() && !bondDerivative->bondData().creditCurveId().empty()) {
        return CrifRecord::ProductClass::Credit;
    } else {
        return creditFreeProductClass;
    }
}

// Logic for assigning Schedule product class for FX derivatives
template <typename FxDerivative>
CrifRecord::ProductClass productClassFX(QuantLib::ext::shared_ptr<const FxDerivative> fxDerivative) {
    // If either Bought or Sold currency is precious or crypto we return Commodity
    if (ore::data::isPseudoCurrency(fxDerivative->boughtCurrency()) ||
        ore::data::isPseudoCurrency(fxDerivative->soldCurrency())) {
        return CrifRecord::ProductClass::Commodity;
    } else {
        return CrifRecord::ProductClass::FX;
    }
}
  
// ORE trade to schedule product class mapping (this mapping differentiates between Rates and FX).
// Not all trade types are in this list, some require additional logic in scheduleProductClassFromOreTrade().
static const map<string, CrifRecord::ProductClass> tradeProductClassMap = {
    {"Ascot", CrifRecord::ProductClass::Credit},
    {"AssetBackedCreditDefaultSwap", CrifRecord::ProductClass::Credit},
    {"Autocallable_01", CrifRecord::ProductClass::Equity},
    {"BalanceGuaranteedSwap", CrifRecord::ProductClass::Rates},
    {"Bond", CrifRecord::ProductClass::Rates},
    {"BondOption", CrifRecord::ProductClass::Rates},
    {"BondRepo", CrifRecord::ProductClass::Rates},
    {"BondTRS", CrifRecord::ProductClass::Rates},
    {"CallableSwap", CrifRecord::ProductClass::Rates},
    {"CapFloor", CrifRecord::ProductClass::Rates},
    {"CashPosition", CrifRecord::ProductClass::FX},
    {"CBO", CrifRecord::ProductClass::Credit},
    {"CommodityAccumulator", CrifRecord::ProductClass::Commodity},
    {"CommodityAsianOption", CrifRecord::ProductClass::Commodity},
    {"CommodityAveragePriceOption", CrifRecord::ProductClass::Commodity},
    {"CommodityBasketOption", CrifRecord::ProductClass::Commodity},
    {"CommodityBasketVarianceSwap", CrifRecord::ProductClass::Commodity},
    {"CommodityDigitalAveragePriceOption", CrifRecord::ProductClass::Commodity},
    {"CommodityDigitalOption", CrifRecord::ProductClass::Commodity},
    {"CommodityForward", CrifRecord::ProductClass::Commodity},
    {"CommodityOption", CrifRecord::ProductClass::Commodity},
    {"CommodityOptionStrip", CrifRecord::ProductClass::Commodity},
    {"CommodityPairwiseVarianceSwap", CrifRecord::ProductClass::Commodity},
    {"CommodityPosition", CrifRecord::ProductClass::Commodity},
    {"CommodityRainbowOption", CrifRecord::ProductClass::Commodity},
    {"CommoditySpreadOption", CrifRecord::ProductClass::Commodity},
    {"CommoditySwap", CrifRecord::ProductClass::Commodity},
    {"CommoditySwaption", CrifRecord::ProductClass::Commodity},
    {"CommodityTaRF", CrifRecord::ProductClass::Commodity},
    {"CommodityVarianceSwap", CrifRecord::ProductClass::Commodity},
    {"CommodityWorstOfBasketSwap", CrifRecord::ProductClass::Commodity},
    {"ContractForDifference", CrifRecord::ProductClass::Rates},
    {"ConvertibleBond", CrifRecord::ProductClass::Rates},
    {"CreditDefaultSwap", CrifRecord::ProductClass::Credit},
    {"CreditDefaultSwapOption", CrifRecord::ProductClass::Credit},
    {"CreditLinkedSwap", CrifRecord::ProductClass::Credit},
    {"CrossCurrencySwap", CrifRecord::ProductClass::Rates},
    {"DoubleDigitalOption", CrifRecord::ProductClass::Rates},
    {"EquityAccumulator", CrifRecord::ProductClass::Equity},
    {"EquityAsianOption", CrifRecord::ProductClass::Equity},
    {"EquityAsianOption", CrifRecord::ProductClass::Equity},
    {"EquityBarrierOption", CrifRecord::ProductClass::Equity},
    {"EquityBasketOption", CrifRecord::ProductClass::Equity},
    {"EquityBasketVarianceSwap", CrifRecord::ProductClass::Equity},
    {"EquityCliquetOption", CrifRecord::ProductClass::Equity},
    {"EquityDigitalOption", CrifRecord::ProductClass::Equity},
    {"EquityDoubleBarrierOption", CrifRecord::ProductClass::Equity},
    {"EquityDoubleTouchOption", CrifRecord::ProductClass::Equity},
    {"EquityEuropeanBarrierOption", CrifRecord::ProductClass::Equity},
    {"EquityForward", CrifRecord::ProductClass::Equity},
    {"EquityFutureOption", CrifRecord::ProductClass::Equity},
    {"EquityOption", CrifRecord::ProductClass::Equity},
    {"EquityOptionPosition", CrifRecord::ProductClass::Equity},
    {"EquityOutperformanceOption", CrifRecord::ProductClass::Equity},
    {"EquityPairwiseVarianceSwap", CrifRecord::ProductClass::Equity},
    {"EquityPosition", CrifRecord::ProductClass::Equity},
    {"EquityRainbowOption", CrifRecord::ProductClass::Equity},
    {"EquitySwap", CrifRecord::ProductClass::Equity},
    {"EquityTaRF", CrifRecord::ProductClass::Equity},
    {"EquityTouchOption", CrifRecord::ProductClass::Equity},
    {"EquityVarianceSwap", CrifRecord::ProductClass::Equity},
    {"EquityWorstOfBasketSwap", CrifRecord::ProductClass::Equity},
    {"EuropeanOptionBarrier", CrifRecord::ProductClass::Equity},
    {"Failed", CrifRecord::ProductClass::Empty},
    {"FlexiSwap", CrifRecord::ProductClass::Rates},
    {"ForwardBond", CrifRecord::ProductClass::Rates},
    {"ForwardRateAgreement", CrifRecord::ProductClass::Rates},
    {"FxAccumulator", CrifRecord::ProductClass::FX},
    {"FxAsianOption", CrifRecord::ProductClass::FX},
    {"FxAverageForward", CrifRecord::ProductClass::FX},
    {"FxBarrierOption", CrifRecord::ProductClass::FX},
    {"FxBasketOption", CrifRecord::ProductClass::FX},
    {"FxBasketVarianceSwap", CrifRecord::ProductClass::FX},
    {"FxDigitalBarrierOption", CrifRecord::ProductClass::FX},
    {"FxDigitalOption", CrifRecord::ProductClass::FX},
    {"FxDoubleBarrierOption", CrifRecord::ProductClass::FX},
    {"FxDoubleTouchOption", CrifRecord::ProductClass::FX},
    {"FxEuropeanBarrierOption", CrifRecord::ProductClass::FX},
    {"FxForward", CrifRecord::ProductClass::FX},
    {"FxKIKOBarrierOption", CrifRecord::ProductClass::FX},
    {"FxOption", CrifRecord::ProductClass::FX},
    {"FxPairwiseVarianceSwap", CrifRecord::ProductClass::FX},
    {"FxRainbowOption", CrifRecord::ProductClass::FX},
    {"FxSwap", CrifRecord::ProductClass::FX},
    {"FxTaRF", CrifRecord::ProductClass::FX},
    {"FxTouchOption", CrifRecord::ProductClass::FX},
    {"FxVarianceSwap", CrifRecord::ProductClass::FX},
    {"FxWorstOfBasketSwap", CrifRecord::ProductClass::FX},
    {"IndexCreditDefaultSwap", CrifRecord::ProductClass::Credit},
    {"IndexCreditDefaultSwapOption", CrifRecord::ProductClass::Credit},
    {"InflationSwap", CrifRecord::ProductClass::Rates},
    {"MultiLegOption", CrifRecord::ProductClass::Rates},
    {"PerformanceOption_01", CrifRecord::ProductClass::Equity},
    {"RiskParticipationAgreement", CrifRecord::ProductClass::Credit},
    {"Swap", CrifRecord::ProductClass::Rates},
    {"Swaption", CrifRecord::ProductClass::Rates},
    {"SyntheticCDO", CrifRecord::ProductClass::Credit},
    {"TotalReturnSwap", CrifRecord::ProductClass::Rates}};

std::map<std::string, std::string> nonStdCcys = {{"CLF", "CLP"}, {"CNH", "CNY"}, {"COU", "CUP"}, {"CUC", "CUP"},
                                                 {"MXV", "MXN"}, {"UYI", "UYU"}, {"UYW", "UYU"}};

std::set<std::string> unidadeCcys = {"CLF", "COU", "MXV", "UYW"};

} // namespace

bool isSimmNonStandardCurrency(const std::string& ccy) { return nonStdCcys.find(ccy) != nonStdCcys.end(); }

bool isUnidadeCurrency(const std::string& ccy) { return unidadeCcys.find(ccy) != unidadeCcys.end(); }

bool isIsin(const string& s) {
    // FIXME, this is a bit too broad: Use enumeration for the first two letters? Validate checksum?
    if (s.size() != 12)
        return false;
    if (s[0] < 'A' || s[0] > 'Z' || s[1] < 'A' || s[1] > 'Z')
        return false;
    for (Size i = 2; i < 11; ++i) {
        if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9'))
            continue;
        return false;
    }
    if (s[11] < '0' || s[11] > '9')
        return false;
    return true;
    // boost::regex isinPattern("^[A-Z]{2}[A-Z0-9]{9}[0-9]{1}");
    // return boost::regex_match(s, isinPattern);
}
  
std::string simmStandardCurrency(const std::string& ccy) {
    if (auto c = nonStdCcys.find(ccy); c != nonStdCcys.end()) {
        return c->second;
    } else {
        return ccy;
    }
}

void convertToSimmStandardCurrency(double& npv, std::string& ccy, const QuantLib::ext::shared_ptr<Market> market) {
    if (!isSimmNonStandardCurrency(ccy))
        return;
    std::string target = simmStandardCurrency(ccy);
    npv *= market->fxRate(ccy + target)->value();
    ccy = target;
}

void convertToSimmStandardCurrency(std::string& ccy) { ccy = simmStandardCurrency(ccy); }

bool convertToSimmStandardCurrencyPair(std::string& ccy) {
    QL_REQUIRE(ccy.size() == 6, "convertToSimmStandardCurrencyPair: expected string of size 6, got '" << ccy << "'");
    std::string ccy1 = simmStandardCurrency(ccy.substr(0, 3));
    std::string ccy2 = simmStandardCurrency(ccy.substr(3));
    ccy = ccy1 + ccy2;
    return ccy1 != ccy2;
}
  
CrifRecord::ProductClass scheduleProductClassFromOreTrade(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {
    // Firstly check for a Product class override in the trade's additional fields
    auto additionalFields = trade->envelope().additionalFields();
    auto it = additionalFields.find("ProductClassOverride");
    if (it != additionalFields.end()) {
        return parseProductClass(it->second);
    }

    // Apply logic based on trade type
    if (QuantLib::ext::dynamic_pointer_cast<ore::data::ScriptedTrade>(trade)) {
        return parseProductClass(
            QuantLib::ext::static_pointer_cast<ore::data::ScriptedTrade>(trade)->simmProductClass());
    } else if (trade->tradeType() == "Bond") {
        return productClassBond(QuantLib::ext::dynamic_pointer_cast<const ore::data::Bond>(trade));
    } else if (trade->tradeType() == "BondPosition") {
        bool hasCreditRisk = false;
        bool hasConvertibleBond = false;
        for (auto const& b : QuantLib::ext::dynamic_pointer_cast<const ore::data::BondPosition>(trade)->bonds()) {
            hasCreditRisk = hasCreditRisk || b.hasCreditRisk;
            hasConvertibleBond = hasConvertibleBond || b.builderLabel == "ConvertibleBond";
        }
        if (hasConvertibleBond)
            return CrifRecord::ProductClass::Equity;
        else if (hasCreditRisk)
            return CrifRecord::ProductClass::Credit;
        else
            return CrifRecord::ProductClass::RatesFX;
    } else if (trade->tradeType() == "ConvertibleBond") {
        return productClassBond(QuantLib::ext::dynamic_pointer_cast<const ore::data::ConvertibleBond>(trade),
                                CrifRecord::ProductClass::Equity);
    } else if (trade->tradeType() == "BondOption") {
        return productClassBond(QuantLib::ext::dynamic_pointer_cast<const BondOption>(trade));
    } else if (trade->tradeType() == "BondTRS") {
        return productClassBond(QuantLib::ext::dynamic_pointer_cast<const BondTRS>(trade));
    } else if (trade->tradeType() == "ForwardBond") {
        return productClassBond(QuantLib::ext::dynamic_pointer_cast<const ForwardBond>(trade));
    } else if (trade->tradeType() == "BondRepo") {
        return productClassBond(QuantLib::ext::dynamic_pointer_cast<const BondRepo>(trade));
    } else if (trade->tradeType() == "FxForward") {
        // ORE FX derivatives need to be handled in turn
        return productClassFX(QuantLib::ext::dynamic_pointer_cast<const FxForward>(trade));
    } else if (trade->tradeType() == "FxOption") {
        return productClassFX(QuantLib::ext::dynamic_pointer_cast<const FxOption>(trade));
    } else if (trade->tradeType() == "FxSwap") {
        QuantLib::ext::shared_ptr<FxSwap> fxSwap(QuantLib::ext::dynamic_pointer_cast<FxSwap>(trade));
        if (isPseudoCurrency(fxSwap->nearBoughtCurrency()) || isPseudoCurrency(fxSwap->nearSoldCurrency())) {
            return CrifRecord::ProductClass::Commodity;
        } else {
            return CrifRecord::ProductClass::FX;
        }
    } else if (QuantLib::ext::dynamic_pointer_cast<const FxSingleAssetDerivative>(trade)) {
        // All ORE+ FX Single Asset Derivatives should hit this
        return productClassFX(QuantLib::ext::dynamic_pointer_cast<const FxSingleAssetDerivative>(trade));
    } else if (trade->tradeType() == "CompositeTrade") {
        QuantLib::ext::shared_ptr<CompositeTrade> compositeTrade(QuantLib::ext::dynamic_pointer_cast<CompositeTrade>(trade));
        CrifRecord::ProductClass pc = CrifRecord::ProductClass::Empty;
        for (const auto& subtrade : compositeTrade->trades()) {
            pc = SimmConfiguration::maxProductClass(pc, scheduleProductClassFromOreTrade(subtrade));
        }
        return pc;
    } else if (trade->tradeType() == "TotalReturnSwap" || trade->tradeType() == "ContractForDifference") {
        auto trs = QuantLib::ext::dynamic_pointer_cast<const ore::data::TRS>(trade);
        QL_REQUIRE(trs, "simm/scheduleProductClassFromOreTradeType: Cannot cast tradeType TotalReturnSwap to TRS");
        CrifRecord::ProductClass result = CrifRecord::ProductClass::Empty;
        for (auto const& u : trs->underlying()) {
            result = SimmConfiguration::maxProductClass(result, scheduleProductClassFromOreTrade(u));
        }
        return result;
    } else {
        if (auto it = tradeProductClassMap.find(trade->tradeType()); it != tradeProductClassMap.end())
            return it->second;
        else {
            QL_FAIL("simm/scheduleProductClassFromOrePlusTrade: tradeType '" << trade->tradeType()
                                                                             << "' not recognised");
        }
    }
}

  
} // namespace analytics
} // namespace ore
