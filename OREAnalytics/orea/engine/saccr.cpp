/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/equityposition.hpp>
#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/trs.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/fxderivative.hpp>
#include <orea/engine/saccr.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/fxtouchoption.hpp>

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/instruments/commodityforward.hpp>
#include <qle/instruments/currencyswap.hpp>
#include <qle/instruments/fxforward.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/currency.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <ql/tuple.hpp>

#include <boost/regex.hpp>

#include <cmath>
#include <algorithm>
#include <iomanip>

using namespace QuantLib;
using std::make_pair;
using std::map;
using std::max;
using std::pair;
using std::set;
using std::string;
using std::vector;
using QuantLib::Null;
using ore::data::LegType;

namespace {
bool isCrossCurrencySwap(const QuantLib::ext::shared_ptr<ore::data::Trade> trade) {
    bool isXCCYSwap = false;
    if (trade->legCurrencies().size() > 0) {
        string ccy1 = trade->legCurrencies().front();
        for (Size j = 1; j < trade->legCurrencies().size(); j++) {
            string ccy2 = trade->legCurrencies()[j];
            if (ccy2 != ccy1) {
                isXCCYSwap = true;
                break;
            }
        }
    }
    return isXCCYSwap;
}

bool isBasisSwap(const QuantLib::ext::shared_ptr<ore::data::Trade> trade) {
    Size floatingLegCount = 0;
    vector<ore::data::LegData> legData;
    QL_REQUIRE(trade->tradeType() == "Swap" || trade->tradeType() == "Swaption",
               "isBasisSwap: Trade type " << trade->tradeType() << " not supported for IR");

    if (trade->tradeType() == "Swap") {
        auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
        QL_REQUIRE(swap, "Trade cast to Swap failed");
        if (swap)
            legData = swap->legData();
    } else {
        auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade);
        QL_REQUIRE(swaption, "Trade cast to Swaption failed");
        if (swaption)
            legData = swaption->legData();
    }

    for (const auto& l : legData) {
        if (l.legType() == LegType::Floating)
            floatingLegCount += 1;
    }

    QL_REQUIRE(floatingLegCount <= 2, "Swaps with more than two floating legs are not supported");
    
    return floatingLegCount == 2;
}

void getFxCurrencies(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, std::string& boughtCcy, std::string& soldCcy) {

    QuantLib::ext::shared_ptr<ore::data::FxForward> fxFwd = QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
    QuantLib::ext::shared_ptr<ore::data::FxOption> fxOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade);
    QuantLib::ext::shared_ptr<ore::data::FxSingleAssetDerivative> fxDer =
        QuantLib::ext::dynamic_pointer_cast<ore::data::FxSingleAssetDerivative>(trade);

    if (fxFwd) {
        boughtCcy = fxFwd->boughtCurrency();
        soldCcy = fxFwd->soldCurrency();
    } else if (fxOpt) {
        boughtCcy = fxOpt->boughtCurrency();
        soldCcy = fxOpt->soldCurrency();
    } else if (fxDer) {
        boughtCcy = fxDer->boughtCurrency();
        soldCcy = fxDer->soldCurrency();
    } else {
        QL_FAIL("getFxCurrencies: unsupported fx trade");
    }
}

Real getOptionStrike(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, const bool flipTrade = false) {

    if (auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade)) {
        QL_REQUIRE(swaption->optionData().style() != "Bermudan",
                   "getOptionStrike: Bermudan swaptions not currently supported");
        
        const auto& ar = swaption->instrument()->additionalResults();
        
        // Get strike data from additional results
        if (ar.find("strike") != ar.end()) {
            return boost::any_cast<Real>(ar.at("strike"));
        
        // Otherwise fallback to rate provided in the fixed leg
        } else {
            for (const auto& l : swaption->legData()) {
                if (l.legType() == LegType::Fixed) {
                    auto fixedLeg = QuantLib::ext::dynamic_pointer_cast<ore::data::FixedLegData>(l.concreteLegData());
                    if (fixedLeg && fixedLeg->rates().size() == 1)
                        return fixedLeg->rates()[0];
                    else
                        QL_FAIL("SACCR::getOptionStrike: Could not find strike for Swaption");
                }
            }
        }
    } else if (auto fxOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade)) {
        const Real boughtAmount = flipTrade ? fxOpt->soldAmount() : fxOpt->boughtAmount();
        const Real soldAmount = flipTrade ? fxOpt->boughtAmount() : fxOpt->soldAmount();
        return soldAmount / boughtAmount;
    } else if (auto fxBar = QuantLib::ext::dynamic_pointer_cast<ore::data::FxBarrierOption>(trade)) {
        const Real boughtAmount = flipTrade ? fxBar->soldAmount() : fxBar->boughtAmount();
        const Real soldAmount = flipTrade ? fxBar->boughtAmount() : fxBar->soldAmount();
        return soldAmount / boughtAmount;
    } else if (auto fxTou = QuantLib::ext::dynamic_pointer_cast<ore::data::FxTouchOption>(trade)) {
        return fxTou->barrier().levels().at(0).value();
    } else if (auto vanillaOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::VanillaOptionTrade>(trade)) {
        const auto& ar = vanillaOpt->instrument()->additionalResults();
        if (ar.find("strike") != ar.end())
            return boost::any_cast<Real>(ar.at("strike"));

        return vanillaOpt->strike().value();

    } else if (auto equityOpPosition = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOptionPosition>(trade)) {
        return equityOpPosition->data().underlyings().front().strike();
    }
    QL_FAIL("SACCR::getOptionStrike(): unsupported option trade type " << trade->tradeType() << ", trade ID "
                                                                       << trade->id());
}

Real getOptionPrice(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade) {
    map<string, QuantLib::ext::any> addResults;
    auto vanillaOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::VanillaOptionTrade>(trade);

    if (!vanillaOpt) {
        if (auto equityOpPosition = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOptionPosition>(trade))
            addResults = equityOpPosition->options().front()->additionalResults();
        else
            QL_FAIL("SACCR::getOptionPrice(): unsupported option trade type " << trade->tradeType() << ", trade ID " << trade->id());
    } else {
        addResults = vanillaOpt->instrument()->additionalResults();
    }
    
    if (addResults.find("forward") != addResults.end())
        return boost::any_cast<Real>(addResults.at("forward"));
    else
        return Null<Real>();
}

Real getStartDate(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, Date today, const DayCounter& dc) {
    Real S = 0.0;
    Date minLegStartDate = Date::maxDate();
    
    auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
    auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade);

    vector<QuantLib::Leg> legs;
    if (swap)
        legs = swap->legs();
    else if (swaption)
        legs = swaption->legs();
    else
        QL_FAIL("getStartDate: Unsupported trade type " << trade->tradeType());
    

    for (const auto& l : legs) {
        if (!l.empty())
            minLegStartDate = std::min(minLegStartDate, l.front()->date());
    }
    if (minLegStartDate > today)
        S = dc.yearFraction(today, minLegStartDate);

    return S;
}

Real getEndDate(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, Date today, const DayCounter& dc) {
    Real E = 0.0;
    Date maxLegEndDate = Date::minDate();

    auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
    auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade);

    vector<QuantLib::Leg> legs;
    if (swap)
        legs = swap->legs();
    else if (swaption)
        legs = swaption->legs();
    else
        QL_FAIL("getStartDate: Unsupported trade type " << trade->tradeType());

    for (const auto& l : legs) {
        if (!l.empty())
            maxLegEndDate = std::max(maxLegEndDate, l.back()->date());
    }
    // Trade should be matured if this condition is not true, but we include it here just in case
    if (maxLegEndDate > today)
        E = dc.yearFraction(today, maxLegEndDate);

    return E;
}

Real getLatestExpiryTime(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, Date today, const DayCounter& dc) {

    ore::data::OptionData optionData;

    if (auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade)) {
        optionData = swaption->optionData();
    } else if (auto fxOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade)) {
        optionData = fxOpt->option();
    } else if (auto fxBar = QuantLib::ext::dynamic_pointer_cast<ore::data::FxBarrierOption>(trade)) {
        optionData = fxBar->option();
    } else if (auto fxTou = QuantLib::ext::dynamic_pointer_cast<ore::data::FxTouchOption>(trade)) {
        optionData = fxTou->option();
    } else if (auto vanillaOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::VanillaOptionTrade>(trade)) {
        optionData = vanillaOpt->option();
    } else if (trade->tradeType() == "TotalReturnSwap") {
        const auto& trs = QuantLib::ext::dynamic_pointer_cast<ore::data::TRS>(trade);
        QL_REQUIRE(trs->underlying().size() == 1, "Currently only 1 underlying supported.");
        const auto& underlyingTrade = trs->underlying().front();
        if (underlyingTrade->tradeType() == "EquityPosition") {
            return QuantLib::Null<Real>();
        } else if (underlyingTrade->tradeType() == "EquityOptionPosition") {
            const auto& equityOpPosition =
                QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOptionPosition>(underlyingTrade);
            QL_REQUIRE(equityOpPosition->data().underlyings().size() == 1, "getLatestExpiryTime(): Currently only 1 underlying supported");
            optionData = equityOpPosition->data().underlyings().front().optionData();
        } else {
            QL_FAIL("Only EquityPosition and EquityOptionPosition underlying trade types supported for Equity TRS. Got "
                    << underlyingTrade->tradeType());
        }
    } else if (trade->tradeType() == "FxForward" || trade->tradeType() == "CommoditySwap" ||
               trade->tradeType() == "CommodityForward" || trade->tradeType() == "Swap") {
        return QuantLib::Null<Real>();
    } else {
        QL_FAIL("SACCR::getLatestExpiryTime() does not support trade type " << trade->tradeType());
    }

    Date LatestExpiryDate = Date::minDate();
    for (auto d : optionData.exerciseDates()) {
        LatestExpiryDate = std::max(LatestExpiryDate, ore::data::parseDate(d));
    }

    return LatestExpiryDate <= today ? 0.0 : dc.yearFraction(today, LatestExpiryDate);
}

pair<Real, Real> getOptionType(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade, bool flipTrade) {

    ore::data::OptionData optionData;

    if (auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade)) {
        optionData = swaption->optionData();
    } else if (auto fxOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade)) {
        optionData = fxOpt->option();
    } else if (auto fxBar = QuantLib::ext::dynamic_pointer_cast<ore::data::FxBarrierOption>(trade)) {
        optionData = fxBar->option();
    } else if (auto fxTou = QuantLib::ext::dynamic_pointer_cast<ore::data::FxTouchOption>(trade)) {
        optionData = fxTou->option();
    } else if (auto equityOption = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOption>(trade)) {
        optionData = equityOption->option();
    } else if (auto equityOpPosition = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOptionPosition>(trade)) {
        optionData = equityOpPosition->data().underlyings().front().optionData();
    } else {
        QL_FAIL("getOptionType: unsupported option trade " << trade->id());
    }

    const Option::Type type = ore::data::parseOptionType(optionData.callPut());
    Real callPut = (type == Option::Type::Call) ? 1 : -1;
    if (flipTrade)
        callPut *= -1;
    const Position::Type positionType = ore::data::parsePositionType(optionData.longShort());
    Real boughtSold = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    return std::make_pair(callPut, boughtSold);
}

Real phi(Real P, Real K, Real T, Real sigma, Real callPut) {
    QL_REQUIRE(P != QuantLib::Null<Real>(), "phi(): P cannot be null");
    QL_REQUIRE(K != QuantLib::Null<Real>(), "phi(): K cannot be null");
    QL_REQUIRE(!close_enough(K, 0), "phi(): K cannot be zero");
    QL_REQUIRE(T != QuantLib::Null<Real>(), "phi(): T cannot be null");
    QL_REQUIRE(sigma != QuantLib::Null<Real>(), "phi(): sigma cannot be null");
    QL_REQUIRE(!close_enough(sigma, 0), "phi(): sigma cannot be zero");
    //QL_REQUIRE(T != 0, "phi(): cannot divide by zero (T)");
    if (close_enough(T, 0)) {
        const Real x = callPut * std::log(P / K);
        return x > 0 ? 1 : -1;
    } else {
        const Real x = callPut * (std::log(P / K) + 0.5 * sigma * sigma * T) / (sigma * std::sqrt(T));
        const CumulativeNormalDistribution N;
        return N(x);
    }
}
} // namespace

namespace ore {
namespace analytics {
using namespace ore::data;

// get average notional from coupon leg
Real SACCR::getLegAverageNotional(const QuantLib::ext::shared_ptr<Trade>& trade, Size legIdx, const DayCounter& dc, Real& currentPrice) {
    Real avgNotional = 0.0;
    Real countTimes = 0.0;
    const Date& today = Settings::instance().evaluationDate();
    const Leg& leg = trade->legs()[legIdx];

    for (auto l : leg) {
        if (l->hasOccurred(today))
            continue;

        Real yearFrac = 1.0;

        if (auto coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(l)) {
            const Date& startDate = coupon->accrualStartDate();
            const Date& endDate = coupon->accrualEndDate();
            yearFrac = dc.yearFraction(std::max(startDate, today), endDate);
            Real notional = coupon->nominal() * getFxRate(trade->legCurrencies()[legIdx]);
            avgNotional += notional * yearFrac;
        } else if (auto coupon_1 = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndexedCashFlow>(l)) {
            Real gearing = coupon_1->gearing();
            Real quantity = coupon_1->periodQuantity();
            Real spread = coupon_1->spread();

            if (currentPrice == QuantLib::Null<Real>())
                currentPrice = coupon_1->fixing();
            Real notional = gearing * quantity * (currentPrice + spread) * getFxRate(trade->legCurrencies()[legIdx]);
            avgNotional += notional * yearFrac;

        } else if (auto coupon_2 = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndexedAverageCashFlow>(l)) {
            Real gearing = coupon_2->gearing();
            Real quantity = coupon_2->periodQuantity();
            Real spread = coupon_2->spread();
            if (currentPrice == QuantLib::Null<Real>())
                currentPrice = coupon_2->fixing();
            Real notional = gearing * quantity * (currentPrice + spread) * getFxRate(trade->legCurrencies()[legIdx]);
            
            const Date& startDate = coupon_2->startDate();
            const Date& endDate = coupon_2->endDate();
            yearFrac = dc.yearFraction(std::max(startDate, today), endDate);

            avgNotional += notional * yearFrac;
        } else if (auto fxLinkedCashflow = QuantLib::ext::dynamic_pointer_cast<QuantExt::FXLinkedCashFlow>(l)) {
            continue;
        } else if (auto cashflow = QuantLib::ext::dynamic_pointer_cast<QuantExt::SimpleCashFlow>(l)) {
            continue;
        } else {
            QL_FAIL("unsupported coupon type");
        }

         countTimes += yearFrac;
    }

    if (countTimes > 0.0)
        avgNotional /= countTimes;

    return avgNotional;
}

void SACCR::clear() {
    totalNPV_ = 0.0;
    totalCC_ = 0.0;
    NPV_.clear();
    npvAssetClass_.clear();
    npvHedgingSet_.clear();
    EAD_.clear();
    RC_.clear();
    PFE_.clear();
    multiplier_.clear();
    addOn_.clear();
    addOnAssetClass_.clear();
    addOnHedgingSet_.clear();
    nettingSets_.clear();
}

SACCR::SACCR(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
             const QuantLib::ext::shared_ptr<NettingSetManager>& nettingSetManager,
             const QuantLib::ext::shared_ptr<CounterpartyManager>& counterpartyManager, const QuantLib::ext::shared_ptr<Market>& market,
             const std::string& baseCurrency,
             const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances,
             const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& calculatedCollateralBalances,
             const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper,
             const QuantLib::ext::shared_ptr<SimmBucketMapper>& bucketMapper,
             const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& refDataManager,
             const std::map<ReportType, QuantLib::ext::shared_ptr<ore::data::Report>>& outReports)
    : reports_(outReports), portfolio_(portfolio), nettingSetManager_(nettingSetManager),
      counterpartyManager_(counterpartyManager), market_(market), baseCurrency_(baseCurrency),
      collateralBalances_(collateralBalances), calculatedCollateralBalances_(calculatedCollateralBalances),
      nameMapper_(nameMapper), bucketMapper_(bucketMapper), refDataManager_(refDataManager) {

    clear();
    validate();
    tradeDetails();
    aggregate();
    combineCollateralBalances();
}

const map<string, SACCR::AssetClass> tradeAssetClassMap = {{"Swap", SACCR::AssetClass::IR},
                                                           {"Swaption", SACCR::AssetClass::IR},
                                                           {"FxOption", SACCR::AssetClass::FX},
                                                           {"FxForward", SACCR::AssetClass::FX},
                                                           {"FxBarrierOption", SACCR::AssetClass::FX},
                                                           {"FxTouchOption", SACCR::AssetClass::FX},
                                                           {"CommodityForward", SACCR::AssetClass::Commodity},
                                                           {"CommoditySwap", SACCR::AssetClass::Commodity},
                                                           {"EquityOption", SACCR::AssetClass::Equity},
                                                           {"TotalReturnSwap", SACCR::AssetClass::Equity},
                                                           {"Failed", SACCR::AssetClass::None}};

std::ostream& operator<<(std::ostream& os, SACCR::AssetClass ac) {
    switch (ac) {
    case SACCR::AssetClass::IR:
        return os << "IR";
    case SACCR::AssetClass::FX:
        return os << "FX";
    case SACCR::AssetClass::Credit:
        return os << "Credit";
    case SACCR::AssetClass::Equity:
        return os << "Equity";
    case SACCR::AssetClass::Commodity:
        return os << "Commodity";
    case SACCR::AssetClass::None:
        return os << "AssetClass::None";
    default:
        QL_FAIL("Unknown SACCR::AssetClass");
    }
}

const map<string, string> commodityBucketMapping = {
    {"1", "Energy"},       {"2", "Energy"},       {"3", "Energy"}, {"4", "Energy"},
    {"5", "Energy"},       {"6", "Energy"},       {"7", "Energy"}, {"8", "Energy"},
    {"9", "Energy"},       {"11", "Metal"},       {"12", "Metal"}, {"13", "Agriculture"},
    {"14", "Agriculture"}, {"15", "Agriculture"}, {"16", "Other"}, {"10", "Other"}};

const map<string, string> commodityQualifierMapping = {{"Coal Americas", "Coal"},
                                                       {"Coal Europe", "Coal"},
                                                       {"Coal Africa", "Coal"},
                                                       {"Coal Australia", "Coal"},
                                                       {"Crude oil Americas", "Crude oil"},
                                                       {"Crude oil Europe", "Crude oil"},
                                                       {"Crude oil Asia/Middle East", "Crude oil"},
                                                       {"Light Ends Americas", "Light Ends"},
                                                       {"Light Ends Europe", "Light Ends"},
                                                       {"Light Ends Asia", "Light Ends"},
                                                       {"Middle Distillates Americas", "Middle Distillates"},
                                                       {"Middle Distillates Europe", "Middle Distillates"},
                                                       {"Middle Distillates Asia", "Middle Distillates"},
                                                       {"Heavy Distillates Americas", "Heavy Distillates"},
                                                       {"Heavy Distillates Europe", "Heavy Distillates"},
                                                       {"Heavy Distillates Asia", "Heavy Distillates"},
                                                       {"NA Natural Gas Gulf Coast", "Natural Gas"},
                                                       {"NA Natural Gas North East", "Natural Gas"},
                                                       {"NA Natural Gas West", "Natural Gas"},
                                                       {"EU Natural Gas Europe", "Natural Gas"},
                                                       {"NA Power Eastern Interconnect", "Power"},
                                                       {"NA Power ERCOT", "Power"},
                                                       {"NA Power Western Interconnect", "Power"},
                                                       {"EU Power Germany", "Power"},
                                                       {"EU Power UK", "Power"}};

string SACCR::getCommodityName(const string& index, bool withPrefix) {
    std::string commodity = index;

    // Remove prefix
    if (!withPrefix) {
        std::string prefix = "COMM-";
        if (commodity.substr(0, prefix.size()) == prefix)
            commodity.erase(0, prefix.size());
    }

    // Remove expiry of form NAME-YYYY-MM-DD
    Date expiry;
    if (commodity.size() > 10) {
        string test = commodity.substr(commodity.size() - 10);
        if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}-\\d{2}"))) {
            expiry = parseDate(test);
            commodity = commodity.substr(0, commodity.size() - test.size() - 1);
        }
    }

    // Remove expiry of form NAME-YYYY-MM if NAME-YYYY-MM-DD failed
    if (expiry == Date() && commodity.size() > 7) {
        string test = commodity.substr(commodity.size() - 7);
        if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}"))) {
            expiry = parseDate(test + "-01");
            commodity = commodity.substr(0, commodity.size() - test.size() - 1);
        }
    }

    return commodity;
}

string SACCR::getCommodityHedgingSubset(const string& comm) {
    std::string commodity = getCommodityName(comm);

    auto qualifier = nameMapper_->qualifier(commodity);
    const auto it = commodityQualifierMapping.find(qualifier);

    // some qualifiers are grouped together, check if this is one
    if (it != commodityQualifierMapping.end())
        return it->second;
    else
        return qualifier;
}

string SACCR::getCommodityHedgingSet(const string& comm) {
    std::string commodity = getCommodityName(comm);

    QL_REQUIRE(nameMapper_, "no simm name mapper provided");
    QL_REQUIRE(bucketMapper_, "no bucket name mapper provided");
    auto qualifier = nameMapper_->qualifier(commodity);
    auto bucket = bucketMapper_->bucket(CrifRecord::RiskType::Commodity, qualifier);
    const auto it = commodityBucketMapping.find(bucket);

    if (it == commodityBucketMapping.end())
        QL_FAIL("no hedging set found for " << commodity);
    return it->second;
}

SACCR::AssetClass SACCR::getAssetClass(const string& tradeType, bool isXCCYSwap) {
    if ((tradeType == "Swap" || tradeType == "Swaption") && isXCCYSwap) {
        return SACCR::AssetClass::FX;
    }
    auto ac = tradeAssetClassMap.find(tradeType);
    QL_REQUIRE(ac != tradeAssetClassMap.end(), "getAssetClass: tradeType '" << tradeType << "' not recognised");

    if (ac->second == SACCR::AssetClass::Commodity) {
        QL_REQUIRE(!isXCCYSwap, "cross currency not supported for commodity trades");
    }
    return ac->second;
}

Real SACCR::getSupervisoryDuration(const TradeData& tradeData) {
    Real SD = Null<Real>();
    if (tradeData.assetClass == SACCR::AssetClass::IR || tradeData.assetClass == SACCR::AssetClass::Credit) {
        SD = (std::exp(-0.05 * tradeData.S) - std::exp(-0.05 * tradeData.E)) / 0.05;
    }
    return SD;
}

std::string SACCR::getFirstRiskFactor(const std::string& hedgingSet, const std::string& hedgingSubset,
                                      const SACCR::AssetClass& assetClass,
                                      const QuantLib::ext::shared_ptr<Trade>& trade) {

    if (assetClass == SACCR::AssetClass::FX) {
        return hedgingSet.substr(0, 3);
    } else if (assetClass == SACCR::AssetClass::IR) {
        // We assume that the swap has two legs here, which is currently reasonable
        // given the QL_REQUIRE(... == 2, ...) currently coded in for IR swaps.{
        if (!isBasisSwap(trade))
            return "";
        else
            QL_FAIL("getFirstRiskFactor: IR basis swaps not currently supported");
            // return hedgingSet;
    } else if (assetClass == SACCR::AssetClass::Equity || assetClass == SACCR::AssetClass::Commodity) {
        if (hedgingSet.find('/') != string::npos) {
            // For basis trades
            return hedgingSet;
        } else {
            return hedgingSubset;
        }
    } else {
        QL_FAIL("getFirstRiskFactor: unsupported asset class " << assetClass);
    }
}

Real SACCR::getSupervisoryOptionVolatility(const TradeData& tradeData) {
    if (tradeData.assetClass == SACCR::AssetClass::Equity) {
        if (tradeData.isEquityIndex)
            return 0.75;
        else
            return 1.2;
    }
    QL_FAIL("SACCR::getSupervisoryOptionVolatility() not supported for trade " << tradeData.id);
}

Real SACCR::getDelta(const QuantLib::ext::shared_ptr<Trade>& trade, TradeData& tradeData, Date today) {

    // The sign of the delta adjustment depends on whether the trade is Long or Short in the primary risk factor
    // A trade is Long if the market value of the instrument increases when the value of the primary risk
    // factor increases, and a trade is Long if the reverse is true.
    Real delta = 1;
    Real multiplier = 1;
    // delta must be consistent within the hedging set (i.e. per ccy pair),
    // so we can arbitarily choose to set delta's sign to +1 (-1) if the nominal repayments
    // are received (paid) in the first currency. We check one leg only.
    // We apply the same logic to FX Forwards, see below
    string firstRiskfactor = getFirstRiskFactor(tradeData.hedgingSet, tradeData.hedgingSubset, tradeData.assetClass, trade);
    if (trade->tradeType() == "Swap") {
        QuantLib::ext::shared_ptr<ore::data::Swap> swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
        QL_REQUIRE(swap, "Trade cast to Swap failed");
        if (tradeData.assetClass == SACCR::AssetClass::FX) {
            for (auto leg : swap->legData()) {
                if (leg.currency() == firstRiskfactor) {
                    multiplier = leg.isPayer() ? -1 : 1;
                    break;
                }
            }
        } else if (tradeData.assetClass == SACCR::AssetClass::IR) {
            for (const auto& leg : swap->legData()) {
                if (leg.legType() == LegType::Floating) {
                    multiplier = leg.isPayer() ? -1 : 1;
                    break;
                }
            }
        } else {
            QL_FAIL("getDelta: Asset class " << tradeData.assetClass << " not currently supported for Swap trade type");
        }
    } else if (trade->tradeType() == "TotalReturnSwap") {
        auto trs = QuantLib::ext::dynamic_pointer_cast<ore::data::TRS>(trade);
        QL_REQUIRE(trs, "Trade cast to TRS failed");
        delta = trs->returnData().payer() ? -1 : 1;

        QL_REQUIRE(trs->underlying().size() == 1, "Currently only 1 underlying supported.");
        const auto& underlyingTrade = trs->underlying().front();
        if (underlyingTrade->tradeType() == "EquityOptionPosition") {
            bool flipTrade = false;
            tradeData.strike = getOptionStrike(underlyingTrade, flipTrade);
            tradeData.price = getOptionPrice(underlyingTrade);

            const auto& [callPut, boughtSold] = getOptionType(underlyingTrade, flipTrade);
            multiplier *= callPut * boughtSold;
            
            Real sigma = getSupervisoryOptionVolatility(tradeData);
            delta *= phi(tradeData.price, tradeData.strike, tradeData.T, sigma, callPut);
        }
    } else if (trade->tradeType() == "Swaption") {
        if (tradeData.assetClass == AssetClass::FX || isCrossCurrencySwap(trade)) {
            QL_FAIL("getDelta: Cross currency swaptions not currently supported");
        } else if (tradeData.assetClass == AssetClass::IR) {
            QL_REQUIRE(!isBasisSwap(trade), "getDelta: IR basis swaps not currently supported.");

            tradeData.strike = getOptionStrike(trade);

            const auto& ar = trade->instrument()->additionalResults();
            QL_REQUIRE(ar.find("atmForward") != ar.end(),
                       "getDelta: Could not find price for IR swaption " << trade->id());
            tradeData.price = boost::any_cast<Real>(ar.at("atmForward"));

            Real sigma = 0.5; // supervisory option volatility for IR trades

            const auto& [callPut, boughtSold] = getOptionType(trade, false);
            multiplier = callPut * boughtSold;

            delta = phi(tradeData.price, tradeData.strike, tradeData.T, sigma, callPut);
        }
    } else if (trade->tradeType() == "FxForward") {
        string boughtCcy;
        QuantLib::ext::shared_ptr<ore::data::FxForward> fxFwd =
            QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
        QL_REQUIRE(fxFwd, "Trade cast to FxForward failed");
        boughtCcy = fxFwd->boughtCurrency();
        multiplier = (firstRiskfactor == boughtCcy) ? 1 : -1;
    } else if (trade->tradeType() == "FxOption" || trade->tradeType() == "FxBarrierOption" ||
               trade->tradeType() == "FxTouchOption" || trade->tradeType() == "EquityOption") {
        bool flipTrade = false;
        Real sigma;
        if (trade->tradeType() == "EquityOption") {
            sigma = getSupervisoryOptionVolatility(tradeData);
            tradeData.strike = getOptionStrike(trade, flipTrade);

            tradeData.price = getOptionPrice(trade);
        } else {
            std::string origBoughtCcy;
            std::string origSoldCcy;
            getFxCurrencies(trade, origBoughtCcy, origSoldCcy);

            // calculate option delta
            sigma = 0.15; // supervisory option volatility for fx trades

            // In saccr the calculation of delta for Fx Options depends on the convention taken wrt to
            // the ordering of the currency pair.
            // For each ccyPair ccy1/ccy2 we wish to maintain the same ordering convention accross the hedging set,
            // and always have boughtCurrency == ccy1
            // if this is not the case then we flip the trade
            flipTrade = (firstRiskfactor != origBoughtCcy);

            string boughtCcy = flipTrade ? origSoldCcy : origBoughtCcy;
            string soldCcy = flipTrade ? origBoughtCcy : origSoldCcy;

            Real disc1near = market_->discountCurve(boughtCcy)->discount(today);
            Real disc1far = market_->discountCurve(boughtCcy)->discount(trade->maturity());
            Real disc2near = market_->discountCurve(soldCcy)->discount(today);
            Real disc2far = market_->discountCurve(soldCcy)->discount(trade->maturity());
            Real fxfwd = disc1near / disc1far * disc2far / disc2near * market_->fxRate(boughtCcy + soldCcy)->value();
            tradeData.price = fxfwd;

            tradeData.strike = getOptionStrike(trade, flipTrade);
        }

        const auto& [callPut, boughtSold] = getOptionType(trade, flipTrade);
        multiplier = callPut * boughtSold;

        delta = phi(tradeData.price, tradeData.strike, tradeData.T, sigma, callPut);
    } else if (trade->tradeType() == "CommoditySwap") {
        QuantLib::ext::shared_ptr<ore::data::CommoditySwap> swap =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CommoditySwap>(trade);
        QL_REQUIRE(swap, "Trade cast to Swap failed");
        QL_REQUIRE(swap->legData().size() == 2, "two legs expected.");

        // if both legs are floating then this is a basis swap
        if (swap->legData().at(0).legType() == LegType::CommodityFloating &&
            swap->legData().at(1).legType() == LegType::CommodityFloating) {
            QuantLib::ext::shared_ptr<CommodityFloatingLegData> com =
                (QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(swap->legData().at(0).concreteLegData()));
            auto legData = com->name() == firstRiskfactor ? swap->legData().at(0) : swap->legData().at(1);
            multiplier = legData.isPayer() ? -1 : 1;
        } else {
            auto legData =
                swap->legData().at(0).legType() == LegType::CommodityFloating ? swap->legData().at(0) : swap->legData().at(1);
            multiplier = legData.isPayer() ? -1 : 1;
        }

    } else if (trade->tradeType() == "CommodityForward") {
        QuantLib::ext::shared_ptr<ore::data::CommodityForward> fwd =
            QuantLib::ext::dynamic_pointer_cast<ore::data::CommodityForward>(trade);
        QL_REQUIRE(fwd, "Trade cast to CommodityForward failed");
        Position::Type position = parsePositionType(fwd->position());

        multiplier = (position == Position::Long) ? 1 : -1;
    } else {
        QL_FAIL("getDelta: unsupported trade type " << trade->tradeType());
    }

    delta *= multiplier;
    return delta;
}

pair<string, QuantLib::ext::optional<string>> SACCR::getHedgingSet(const QuantLib::ext::shared_ptr<Trade>& trade, TradeData& tradeData) {
    string hedgingSet;
    QuantLib::ext::optional<string> hedgingSubset = QuantLib::ext::nullopt;

    const SACCR::AssetClass assetClass = tradeData.assetClass;
    // FX derivatives consist of a separate hedging set for each currency pair;
    // we will extract the currencies from this trade and use these to construct its ccy pair.
    if (assetClass == SACCR::AssetClass::FX) {
        std::set<string> currencies;
        // cross currency swaptions are not currently supported in ore
        // we'll put this check here in case this changes in future, so no undefined behaviour occurs
        QL_REQUIRE(trade->tradeType() != "Swaption", "cross currency swaptions are not currently supported");
        // swaps/fxswaps/fxfwds
        if (trade->tradeType() == "Swap" || trade->tradeType() == "FxSwap" || trade->tradeType() == "FxForward") {
            std::vector<string> legCurrencies = trade->legCurrencies();
            std::copy(legCurrencies.begin(), legCurrencies.end(), std::inserter(currencies, currencies.end()));
        } else {
            std::string boughtCcy;
            std::string soldCcy;
            getFxCurrencies(trade, boughtCcy, soldCcy);
            currencies.insert(boughtCcy);
            currencies.insert(soldCcy);
        }

        QL_REQUIRE(currencies.size() == 2, "each FX trade should have exactly two underlying currencies");
        vector<string> ccyPair(currencies.begin(), currencies.end());
        std::sort(ccyPair.begin(), ccyPair.end());
        hedgingSet = ccyPair.at(0) + ccyPair.at(1);
        // Interest rate derivatives consist of a separate hedging set for each currency;
        // However Derivatives that reference the basis between two risk factors and are denominated in a single
        // currency (basis transactions) must be treated within separate hedging sets
    } else if (assetClass == SACCR::AssetClass::IR) {
        auto swap = QuantLib::ext::dynamic_pointer_cast<ore::data::Swap>(trade);
        auto swaption = QuantLib::ext::dynamic_pointer_cast<ore::data::Swaption>(trade);
        if (swap)
            QL_REQUIRE(swap->legs().size() == 2, "two legs expected for IR swap");
        else if (swaption)
            QL_REQUIRE(swaption->legs().size() == 2, "two legs expected for swaption");

        set<string> indicesIR = trade->underlyingIndices()[ore::data::AssetClass::IR];
        set<string> indicesINF = trade->underlyingIndices()[ore::data::AssetClass::INF];

        string ccy = trade->legCurrencies().front();
        if (indicesIR.size() == 0 && indicesINF.size() == 0) {
            hedgingSet = ccy;
        } else if (indicesINF.size() > 0) {
            hedgingSet = ccy + "-BASIS-IBOR-INFLATION";
        } else if (indicesIR.size() == 2) {
            if (indicesIR.find("USD-SIFMA") != indicesIR.end()) {
                hedgingSet = "USD-BASIS-BMA";
            } else {
                vector<string> tenors;
                for (auto& i : indicesIR) {
                    tenors.push_back(i.substr(i.find_last_of("-")));
                }
                std::sort(tenors.begin(), tenors.end());
                hedgingSet = ccy + "-BASIS" + tenors.at(0) + tenors.at(1);
            }
            basisHedgingSets_.insert(hedgingSet);
        } else {
            QL_FAIL("Hedging set not found");
        }
    } else if (assetClass == SACCR::AssetClass::Commodity) {
        std::set<std::string> indicesSet = trade->underlyingIndices()[ore::data::AssetClass::COM];
        std::vector<std::string> indices(indicesSet.begin(), indicesSet.end());
        std::sort(indices.begin(), indices.end());
        QL_REQUIRE(indices.size() == 1 || indices.size() == 2, "unexpected number of commodity indices found");
        if (indices.size() == 1) {
            hedgingSet = getCommodityHedgingSet(*indices.begin());
            hedgingSubset = getCommodityHedgingSubset(*indices.begin());
        } else {
            // For basis trades each commodity pair form their own hedging set with a single hedging subset
            // But we should note that for "Electricity" commodity trades the supervisory factor differs from other
            // classes. So if one of the commodities in the trade are "Electricity" based we mark their subclass as such
            Size count = 0;
            for (auto i : indices) {
                hedgingSet += getCommodityName(i, true);
                if (count++ < indices.size() - 1)
                    hedgingSet += "/";
            }
            // std::ostringstream stream;
            // std::copy(indices.begin(), indices.end(), std::ostream_iterator<std::string>(stream, "/"));
            // hedgingSet = stream.str();
            // hedgingSet.pop_back();

            bool power = false;
            for (auto& i : indices) {
                if ((power = (getCommodityHedgingSubset(i) == "Power")))
                    break;
            }
            hedgingSubset = power ? "Power" : "";
            basisHedgingSets_.insert(hedgingSet);
        }
    } else if (assetClass == SACCR::AssetClass::Equity) {
        // FIXME: Mostly duplicating commodity logic - Credit will be very similar to Equity logic
        std::set<std::string> indicesSet = trade->underlyingIndices()[ore::data::AssetClass::EQ];
        std::vector<std::string> indices(indicesSet.begin(), indicesSet.end());
        if (indices.size() == 1) {
            hedgingSubset = indices[0];
            // Store information on whether equity underlying is an index
            if (refDataManager_ && refDataManager_->hasData("Equity", *hedgingSubset)) {
                auto eqRefData = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityReferenceDatum>(
                    refDataManager_->getData("Equity", *hedgingSubset));
                tradeData.isEquityIndex = eqRefData->equityData().isIndex;
            }
        } else {
            QL_FAIL("SACCR::getHedgingSet() multiple underlyings not yet supported.");
        };
    } else {
        QL_FAIL("HedgingSet: currently unsupported asset class " << assetClass);
    }

    return std::make_pair(hedgingSet, hedgingSubset);
}

Real SACCR::getCurrentNotional(const QuantLib::ext::shared_ptr<Trade>& trade, SACCR::AssetClass assetClass,
                               const string& baseCcy, const DayCounter& dc, Real& currentPrice1,
                               Real& currentPrice2, const string& hedgingSet,
                               const string& hedgingSubset) {
    
    const Date& today = Settings::instance().evaluationDate();
    Real currentNotional = Null<Real>();

    try {
        // For FX derivatives, the adjusted notional is defined as the notional of the foreign currency leg, converted to
        // the domestic currency. If both legs are in currencies other than the domestic currency, the notional amount of
        // each leg is converted to the domestic currency and the leg with the larger domestic currency value is the
        // adjusted notional amount.
        if (trade->tradeType() == "FxForward" || trade->tradeType() == "FxOption" ||
            trade->tradeType() == "FxBarrierOption") {
            string boughtCcy;
            string soldCcy;
            Real boughtAmount = 0.0;
            Real soldAmount = 0.0;
            if (trade->tradeType() == "FxForward") {
                QuantLib::ext::shared_ptr<ore::data::FxForward> fxFwd = QuantLib::ext::dynamic_pointer_cast<ore::data::FxForward>(trade);
                QL_REQUIRE(fxFwd, "Trade cast to FxForward failed");
                boughtCcy = fxFwd->boughtCurrency();
                soldCcy = fxFwd->soldCurrency();

                boughtAmount = fxFwd->boughtAmount();
                soldAmount = fxFwd->soldAmount();

            } else if (trade->tradeType() == "FxOption") {
                QuantLib::ext::shared_ptr<ore::data::FxOption> fxOpt = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade);
                QL_REQUIRE(fxOpt, "Trade cast to FxOption failed");

                boughtCcy = fxOpt->boughtCurrency();
                soldCcy = fxOpt->soldCurrency();

                boughtAmount = fxOpt->boughtAmount();
                soldAmount = fxOpt->soldAmount();
            } else {
                QuantLib::ext::shared_ptr<ore::data::FxBarrierOption> fxOpt =
                    QuantLib::ext::dynamic_pointer_cast<ore::data::FxBarrierOption>(trade);
                QL_REQUIRE(fxOpt, "Trade cast to FxBarrierOption failed");

                boughtCcy = fxOpt->boughtCurrency();
                soldCcy = fxOpt->soldCurrency();

                boughtAmount = fxOpt->boughtAmount();
                soldAmount = fxOpt->soldAmount();
            }

            const Real boughtFx = getFxRate(boughtCcy);
            const Real soldFx = getFxRate(soldCcy);

            const Real boughtNotional = (boughtCcy == baseCcy) ? 0.0 : boughtAmount * boughtFx;
            const Real soldNotional = (soldCcy == baseCcy) ? 0.0 : soldAmount * soldFx;
            currentNotional = std::max(boughtNotional, soldNotional);
        } else if (trade->tradeType() == "FxTouchOption") {
            QuantLib::ext::shared_ptr<ore::data::FxTouchOption> fxOpt =
                QuantLib::ext::dynamic_pointer_cast<ore::data::FxTouchOption>(trade);
            QL_REQUIRE(fxOpt, "Trade cast to FxTouchOption failed");
            const Real fx = getFxRate(fxOpt->payoffCurrency());
            currentNotional = fx * fxOpt->payoffAmount();
        } else if (trade->tradeType() == "EquityOption") {
            auto equityOption = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOption>(trade);
            Real quantity = equityOption->quantity();
            Real fx = getFxRate(equityOption->notionalCurrency());
            currentPrice1 =
                market_->equityCurve(equityOption->equityName())
                    ->fixing(market_->equityCurve(equityOption->equityName())->fixingCalendar().adjust(today)) * fx;
            currentNotional = quantity * currentPrice1;
        } else if (trade->tradeType() == "TotalReturnSwap") {
            QL_REQUIRE(assetClass == AssetClass::Equity,
                       "TRS currently only supported for asset class " << AssetClass::Equity << ". Got " << assetClass);
            auto trs = QuantLib::ext::dynamic_pointer_cast<ore::data::TRS>(trade);
            const map<ore::data::AssetClass, set<string>>& underlyingIndices = trs->underlyingIndices();
            QL_REQUIRE(underlyingIndices.size() == 1 && underlyingIndices.begin()->first == ore::data::AssetClass::EQ &&
                           underlyingIndices.at(ore::data::AssetClass::EQ).size() == 1,
                       "Only single-underlying Equity TRS currently supported.");

            const string& equityName = *(underlyingIndices.at(ore::data::AssetClass::EQ).begin());
            Real fx = getFxRate(market_->equityCurve(equityName)->currency().code());
            currentPrice1 = market_->equityCurve(equityName)
                                ->fixing(market_->equityCurve(equityName)->fixingCalendar().adjust(today)) * fx;
            const auto& underlyingTrade = trs->underlying().front();

            if (underlyingTrade->tradeType() == "EquityPosition") {
                auto equityPosition = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityPosition>(underlyingTrade);
                currentNotional = currentPrice1 * equityPosition->data().quantity();
            } else if (underlyingTrade->tradeType() == "EquityOptionPosition") {
                auto equityOpPosition = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOptionPosition>(underlyingTrade);
                currentNotional = currentPrice1 * equityOpPosition->data().quantity();
            } else {
                QL_FAIL("Only EquityPosition and EquityOptionPosition underlying trade types supported for Equity TRS. Got "
                        << underlyingTrade->tradeType());
            }
        } else if (trade->tradeType() == "CommodityForward") {
            // For commodity derivatives, the adjusted notional is defined as the product of the
            // current price of one unit of the stock or commodity (eg a share of equity or barrel of oil) and
            // the number of units referenced by the trade.

            auto commFwd = QuantLib::ext::dynamic_pointer_cast<ore::data::CommodityForward>(trade);

            currentNotional = commFwd->currentNotional() * getFxRate(trade->notionalCurrency());
            currentPrice1 = currentNotional / commFwd->quantity();

        } else if (trade->tradeType() == "CommoditySwap") {
            // FIXME: we make heavy use of the assumption that there are only 2 legs here, as a basis swap
            auto commoditySwap = QuantLib::ext::dynamic_pointer_cast<ore::data::CommoditySwap>(trade);
            string firstRiskFactor = getFirstRiskFactor(hedgingSet, hedgingSubset, SACCR::AssetClass::Commodity, trade);
            bool isBasis = firstRiskFactor.find("/") != string::npos;
            vector<string> tokens;
            boost::split(tokens, firstRiskFactor, boost::is_any_of("/"));
            QL_REQUIRE(!isBasis || tokens.size() == 2, "Expected 2 tokens for firstRiskFactor. Got " << tokens.size());

            for (Size i = 0; i < commoditySwap->legCurrencies().size(); i++) {
                if (commoditySwap->legData().at(i).legType() != LegType::CommodityFloating)
                    continue;

                auto floatingLeg = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(
                    commoditySwap->legData()[i].concreteLegData());
                string commName = floatingLeg->name();

                if (getCommodityHedgingSubset(commName) == firstRiskFactor || isBasis) {
                    Real multiplier = commoditySwap->legData()[i].isPayer() ? -1 : 1;
                    if (isBasis) {
                        // If we are short w.r.t. the basis, then revert the multiplier
                        if ((commName == tokens[0] && commoditySwap->legData()[i].isPayer()) ||
                            (commName == tokens[1] && !commoditySwap->legData()[i].isPayer()))
                            multiplier *= -1.0;
                    }
                    Real legCurrentNotional =
                        getLegAverageNotional(commoditySwap, i, dc,
                                              currentPrice1 == Null<Real>() ? currentPrice1 : currentPrice2) *
                        multiplier;

                    if (currentNotional == Null<Real>())
                        currentNotional = legCurrentNotional;
                    else
                        currentNotional += legCurrentNotional;
                }
            }
        } else if (trade->legCurrencies().size() > 0) {
            for (Size i = 0; i < trade->legCurrencies().size(); i++) {
                if (assetClass == SACCR::AssetClass::FX && trade->legCurrencies().at(i) == baseCcy)
                    continue;

                Real legCurrentNotional = getLegAverageNotional(
                    trade, i, dc, currentPrice1 == Null<Real>() ? currentPrice1 : currentPrice2);
                
                if (currentNotional == Null<Real>())
                    currentNotional = legCurrentNotional;
                else
		    currentNotional = std::max(currentNotional, legCurrentNotional);
            }
        } else {
            QL_FAIL("CurrentNotional: unsupported trade type: " << trade->tradeType());
        }
    } catch (const std::exception& e) {
        auto subFields = map<string, string>({{"tradeId", trade->id()}});
        StructuredAnalyticsWarningMessage("SA-CCR", "Could not get trade notional", e.what(), subFields).log();
    }
    return currentNotional;
}

void SACCR::validate() {
    DLOG("SA-CCR: Validating configurations");

    const bool emptyNettingSetManager = nettingSetManager_->empty();
    const bool emptyCollateralBalances = collateralBalances_->empty();
    const bool emptyCounterpartyManager = counterpartyManager_->empty();

    // Check #1: For files that were not provided, log a top-level warning message instead
    auto analyticSubField = map<string, string>({{"analyticType", "SA-CCR"}});
    if (emptyNettingSetManager) {
        StructuredConfigurationWarningMessage("Netting set definitions", "", "Validating input configurations",
                                              "Input configuration was not provided. The default values will be "
                                              "used for all netting sets in the portfolio",
                                              analyticSubField)
            .log();
    }
    if (emptyCollateralBalances) {
        StructuredConfigurationWarningMessage("Collateral balances", "", "Validating input configurations",
                                              "Input configuration was not provided. The default values will be "
                                              "used for all netting sets in the portfolio",
                                              analyticSubField)
            .log();
    }
    if (emptyCounterpartyManager) {
        StructuredConfigurationWarningMessage("Counterparty information", "", "Validating input configurations",
                                              "Input configuration was not provided. The default values will be "
                                              "used for all netting sets in the portfolio",
                                              analyticSubField)
            .log();
    }

    // Collect list of netting sets from netting set definitions
    const vector<NettingSetDetails>& nettingSets = nettingSetManager_->uniqueKeys();
    nettingSets_ = std::set<NettingSetDetails>(nettingSets.begin(), nettingSets.end());

    DLOG("SA-CCR: Validating netting set definitions");

    // Check #2: Ensure that each trade has an existing entry in the netting set definitions
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        if (!nettingSetManager_->has(tradeNettingSetDetails)) {
            if (!emptyNettingSetManager) {
                StructuredConfigurationWarningMessage("Netting set definitions",
                                                           ore::data::to_string(tradeNettingSetDetails),
                                                           "Validating input configurations",
                                                           "Failed to find an entry for the given netting set "
                                                           "details, so the default configuration will be "
                                                           "assumed for this missing netting set definition.",
                                                           analyticSubField).log();
            }

            // Add default netting set definition entry in place of missing netting set
            NettingSetDefinition nsd = NettingSetDefinition(
                tradeNettingSetDetails, "Bilateral", baseCurrency_, "", 0.0, saCcrDefaults_.nettingSetDef.thresholdRcv,
                0.0, saCcrDefaults_.nettingSetDef.mtaRcv, saCcrDefaults_.nettingSetDef.iaHeld, "FIXED", "1D", "1D",
                ore::data::to_string(saCcrDefaults_.nettingSetDef.mpor), 0.0, 0.0, vector<string>(), false, "Bilateral",
                saCcrDefaults_.nettingSetDef.calculateIMAmount, saCcrDefaults_.nettingSetDef.calculateVMAmount);
            nettingSetManager_->add(QuantLib::ext::make_shared<NettingSetDefinition>(nsd));
            nettingSets_.insert(nsd.nettingSetDetails());
        }
    }

    DLOG("SA-CCR: Validating collateral balances");

    // Check #3: Check if there are balances that override the calculateIMAmount and caculateVMAmount in netting set
    // definitions
    set<NettingSetDetails> checkedNettingSets;
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        // To avoid duplicated warnings for the same netting set details
        if (checkedNettingSets.find(tradeNettingSetDetails) != checkedNettingSets.end())
            continue;

        checkedNettingSets.insert(tradeNettingSetDetails);

        const auto& nsd = nettingSetManager_->nettingSetDefinitions().at(tradeNettingSetDetails);
        if (!nsd->activeCsaFlag())
            continue;

        const bool calculateIM = nsd->csaDetails()->calculateIMAmount();
        const bool calculateVM = nsd->csaDetails()->calculateVMAmount();

        if (collateralBalances_->has(tradeNettingSetDetails)) {
            const auto& cb = collateralBalances_->get(tradeNettingSetDetails);
            if (calculateIM && cb->initialMargin() != Null<Real>()) {
                StructuredConfigurationWarningMessage(
                    "Collateral balances and netting set definitions", ore::data::to_string(tradeNettingSetDetails),
                    "Validating input configurations",
                    "CalculateIMAmount=True, but an initial margin amount was still provided. This overriding "
                    "initial margin balance will be used.",
                    analyticSubField)
                    .log();
            }
            if (calculateVM && cb->variationMargin() != Null<Real>()) {
                StructuredConfigurationWarningMessage(
                    "Collateral balances and netting set definitions", ore::data::to_string(tradeNettingSetDetails),
                    "Validating input configurations",
                    "CalculateVMAmount=True, but a variation margin amount was still provided. This overriding "
                    "variation margin balance will be used.",
                    analyticSubField)
                    .log();
            }
        }
    }

    // Check #4: Ensure that collateral balances are unique
    set<NettingSetDetails> netSetsToProcess;
    for (const auto& [tradeId, trade] : portfolio_->trades())
        netSetsToProcess.insert(trade->envelope().nettingSetDetails());

    map<NettingSetDetails, int> collateralBalanceCounts;
    for (const auto& cb : collateralBalances_->collateralBalances()) {
        const NettingSetDetails& nettingSetDetails = cb.first;
        if (netSetsToProcess.count(nettingSetDetails) == 0)
            continue;

        if (collateralBalanceCounts.find(nettingSetDetails) == collateralBalanceCounts.end()) {
            collateralBalanceCounts.insert(make_pair(nettingSetDetails, 0));
        } else {
            collateralBalanceCounts[nettingSetDetails] += 1;
        }
    }
    for (const auto& c : collateralBalanceCounts) {
        const NettingSetDetails& nettingSetDetails = c.first;
        const int n = c.second;
        if (n > 1) {
            StructuredConfigurationWarningMessage("Collateral balances", ore::data::to_string(nettingSetDetails),
                                                  "Validating input configurations",
                                                  "Multiple entries found (" + std::to_string(n) + ").", analyticSubField)
                .log();
        }
    }

    // Check #5: Ensure that each trade has an existing entry in the collateral balances
    checkedNettingSets.clear();
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        // To avoid duplicated warnings for the same netting set details
        if (checkedNettingSets.find(tradeNettingSetDetails) != checkedNettingSets.end())
            continue;

        // We require a collateral balance if there is none found in the collateral balances input file or in the
        // SIMM-generated collateral balances
        const auto& nsd = nettingSetManager_->nettingSetDefinitions().at(tradeNettingSetDetails);
        bool requiresCollateralBalance = nsd->activeCsaFlag() && !collateralBalances_->has(tradeNettingSetDetails);
        if (requiresCollateralBalance) {
            // If there is already a collateral balance from calculated IM and VM is to be calculated
            if (nsd->csaDetails()->calculateIMAmount() && calculatedCollateralBalances_->has(tradeNettingSetDetails) &&
                nsd->csaDetails()->calculateVMAmount())
                requiresCollateralBalance = false;
        }

        //if (requiresCollateralBalance && !collateralBalances_->has(tradeNettingSetDetails)) {
        if (requiresCollateralBalance) {
            if (!emptyCollateralBalances) {
                StructuredConfigurationWarningMessage("Collateral balances", ore::data::to_string(tradeNettingSetDetails),
                                                      "Validating input configurations",
                                                      "Failed to find an entry for the given netting set "
                                                      "details, so the default configuration will be "
                                                      "assumed for this missing collateral balance.",
                                                      analyticSubField)
                    .log();
            }

            // Add default collateral balances entry in place of missing netting set
            CollateralBalance cb = CollateralBalance(tradeNettingSetDetails, saCcrDefaults_.collBalances.ccy,
                                                     saCcrDefaults_.collBalances.im, saCcrDefaults_.collBalances.vm);
            collateralBalances_->add(QuantLib::ext::make_shared<CollateralBalance>(cb));
            defaultIMBalances_.insert(tradeNettingSetDetails);
            defaultVMBalances_.insert(tradeNettingSetDetails);
        }

        checkedNettingSets.insert(tradeNettingSetDetails);
    }

    // Check #6: Ensure that each netting set has an entry in the collateral balances (even if it does not have a trade)
    for (const NettingSetDetails& nettingSetDetails : nettingSets_) {
        // We require a collateral balance if there is none found in the collateral balances input file
        const auto& nsd = nettingSetManager_->nettingSetDefinitions().at(nettingSetDetails);
        bool requiresCollateralBalance = nsd->activeCsaFlag();

        if (requiresCollateralBalance) {
            if (!collateralBalances_->has(nettingSetDetails) &&
                !calculatedCollateralBalances_->has(nettingSetDetails)) {
                // Add default collateral balances entry in place of missing netting set
                CollateralBalance cb = CollateralBalance(nettingSetDetails, saCcrDefaults_.collBalances.ccy,
                                                         saCcrDefaults_.collBalances.im, saCcrDefaults_.collBalances.vm);
                collateralBalances_->add(QuantLib::ext::make_shared<CollateralBalance>(cb));
                defaultIMBalances_.insert(nettingSetDetails);
                defaultVMBalances_.insert(nettingSetDetails);
            } else if (collateralBalances_->has(nettingSetDetails)) {
                auto& cb = collateralBalances_->get(nettingSetDetails);
                if (cb->variationMargin() == Null<Real>() && !nettingSetManager_->get(nettingSetDetails)->csaDetails()->calculateVMAmount()) {
                    cb->variationMargin() = saCcrDefaults_.collBalances.vm;
                    defaultVMBalances_.insert(nettingSetDetails);

                    StructuredConfigurationWarningMessage(
                        "Collateral balances", ore::data::to_string(nettingSetDetails),
                        "Validating input configurations",
                        "CalculateVMAmount was set to \'false\' in the netting "
                        "set definition, but no VariationMargin "
                        "was provided in the collateral balance. The default value of " +
                            std::to_string(saCcrDefaults_.collBalances.vm) + " will be assumed.",
                        analyticSubField)
                        .log();
                }
            }
        }
    }

    DLOG("SA-CCR: Validating counterparty information");

    // Check #7: Ensure that each trade has an existing entry in the counterparty information
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const string tradeCpty = trade->envelope().counterparty();
        if (!counterpartyManager_->has(tradeCpty)) {
            if (!emptyCounterpartyManager) {
                StructuredConfigurationWarningMessage(
                    "Counterparty information", tradeCpty, "Validating input configurations",
                    "Failed to find an entry for the given counterparty, so the default configuration will be "
                    "assumed for this counterparty",
                    analyticSubField)
                    .log();
            }

            // Add default counterparty entry in place of missing counterparty
            CounterpartyInformation cptyInfo =
                CounterpartyInformation(tradeCpty, saCcrDefaults_.cptyInfo.ccp, CounterpartyCreditQuality::NR,
                                        Null<Real>(), saCcrDefaults_.cptyInfo.saccrRW, "");
            counterpartyManager_->add(QuantLib::ext::make_shared<CounterpartyInformation>(cptyInfo));
        }
    }

    // Check #8: Create default counterparty information for netting sets that do not have trades
    // (since we create nettingSet-counterparty mappings via trades)
    if (!counterpartyManager_->has(saCcrDefaults_.cptyInfo.counterpartyId)) {
        // Add default counterparty entry in place of missing counterparty
        CounterpartyInformation cptyInfo =
            CounterpartyInformation(saCcrDefaults_.cptyInfo.counterpartyId, saCcrDefaults_.cptyInfo.ccp, CounterpartyCreditQuality::NR,
                                    Null<Real>(), saCcrDefaults_.cptyInfo.saccrRW, "");
        counterpartyManager_->add(QuantLib::ext::make_shared<CounterpartyInformation>(cptyInfo));
    }

    // Check #9: Check that each counterparty SA-CCR risk weight is between 0 and 1.5
    set<string> checkedCounterparties;
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const string& tradeCpty = trade->envelope().counterparty();

        // To avoid duplicated warnings for the same netting set details
        if (checkedCounterparties.find(tradeCpty) != checkedCounterparties.end())
            continue;

        const auto& c = counterpartyManager_->counterpartyInformation().at(tradeCpty);
        if (c->saCcrRiskWeight() < 0.0 || c->saCcrRiskWeight() > 1.5) {
            StructuredConfigurationWarningMessage(
                "Counterparty Information", tradeCpty, "Validating input configurations",
                "Unexpected SA-CCR risk weight (should be between 0.0 and 1.5, inclusive). The provided amount of " +
                    std::to_string(c->saCcrRiskWeight()) + " will still be used in subsequent calculations.",
                analyticSubField)
                .log();
        }
        checkedCounterparties.insert(tradeCpty);
    }
    checkedCounterparties.clear();

    // Check #10: For netting sets with clearing counterparty, ensure that initial margin is zero
    set<NettingSetDetails> clearingNettingSets;
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const NettingSetDetails& nettingSetDetails = trade->envelope().nettingSetDetails();
        const string& cpty = trade->envelope().counterparty();
        const bool isClearingCp = counterpartyManager_->get(cpty)->isClearingCP();
        if (isClearingCp)
            clearingNettingSets.insert(nettingSetDetails);
    }

    for (const NettingSetDetails& nsd : clearingNettingSets) {
        if (collateralBalances_->has(nsd)) {
            const auto& cb = collateralBalances_->get(nsd);
            if (cb->initialMargin() != Null<Real>())
                cb->initialMargin() = 0.0;
        }
        if (calculatedCollateralBalances_ && calculatedCollateralBalances_->has(nsd)) {
            const auto& cb = calculatedCollateralBalances_->get(nsd);
            if (cb->initialMargin() != Null<Real>())
                cb->initialMargin() = 0.0;
        }
    }

    // Check #11: Final confirmation/validation, which itself is a validation of the previous checks:
    // For each trade, check that there is a collateral balance, netting set definition and counterparty info
    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();
        const string& cpty = trade->envelope().counterparty();
        

        QL_REQUIRE(nettingSetManager_->has(tradeNettingSetDetails),
                   "Trade id \'" << tradeId << "\': Could not find corresponding entry for [" << tradeNettingSetDetails
                                 << "] in the netting set definitions.");

        if (nettingSetManager_->get(tradeNettingSetDetails)->activeCsaFlag()) {
            QL_REQUIRE(collateralBalances_->has(tradeNettingSetDetails) ||
                           calculatedCollateralBalances_->has(tradeNettingSetDetails),
                       "Trade id \'" << tradeId << "\': Could not find corresponding entry for ["
                                     << tradeNettingSetDetails
                                     << "] in the collateral balances.");
            QL_REQUIRE(counterpartyManager_->has(cpty),
                       "Trade id \'" << tradeId << "\': Could not find corresponding counerparty entry for " << cpty
                                     << " in the counterparty information.");
        }
    }
}

Real SACCR::getFxRate(const string& ccy) {
    Real fx = 1.0;
    if (ccy != baseCurrency_) {
        Handle<Quote> fxQuote = market_->fxRate(ccy + baseCurrency_);
        fx = fxQuote->value();
    }
    return fx;
}

void SACCR::tradeDetails() {
    DLOG("SA-CCR: Collecting trade contributions");

    // Collect trade NPVs if NPV provided in a report
    map<string, pair<Real, string>> crifNpv;
    if (reports_.find(SACCR::ReportType::TradeNPV) != reports_.end() && reports_.at(SACCR::ReportType::TradeNPV)) {
        const auto& npvReport =
            QuantLib::ext::dynamic_pointer_cast<InMemoryReport>(reports_.at(SACCR::ReportType::TradeNPV));

        // Collect index of columns
        map<string, Size> headersIdx = {};
        for (const string& col : {"TradeId", "NPV", "NpvCurrency"}) {
            if (npvReport->hasHeader(col)) {
                for (Size i = 0; i < npvReport->columns(); i++) {
                    if (npvReport->header(i) == col) {
                        headersIdx.insert(make_pair(col, i));
                        break;
                    }
                }
            }
            QL_REQUIRE(headersIdx.find(col) != headersIdx.end(), "Could not find header ");
        }

        // Collect trade NPVs
        for (Size i = 0; i < npvReport->rows(); i++) {
            const string& tradeId = boost::get<string>(npvReport->data(headersIdx["TradeId"], i));
            const Real npv = boost::get<Real>(npvReport->data(headersIdx["NPV"], i));
            const string& npvCcy = boost::get<string>(npvReport->data(headersIdx["NpvCurrency"], i));

            crifNpv.insert(make_pair(tradeId, make_pair(npv, npvCcy)));
        }
    }

    map<NettingSetDetails, Real> tradeCount;

    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        const NettingSetDetails& tradeNettingSetDetails = trade->envelope().nettingSetDetails();

        if (tradeCount.find(tradeNettingSetDetails) == tradeCount.end()) {
            tradeCount[tradeNettingSetDetails] = 1;
        } else {
            tradeCount[tradeNettingSetDetails] = tradeCount[tradeNettingSetDetails] + 1;
        }
    }

    DayCounter dc = ActualActual(ActualActual::ISDA); // SK: why ACT/ACT?
    const Date today = Settings::instance().evaluationDate();

    for (const auto& [tradeId, trade] : portfolio_->trades()) {
        DLOG("Processing trade: " << tradeId << " " << trade->tradeType());
        try {
            NettingSetDetails tradeNettingSetDetails = trade->envelope().nettingSetDetails();

            // check if the tradeType is supported
            const bool isSupportedTradeType = tradeAssetClassMap.count(trade->tradeType()) > 0;
            if (!isSupportedTradeType) {
                StructuredTradeWarningMessage(trade, "Trade will not be processed",
                                              "SA-CCR: Trade type is not supported")
                    .log();
                continue;
            }

            TradeData tradeData;
            // trade id, trade type, netting set (details), counterparty
            tradeData.id = trade->id();
            tradeData.type = trade->tradeType();
            tradeData.nettingSetDetails = tradeNettingSetDetails;
            tradeData.cpty = trade->envelope().counterparty();

            if (crifNpv.find(trade->id()) != crifNpv.end()) {
                // If trade was processed in one of the CRIF sub-analytics, take the NPV from there
                const Real npv = crifNpv.at(trade->id()).first;
                const string& npvCcy = crifNpv.at(trade->id()).second;
                const Real fx = getFxRate(npvCcy);
                tradeData.NPV = npv * fx;
            } else {
                // Otherwise, just use the trade from the original portfolio
                const Real npv = trade->instrument()->NPV();
                const Real fx = getFxRate(trade->npvCurrency());
                tradeData.NPV = npv * fx;
            }
            tradeData.npvCcy = baseCurrency_;

            // asset class
            // trades are allocated to asset classes based on trade type
            // FIXME: more complex trades could in theory have more than one asset class, we're not currently treating
            // that case
            const bool isXCCY = isCrossCurrencySwap(trade);
            tradeData.assetClass = getAssetClass(tradeData.type, isXCCY);

            // Maturity
            // For all asset classes, the maturity of a contract is the latest date when the contract may still be
            // active. If a derivative contract has another derivative contract as its underlying (eg swaptions) and may
            // be physically exercised into the underlying contract (ie a bank would assume a position in the underlying
            // contract in the event of exercise), then maturity of the contract is the final settlement date of the
            // underlying derivative contract. This should however be taken care of in the trade's own 'maturityDate()'
            // logic
            const Date matDate = trade->maturity();
            tradeData.M = matDate <= today ? 0.0 : dc.yearFraction(today, matDate);

            // For our current FX product coverage these are not used
            // tradeData_[i].E
            // tradeData_[i].S

            // Maturity Factor
            // Unmargined: MF = sqrt(min(1, M)) where maturity M is in years & floored at ten business days
            // Margined: MF = 1.5 * sqrt(MPR) where MPR is in years
            // - MPR = 10 business days, non-centrally cleared
            // - MPR = 5 bus days, centrally cleared
            // - MPR = 20 bus days, non-centrally cleared, netting set > 5000 trades
            // - double MPR for netting sets with outstanding disputes

            QuantLib::ext::shared_ptr<CounterpartyInformation> cp = counterpartyManager_->get(tradeData.cpty);
            QuantLib::ext::shared_ptr<NettingSetDefinition> ndef = nettingSetManager_->get(tradeData.nettingSetDetails);

            if (ndef->activeCsaFlag()) {
                QL_REQUIRE(ndef->csaDetails()->marginPeriodOfRisk().units() == Weeks, "MPOR is expected in weeks");
                Real MPORinWeeks = weeks(ndef->csaDetails()->marginPeriodOfRisk());
                if (tradeCount[tradeData.nettingSetDetails] > 5000 && !cp->isClearingCP())
                    MPORinWeeks = 4.0;
                tradeData.MF = 1.5 * std::sqrt(MPORinWeeks / 52.0);
            } else {
                const Real m = std::max(tradeData.M, 2.0 / 52.0);
                tradeData.MF = std::sqrt(std::min(m, 1.0));
            }

            if (tradeData.assetClass == SACCR::AssetClass::IR || tradeData.assetClass == SACCR::AssetClass::Credit) {
                tradeData.S = getStartDate(trade, today, dc);
                tradeData.E = getEndDate(trade, today, dc);
            }


            // these next fields are tradeType specific

            // if the trade is an option then this is the latest expiry date
            tradeData.T = getLatestExpiryTime(trade, today, dc);
            const auto& [hedgingSet, hedgingSubset] = getHedgingSet(trade, tradeData);
            tradeData.hedgingSet = hedgingSet;
            if (hedgingSubset)
                tradeData.hedgingSubset = *hedgingSubset;

            tradeData.currentNotional = getCurrentNotional(trade, tradeData.assetClass, baseCurrency_, dc, tradeData.currentPrice1, tradeData.currentPrice2,
                                                           tradeData.hedgingSet, tradeData.hedgingSubset);

            tradeData.delta = getDelta(trade, tradeData, today);
            tradeData.SD = getSupervisoryDuration(tradeData);

            // FIXME: Hard-coding for CommSwaps - we want to handle this generally
            // Currently, the only time we have negative currentNotional is for float-float with same underlyings.
            // In this case, we want delta to 
            if (trade->tradeType() == "CommoditySwap") {
                tradeData.delta = tradeData.currentNotional > 0.0 ? 1.0 : -1.0;
                tradeData.currentNotional = std::fabs(tradeData.currentNotional);
            }

            tradeData.d = tradeData.SD == Null<Real>() ? tradeData.currentNotional : tradeData.SD * tradeData.currentNotional;

            tradeData_.push_back(tradeData);

            // build up nettingSet -> counterparty map for the aggregation step
            if (nettingSetToCpty_.find(tradeData.nettingSetDetails) != nettingSetToCpty_.end()) {
                nettingSetToCpty_[tradeData.nettingSetDetails].insert(tradeData.cpty);
            } else {
                nettingSetToCpty_.insert({tradeData.nettingSetDetails, {tradeData.cpty}});
            }

            // Validate nettingSet-to-counterParty map. Only allow 1-to-1 and many-to-1 mappings.
            for (const auto& n : nettingSetToCpty_) {
                if (n.second.size() > 1) {
                    StructuredAnalyticsWarningMessage(
                        "SA-CCR", "Invalid netting set and counterparty pair",
                        ore::data::to_string(n.first) +
                            ": Found more than one counterparty associated with this netting set."
                            "The first SA-CCR risk weight found will be used for this netting set."
                            " Please check the netting set and counterparty details in the portfolio.")
                        .log();
                }
            }

            DLOG("SA-CCR: Trade details processed for trade " << tradeData.id << ", " << tradeData.nettingSetDetails);
        } catch (const std::exception& e) {
            StructuredTradeErrorMessage(trade, "SA-CCR: Trade failed to process", e.what()).log();
        }
    }

    // Check if at least one trade has optional netting set detail fields populated. If not, then we will exclude
    // these optional fields from the final reports.
    for (const NettingSetDetails& nettingSetDetails : nettingSets_) {
        if (!nettingSetDetails.emptyOptionalFields()) {
            hasNettingSetDetails_ = true;
            break;
        }
        hasNettingSetDetails_ = false;
    }

    // Make sure every netting set has an associated counterparty - some netting sets may not have trades associated with it
    for (const NettingSetDetails& nettingSetDetails : nettingSets_) {
        if (nettingSetToCpty_.find(nettingSetDetails) == nettingSetToCpty_.end())
            nettingSetToCpty_[nettingSetDetails] = {saCcrDefaults_.cptyInfo.counterpartyId};
    }
    LOG("SA-CCR: Collecting trade contributions done");
}

void SACCR::aggregate() {
    DLOG("SA-CCR: Aggregation");

    // NPV aggregation, initialization
    map<NettingSetDetails, Real> grossNPV;

    for (const NettingSetDetails& nettingSetDetails : nettingSets_) {
        NPV_[nettingSetDetails] = 0.0;
        RC_[nettingSetDetails] = 0.0;
        addOn_[nettingSetDetails] = 0.0;
        PFE_[nettingSetDetails] = 0.0;
        multiplier_[nettingSetDetails] = 0.0;
        grossNPV[nettingSetDetails] = 0.0;
    }

    for (Size i = 0; i < tradeData_.size(); i++) {
        const TradeData& td = tradeData_.at(i);
        const NettingSetDetails& nettingSetDetails = td.nettingSetDetails;
        SACCR::AssetClass assetClass = td.assetClass;
        string hedgingSet = td.hedgingSet;
        NPV_[nettingSetDetails] += td.NPV;
        totalNPV_ += td.NPV;
        grossNPV[nettingSetDetails] += std::max(td.NPV, 0.0);
        AssetClassKey key(nettingSetDetails, assetClass);
        if (addOnAssetClass_.find(key) == addOnAssetClass_.end())
            addOnAssetClass_[key] = 0.0;
        if (npvAssetClass_.find(key) == npvAssetClass_.end())
            npvAssetClass_[key] = 0.0;
        HedgingSetKey key2(nettingSetDetails, assetClass, hedgingSet);
        if (addOnHedgingSet_.find(key2) == addOnHedgingSet_.end())
            addOnHedgingSet_[key2] = 0.0;
        if (npvHedgingSet_.find(key2) == npvHedgingSet_.end())
            npvHedgingSet_[key2] = 0.0;
    }

    // Build list of collateral balances in base currency
    DLOG("SA-CCR: Building list of collateral balances");
    for (const NettingSetDetails& nettingSetDetails : nettingSets_) {
        const auto& nsd = nettingSetManager_->get(nettingSetDetails);
        DLOG("Building collateral balances for netting set:" << nettingSetDetails);
        if (nsd->activeCsaFlag()) {
            QuantLib::ext::shared_ptr<CollateralBalance> cb = nullptr;
            Real cbFxQuote = 1.0;
            if (collateralBalances_->has(nettingSetDetails)) {
                cb = collateralBalances_->get(nettingSetDetails);
                cbFxQuote = getFxRate(cb->currency());
            }

            QuantLib::ext::shared_ptr<CollateralBalance> ccb = nullptr;
            const bool hasCcb = calculatedCollateralBalances_->has(nettingSetDetails);
            if (nsd->csaDetails()->calculateIMAmount() && hasCcb)
                ccb = calculatedCollateralBalances_->get(nettingSetDetails);

            // Initial margin
            Real initialMargin = QuantLib::Null<Real>();
            if (nsd->csaDetails()->calculateIMAmount()) {
                // InitialMargin = SIMM-generated IM, unless an overriding balance was provided, in which case we use
                // the balance provided.
                if (cb && cb->initialMargin() != QuantLib::Null<Real>() && defaultIMBalances_.find(nettingSetDetails) ==
                              defaultIMBalances_.end()) {
                    initialMargin = cbFxQuote * cb->initialMargin();
                } else {
                    const Real ccbFxQuote = hasCcb ? getFxRate(ccb->currency()) : 1.0;
                    initialMargin = hasCcb ? ccbFxQuote * ccb->initialMargin() : 0.0;
                }
            } else {
                // If no balance was provided, and calculateIMAmount=false, the calculation should fail
                if (!cb || cb->initialMargin() == Null<Real>()) {
                    auto msg = StructuredConfigurationErrorMessage(
                        "Collateral balances", ore::data::to_string(nettingSetDetails),
                        "Inconsistent netting set configurations",
                        "CalculateIMAmount was set to \'false\' in the netting set "
                        "definition, but no InitialMargin was "
                        "provided in the collateral balance.");
                    msg.log();
                    QL_FAIL(msg.msg());
                } else {
                    initialMargin = cbFxQuote * cb->initialMargin();
                }
            }
            amountsBase_[nettingSetDetails].im = initialMargin;

            // Variation margin
            Real variationMargin = QuantLib::Null<Real>();
            if (nsd->csaDetails()->calculateVMAmount()) {
                // VariationMargin = NPV, unless an overriding balance was provided, in which case we use the balance
                // provided.
                if (cb && cb->variationMargin() != QuantLib::Null<Real>() &&
                    defaultVMBalances_.find(nettingSetDetails) == defaultVMBalances_.end()) {
                    variationMargin = cbFxQuote * cb->variationMargin();
                } else {
                    variationMargin = NPV_[nettingSetDetails];
                }
            } else {
                // If no balance was provided, even though calculateVMAmount=false, then the calculation should fail
                if (!cb || cb->variationMargin() == Null<Real>()) {
                    auto msg = StructuredConfigurationErrorMessage(
                        "Collateral balances", ore::data::to_string(nettingSetDetails),
                        "Inconsistent netting set configurations",
                        "CalculateVMAmount was set to \'false\' in the netting set definition, but no VariationMargin "
                        "was provided in the collateral balance.");
                    msg.log();
                    QL_FAIL(msg.msg());
                } else {
                    variationMargin = cbFxQuote * cb->variationMargin();
                }
            }
            amountsBase_[nettingSetDetails].vm = variationMargin;

            // Get FX rate for amounts in the netting set definition
            string csaCcy = nsd->csaDetails()->csaCurrency();
            if (csaCcy.empty())
                csaCcy = baseCurrency_;
            const Real csaFxQuote = getFxRate(csaCcy);

            // Independent amount from CSA details
            Real independentAmounHeld = csaFxQuote * nsd->csaDetails()->independentAmountHeld();
            amountsBase_[nettingSetDetails].iah = independentAmounHeld;

            // Minimum transfer amount
            amountsBase_[nettingSetDetails].mta = csaFxQuote * nsd->csaDetails()->mtaRcv();

            // Threshould amount
            amountsBase_[nettingSetDetails].tha = csaFxQuote * nsd->csaDetails()->thresholdRcv();
        } else {
            // If netting set is uncollateralised
            amountsBase_[nettingSetDetails].im = 0.0;
            amountsBase_[nettingSetDetails].vm = 0.0;
            amountsBase_[nettingSetDetails].iah = 0.0;
            amountsBase_[nettingSetDetails].mta = 0.0;
            amountsBase_[nettingSetDetails].tha = 0.0;
        }
    }

    // Make sure that all amounts in each netting set has been filled (either zero or any other non-trivial value)
    for (const auto& ns : amountsBase_) {
        if (ns.second.im == Null<Real>()) {
            StructuredAnalyticsErrorMessage("SA-CCR", "Aggregating netting set initial margin",
                                            "Initial margin must not be null for [" + ore::data::to_string(ns.first) +
                                                "]. Please check that the inputs are valid.")
                .log();
        }
        if (ns.second.vm == Null<Real>()) {
            StructuredAnalyticsErrorMessage("SA-CCR", "Aggregating netting set variation margin",
                                            "Variation margin must not be null for [" + ore::data::to_string(ns.first) +
                                                "]. Please check that the inputs are valid.")
                .log();
        }
        if (ns.second.iah == Null<Real>()) {
            StructuredAnalyticsErrorMessage("SA-CCR", "Aggregating netting set independent amount",
                                            "Independent amount must not be null for [" +
                                                ore::data::to_string(ns.first) +
                                                "]. Please check that the inputs are valid.")
                .log();
        }
    }

    // RC calculation
    DLOG("SA-CCR RC calculation");
    // Get CSA details and collateral balance and compute replacement cost per netting set
    for (map<NettingSetDetails, Real>::iterator it = NPV_.begin(); it != NPV_.end(); it++) {

        const NettingSetDetails nettingSetDetails = it->first;

        QuantLib::ext::shared_ptr<NettingSetDefinition> ndef = nettingSetManager_->get(nettingSetDetails);

        const Real independentAmountHeld = amountsBase_[nettingSetDetails].iah;
        const Real initialMargin = amountsBase_[nettingSetDetails].im;
        const Real variationMargin = amountsBase_[nettingSetDetails].vm;
        const Real mta = amountsBase_[nettingSetDetails].mta;
        const Real th = amountsBase_[nettingSetDetails].tha;

        const Real nica = independentAmountHeld + initialMargin;
        const Real C = variationMargin + nica;

        RC_[nettingSetDetails] = std::max(NPV_[nettingSetDetails] - C, std::max(th + mta - nica, 0.0));

        DLOG("RC for [" << nettingSetDetails << "]: RC=" << RC_[nettingSetDetails] << " NPV=" << NPV_[nettingSetDetails]
                        << " VM=" << variationMargin << " IM=" << initialMargin << " C=" << C << " TH=" << th
                        << " MTA=" << mta << " NICA=" << nica);
    }

    // Hedging set AddOn calculation
    DLOG("SA-CCR: Hedging set AddOn calculation");
    for (map<HedgingSetKey, Real>::iterator it = addOnHedgingSet_.begin(); it != addOnHedgingSet_.end(); it++) {
        Real D1 = 0, D2 = 0, D3 = 0;
        set<HedgingSubsetKey> commoditySubsetKeys;
        map<HedgingSubsetKey, bool> equitySubsetKeys;
        for (Size i = 0; i < tradeData_.size(); i++) {
            const TradeData& td = tradeData_.at(i);
            const NettingSetDetails& nettingSetDetails = td.nettingSetDetails;
            const SACCR::AssetClass assetClass = td.assetClass;
            string hedgingSet = td.hedgingSet;
            HedgingSetKey key(nettingSetDetails, assetClass, hedgingSet);
            if (it->first != key)
                continue;

            npvHedgingSet_[key] += td.NPV;

            // Effective notional
            if (assetClass == SACCR::AssetClass::IR) {
                if (td.M < 1.0)
                    D1 += td.delta * td.d * td.MF;
                else if (td.M <= 5.0)
                    D2 += td.delta * td.d * td.MF;
                else
                    D3 += td.delta * td.d * td.MF;
            } else if (assetClass == SACCR::AssetClass::FX) {
                effectiveNotional_[key] += td.delta * td.d * td.MF;
            } else if (assetClass == SACCR::AssetClass::Commodity || assetClass == SACCR::AssetClass::Equity) {
                HedgingSubsetKey subsetKey(nettingSetDetails, assetClass, hedgingSet, td.hedgingSubset);
                subsetEffectiveNotional_[subsetKey] += td.delta * td.d * td.MF;
                if (assetClass == SACCR::AssetClass::Commodity)
                    commoditySubsetKeys.insert(subsetKey);
                else
                    equitySubsetKeys.insert(make_pair(subsetKey, td.isEquityIndex));
            } else
                QL_FAIL("asset class " << assetClass << " not covered");
        }

        // Add-ons
        const NettingSetDetails& nettingSetDetails = QuantLib::ext::get<0>(it->first);
        const SACCR::AssetClass& assetClass = QuantLib::ext::get<1>(it->first);
        const string& hedgingSet = QuantLib::ext::get<2>(it->first);
        if (assetClass == SACCR::AssetClass::IR) {
            effectiveNotional_[it->first] =
	        std::sqrt(D1 * D1 + D2 * D2 + D3 * D3 + 1.4 * (D1 * D2 + D2 * D3) + 0.6 * D1 * D3);
            Real supervisoryFactor = 0.005; // 0.5%
            addOnHedgingSet_[it->first] = supervisoryFactor * effectiveNotional_[it->first];
            DLOG("AddOn for [" << nettingSetDetails << "]/" << assetClass << "/" << hedgingSet << ": "
                               << addOnHedgingSet_[it->first]);
        } else if (assetClass == SACCR::AssetClass::FX) {
            Real supervisoryFactor = 0.04; // 4%
            addOnHedgingSet_[it->first] = supervisoryFactor * fabs(effectiveNotional_[it->first]);
            DLOG("AddOn for [" << nettingSetDetails << "]/" << assetClass << "/" << hedgingSet << ": "
                               << addOnHedgingSet_[it->first]);

        } else if (assetClass == SACCR::AssetClass::Commodity) {
            Real addonType = 0;
            Real addonTypeSquared = 0;
            for (auto& s : commoditySubsetKeys) {
                const string& hedgingSubset = QuantLib::ext::get<3>(s);
                const Real supervisoryFactor = hedgingSubset == "Power" ? 0.4 : 0.18;
                const Real tmp = supervisoryFactor * subsetEffectiveNotional_.at(s);
                addonType += tmp;
                addonTypeSquared += tmp * tmp;
            }
            const constexpr Real corr = 0.4;
            addOnHedgingSet_[it->first] =
                std::sqrt((corr * addonType) * (corr * addonType) + (1 - corr * corr) * addonTypeSquared);
        } else if (assetClass == SACCR::AssetClass::Equity) {
            // Close duplicate of commodity logic
            Real addonType = 0;
            Real addonTypeSquared = 0;
            for (auto& [subsetKey, isEquityIndex] : equitySubsetKeys) {
                const Real supervisoryFactor = isEquityIndex ? 0.2 : 0.32;
                const Real corr = isEquityIndex ? 0.8 : 0.5;
                const Real tmp = supervisoryFactor * subsetEffectiveNotional_.at(subsetKey);
                addonType += corr * tmp;
                addonTypeSquared += (1 - corr*corr) * tmp * tmp;
            }
            addOnHedgingSet_[it->first] = std::sqrt(addonType * addonType + addonTypeSquared);
        } else
            QL_FAIL("asset class " << assetClass << " not covered");

        // For hedging sets consisting of basis transactions,
        // the supervisory factor applicable to a given asset class must be multiplied by one-half.
        if (basisHedgingSets_.find(hedgingSet) != basisHedgingSets_.end())
            addOnHedgingSet_[it->first] *= 0.5;
    }

    // Asset class AddOn calculation, pure aggregation across matching hedging sets
    DLOG("SA-CCR: Asset Class AddOn calculation");
    for (map<AssetClassKey, Real>::iterator it = addOnAssetClass_.begin(); it != addOnAssetClass_.end(); it++) {
        for (map<HedgingSetKey, Real>::iterator ith = addOnHedgingSet_.begin(); ith != addOnHedgingSet_.end(); ith++) {
            NettingSetDetails nettingSetDetails = QuantLib::ext::get<0>(ith->first);
            SACCR::AssetClass assetClass = QuantLib::ext::get<1>(ith->first);
            // string hedgingSet = QuantLib::ext::get<2>(it->first);
            AssetClassKey key(nettingSetDetails, assetClass);
            if (it->first != key)
                continue;
            it->second += ith->second;
            npvAssetClass_[key] += npvHedgingSet_[ith->first];
        }
    }

    // Netting set AddOn calculation, pure aggregation across asset classes
    // Multiplier
    // PFE
    // EAD
    DLOG("SA-CCR: Aggregate AddOn and EAD calculation");
    for (map<NettingSetDetails, Real>::iterator it = addOn_.begin(); it != addOn_.end(); it++) {
        NettingSetDetails nettingSetDetails = it->first;

        for (map<AssetClassKey, Real>::iterator ita = addOnAssetClass_.begin(); ita != addOnAssetClass_.end(); ita++) {
            NettingSetDetails aNettingSetDetails = ita->first.first;
            if (nettingSetDetails != aNettingSetDetails)
                continue;
            it->second += ita->second;
        }

        QuantLib::ext::shared_ptr<NettingSetDefinition> ndef = nettingSetManager_->get(nettingSetDetails);

        const Real independentAmountHeld = amountsBase_[nettingSetDetails].iah;
        const Real initialMargin = amountsBase_[nettingSetDetails].im;
        const Real variationMargin = amountsBase_[nettingSetDetails].vm;

        const Real nica = independentAmountHeld + initialMargin;
        const Real C = variationMargin + nica;

        const Real V = NPV_[nettingSetDetails];
        const Real A = addOn_[nettingSetDetails];
        multiplier_[nettingSetDetails] = std::min(1.0, 0.05 + 0.95 * std::exp((V - C) / (2.0 * 0.95 * A)));
        PFE_[nettingSetDetails] = multiplier_[nettingSetDetails] * addOn_[nettingSetDetails];
        const constexpr Real alpha = 1.4;
        Real ead = alpha * (RC_[nettingSetDetails] + PFE_[nettingSetDetails]);
        EAD_[nettingSetDetails] = ead;

        // Get the counterparty
        string cpStr = *nettingSetToCpty_[nettingSetDetails].begin();
        QL_REQUIRE(!cpStr.empty(), "Netting set does not contain valid counterparty");
        QuantLib::ext::shared_ptr<CounterpartyInformation> cp = counterpartyManager_->get(cpStr);

        Real rw = cp->saCcrRiskWeight();
        RW_[nettingSetDetails] = rw;

        Real cc = ead * rw;
        CC_[nettingSetDetails] = cc;
        totalCC_ += cc;
    }

    nettingSetDetails_.clear();
    for (map<NettingSetDetails, Real>::iterator it = addOn_.begin(); it != addOn_.end(); it++)
        nettingSetDetails_.push_back(it->first);

    assetClasses_.clear();
    for (map<AssetClassKey, Real>::iterator it = addOnAssetClass_.begin(); it != addOnAssetClass_.end(); it++) {
        NettingSetDetails nettingSetDetails = it->first.first;
        if (assetClasses_.find(nettingSetDetails) == assetClasses_.end())
            assetClasses_[nettingSetDetails] = vector<SACCR::AssetClass>();
        assetClasses_[nettingSetDetails].push_back(it->first.second);
    }

    hedgingSets_.clear();
    for (map<HedgingSetKey, Real>::iterator it = addOnHedgingSet_.begin(); it != addOnHedgingSet_.end(); it++) {
        NettingSetDetails nettingSetDetails = QuantLib::ext::get<0>(it->first);
        SACCR::AssetClass ac = QuantLib::ext::get<1>(it->first);
        pair<NettingSetDetails, SACCR::AssetClass> key(nettingSetDetails, ac);
        if (hedgingSets_.find(key) == hedgingSets_.end())
            hedgingSets_[key] = vector<string>();
        hedgingSets_[key].push_back(QuantLib::ext::get<2>(it->first));
    }

    if (reports_.size() > 0)
        writeReports();

    DLOG("SA-CCR: Aggregation done");
}

void SACCR::combineCollateralBalances() {
    DLOG("Combining collateral balances.");

    // Get unique list of netting sets from the balances
    set<NettingSetDetails> uniqueNettingSets;
    if (collateralBalances_) {
        for (const auto& cb : collateralBalances_->collateralBalances()) {
            uniqueNettingSets.insert(cb.first);
        }
    }
    if (calculatedCollateralBalances_)
        for (const auto& ccb : calculatedCollateralBalances_->collateralBalances()) {
            uniqueNettingSets.insert(ccb.first);
        }

    // Add calculated collateral balances to collateral balances file under the same netting set
    for (const NettingSetDetails& nettingSetDetails : uniqueNettingSets) {
        if (collateralBalances_->has(nettingSetDetails)) {
            const QuantLib::ext::shared_ptr<CollateralBalance>& cb = collateralBalances_->get(nettingSetDetails);

            // SIMM-generated IM and NPV-based VM are both in terms of base ccy, so we convert back to the currency
            // of the original collateral balance file.
            const Real cbFxQuote = getFxRate(cb->currency());

            // Initial margin
            if (cb->initialMargin() == Null<Real>()) {
                cb->initialMargin() = amountsBase_[nettingSetDetails].im / cbFxQuote;
            }

            // Variation margin
            if (cb->variationMargin() == Null<Real>()) {
                cb->variationMargin() = amountsBase_[nettingSetDetails].vm / cbFxQuote;
            }

        } else {
            if (calculatedCollateralBalances_ && calculatedCollateralBalances_->has(nettingSetDetails)) {
                QuantLib::ext::shared_ptr<CollateralBalance> ccb =
                    QuantLib::ext::make_shared<CollateralBalance>(*calculatedCollateralBalances_->get(nettingSetDetails));
                ccb->variationMargin() = amountsBase_[nettingSetDetails].vm;

                collateralBalances_->add(ccb);
            }
        }
    }
}

const vector<SACCR::AssetClass>& SACCR::assetClasses(NettingSetDetails nettingSetDetails) const {
    auto assetClassesIt = assetClasses_.find(nettingSetDetails);
    QL_REQUIRE(assetClassesIt != assetClasses_.end(), "netting set not found in asset class map");
    return assetClassesIt->second;
}

const vector<string>& SACCR::hedgingSets(NettingSetDetails nettingSetDetails, SACCR::AssetClass assetClass) const {
    pair<NettingSetDetails, SACCR::AssetClass> key(nettingSetDetails, assetClass);
    auto hedgingSetsIt = hedgingSets_.find(key);
    QL_REQUIRE(hedgingSetsIt != hedgingSets_.end(),
               "netting set and asset class not found in hedging set map");
    return hedgingSetsIt->second;
}

Real SACCR::EAD(NettingSetDetails nettingSetDetails) const {
    auto eadIt = EAD_.find(nettingSetDetails);
    QL_REQUIRE(eadIt != EAD_.end(), "netting set " << nettingSetDetails << " not found in EAD");
    return eadIt->second;
}

Real SACCR::EAD(string nettingSet) const {
    NettingSetDetails nettingSetDetails(nettingSet);
    return EAD(nettingSetDetails);
}

Real SACCR::RW(NettingSetDetails nettingSetDetails) const {
    auto rwIt = RW_.find(nettingSetDetails);
    QL_REQUIRE(rwIt != RW_.end(), "netting set " << nettingSetDetails << " not found in RW");
    return rwIt->second;
}

Real SACCR::CC(NettingSetDetails nettingSetDetails) const {
    auto ccIt = CC_.find(nettingSetDetails);
    QL_REQUIRE(ccIt != CC_.end(), "netting set " << nettingSetDetails << " not found in CC");
    return ccIt->second;
}

Real SACCR::NPV(NettingSetDetails nettingSetDetails) const {
    auto npvIt = NPV_.find(nettingSetDetails);
    QL_REQUIRE(npvIt != NPV_.end(), "netting set " << nettingSetDetails << " not found in NPV");
    return npvIt->second;
}

Real SACCR::NPV(NettingSetDetails nettingSetDetails, SACCR::AssetClass assetClass) const {
    AssetClassKey key(nettingSetDetails, assetClass);
    auto npvAssetClassIt = npvAssetClass_.find(key);
    QL_REQUIRE(npvAssetClassIt != npvAssetClass_.end(),
               "netting set " << nettingSetDetails << " and " << assetClass << " not found in npvAssetClass");
    return npvAssetClassIt->second;
}

Real SACCR::NPV(NettingSetDetails nettingSetDetails, SACCR::AssetClass assetClass, string hedgingSet) const {
    HedgingSetKey key(nettingSetDetails, assetClass, hedgingSet);
    auto npvHedgingSetIt = npvHedgingSet_.find(key);
    QL_REQUIRE(npvHedgingSetIt != npvHedgingSet_.end(),
               "netting set " << nettingSetDetails << ", asset class " << assetClass << ", hedging set " << hedgingSet
                              << " not found in npvAssetClass");
    return npvHedgingSetIt->second;
}

Real SACCR::RC(NettingSetDetails nettingSetDetails) const {
    auto rcIt = RC_.find(nettingSetDetails);
    QL_REQUIRE(rcIt != RC_.end(), "netting set " << nettingSetDetails << " not found in RC");
    return rcIt->second;
}

Real SACCR::PFE(NettingSetDetails nettingSetDetails) const {
    auto pfeIt = PFE_.find(nettingSetDetails);
    QL_REQUIRE(pfeIt != PFE_.end(), "netting set " << nettingSetDetails << " not found in PFE");
    return pfeIt->second;
}

Real SACCR::multiplier(NettingSetDetails nettingSetDetails) const {
    auto multiplierIt = multiplier_.find(nettingSetDetails);
    QL_REQUIRE(multiplierIt != multiplier_.end(),
               "netting set " << nettingSetDetails << " not found in multiplier");
    return multiplierIt->second;
}

Real SACCR::addOn(NettingSetDetails nettingSetDetails) const {
    auto addOnIt = addOn_.find(nettingSetDetails);
    QL_REQUIRE(addOnIt != addOn_.end(),
               "netting set " << nettingSetDetails << " not found in addOn");
    return addOnIt->second;
}

Real SACCR::addOn(NettingSetDetails nettingSetDetails, SACCR::AssetClass assetClass) const {
    AssetClassKey key(nettingSetDetails, assetClass);
    auto addOnIt = addOnAssetClass_.find(key);
    QL_REQUIRE(addOnIt != addOnAssetClass_.end(),
               "netting set " << nettingSetDetails << " and " << assetClass << " not found in addOnAssetClass");
    return addOnIt->second;
}

Real SACCR::addOn(NettingSetDetails nettingSetDetails, SACCR::AssetClass assetClass, string hedgingSet) const {
    HedgingSetKey key(nettingSetDetails, assetClass, hedgingSet);
    auto addOnHedgingSetIt = addOnHedgingSet_.find(key);
    QL_REQUIRE(addOnHedgingSetIt != addOnHedgingSet_.end(),
               "netting set " << nettingSetDetails << ", asset class " << assetClass << ", hedging set " << hedgingSet
                              << " not found in addOnAssetClass");
    return addOnHedgingSetIt->second;
}

void SACCR::writeReports() {

    LOG("writing reports");
    auto detailReport = reports_.find(ReportType::Detail);
    if (detailReport != reports_.end()) {
        detailReport->second
            ->addColumn("TradeId", string())
            .addColumn("TradeType", string())
            .addColumn("NettingSet", string());

        if (hasNettingSetDetails_) {
            for (const auto& nettingSetField : NettingSetDetails::optionalFieldNames())
                detailReport->second->addColumn(nettingSetField, string());
        }

        detailReport->second
            ->addColumn("AssetClass", string())
            .addColumn("HedgingSet", string())
            .addColumn("HedgingSubset", string())
            .addColumn("NPV", Real(), 2)
            .addColumn("NpvCcy", string())
            .addColumn("SD", Real())
            .addColumn("delta", Real(), 4)
            .addColumn("d", Real(), 4)
            .addColumn("MF", Real(), 7)
            .addColumn("M", Real(), 4)
            .addColumn("S", Real(), 4)
            .addColumn("E", Real(), 4)
            .addColumn("T", Real(), 4)
            .addColumn("CurrentPrice1", Real(), 6)
            .addColumn("CurrentPrice2", Real(), 6)
            .addColumn("NumNominalFlows", Size())
            .addColumn("Price", Real(), 4)
            .addColumn("Strike", Real(), 4);

        for (const TradeData& td : tradeData_) {
            detailReport->second->next().add(td.id).add(td.type);

            map<string, string> nettingSetMap = td.nettingSetDetails.mapRepresentation();
            for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                detailReport->second->add(nettingSetMap[fieldName]);

            detailReport->second
                ->add(ore::data::to_string(td.assetClass))
                .add(td.hedgingSet)
                .add(td.hedgingSubset)
                .add(td.NPV)
                .add(td.npvCcy)
                .add(td.SD)
                .add(td.delta)
                .add(td.d)
                .add(td.MF)
                .add(td.M)
                .add(td.S)
                .add(td.E)
                .add(td.T)
                .add(td.currentPrice1)
                .add(td.currentPrice2)
                .add(td.numNominalFlows)
                .add(td.price)
                .add(td.strike);
        }
        detailReport->second->end();
    }

    auto summaryReportIt = reports_.find(ReportType::Summary);
    if (summaryReportIt != reports_.end()) {
        summaryReportIt->second->addColumn("NettingSet", string());

        if (hasNettingSetDetails_) {
            for (const auto& nettingSetField : NettingSetDetails::optionalFieldNames())
                summaryReportIt->second->addColumn(nettingSetField, string());
        }

        summaryReportIt->second
            ->addColumn("AssetClass", string())
            .addColumn("HedgingSet", string())
            .addColumn("AddOn", string())
            .addColumn("NPV", string())
            .addColumn("IndependentAmountHeld", string())
            .addColumn("InitialMargin", string())
            .addColumn("VariationMargin", string())
            .addColumn("ThresholdAmount", string())
            .addColumn("MinimumTransferAmount", string())
            .addColumn("RC", string())
            .addColumn("Multiplier", string())
            .addColumn("PFE", string())
            .addColumn("EAD", string())
            .addColumn("RW", string())
            .addColumn("CC", string());

        summaryReportIt->second->next();

        Size numNettingSetFields = NettingSetDetails::fieldNames(hasNettingSetDetails_).size();
        for (Size t = 0; t < numNettingSetFields; t++)
            summaryReportIt->second->add("All");

        // Portfolio level
        summaryReportIt->second
            ->add("All")
            .add("All")
            .add("")
            .add(std::to_string(NPV()))
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add("")
            .add(std::to_string(CC()));

        for (const NettingSetDetails& nettingSetDetails : nettingSetDetails_) {
            const vector<SACCR::AssetClass>& assetClasses = assetClasses_[nettingSetDetails];

            // Netting set level
            summaryReportIt->second->next();

            map<string, string> nettingSetMap = nettingSetDetails.mapRepresentation();
            for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                summaryReportIt->second->add(nettingSetMap[fieldName]);

            summaryReportIt->second
                ->add("All")
                .add("All")
                .add(std::to_string(addOn(nettingSetDetails)))
                .add(std::to_string(NPV(nettingSetDetails)))
                .add(std::to_string(amountsBase_[nettingSetDetails].iah))
                .add(std::to_string(amountsBase_[nettingSetDetails].im))
                .add(std::to_string(amountsBase_[nettingSetDetails].vm))
                .add(std::to_string(amountsBase_[nettingSetDetails].tha))
                .add(std::to_string(amountsBase_[nettingSetDetails].mta))
                .add(std::to_string(RC(nettingSetDetails)))
                .add(std::to_string(multiplier(nettingSetDetails)))
                .add(std::to_string(PFE(nettingSetDetails)))
                .add(std::to_string(EAD(nettingSetDetails)))
                .add(std::to_string(RW(nettingSetDetails)))
                .add(std::to_string(CC(nettingSetDetails)));

            for (const SACCR::AssetClass& assetClass : assetClasses) {
                // Asset class level
                summaryReportIt->second->next();

                for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                    summaryReportIt->second->add(nettingSetMap[fieldName]);

                summaryReportIt->second
                    ->add(to_string(assetClass))
                    .add("All") 
                    .add(std::to_string(addOn(nettingSetDetails, assetClass)))
                    .add(std::to_string(NPV(nettingSetDetails, assetClass)))
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("")
                    .add("");

                const vector<string>& hedgingSets = this->hedgingSets(nettingSetDetails, assetClass);
                for (const string& hedgingSet : hedgingSets) {
                    // Hedging set level
                    summaryReportIt->second->next();

                    for (const auto& fieldName : NettingSetDetails::fieldNames(hasNettingSetDetails_))
                        summaryReportIt->second->add(nettingSetMap[fieldName]);

                    summaryReportIt->second
                        ->add(to_string(assetClass))
                        .add(hedgingSet)
                        .add(std::to_string(addOn(nettingSetDetails, assetClass, hedgingSet)))
                        .add(std::to_string(NPV(nettingSetDetails, assetClass, hedgingSet)))
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("")
                        .add("");
                }
            }
        }
        summaryReportIt->second->end();
    }
}

} // namespace analytics
} // namespace ore
