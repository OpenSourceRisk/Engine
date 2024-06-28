/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

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

#include <ored/portfolio/builders/convertiblebond.hpp>
#include <ored/portfolio/convertiblebond.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/convertiblebondreferencedata.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/utilities/bondindexbuilder.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/cashflows/bondtrscashflow.hpp>
#include <qle/instruments/convertiblebond2.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/time/calendars/jointcalendar.hpp>

#include <boost/lexical_cast.hpp>

#include <algorithm>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

std::map<AssetClass, std::set<std::string>>
ConvertibleBond::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    data_ = originalData_;
    data_.populateFromBondReferenceData(referenceDataManager);
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {data_.bondData().securityId()};
    if (!data_.conversionData().equityUnderlying().name().empty())
        result[AssetClass::EQ] = {data_.conversionData().equityUnderlying().name()};
    return result;
}

namespace {

ConvertibleBond2::ExchangeableData buildExchangeableData(const ConvertibleBondData::ConversionData& conversionData) {
    ConvertibleBond2::ExchangeableData result{false, false};
    if (conversionData.initialised() && conversionData.exchangeableData().initialised()) {
        result.isExchangeable = conversionData.exchangeableData().isExchangeable();
        result.isSecured = conversionData.exchangeableData().secured();
    }
    return result;
}

std::vector<ConvertibleBond2::CallabilityData>
buildCallabilityData(const ConvertibleBondData::CallabilityData& callData, const Date& openEndDateReplacement) {
    std::vector<ConvertibleBond2::CallabilityData> result;
    if (callData.initialised()) {
        QuantLib::Schedule schedule = makeSchedule(callData.dates(), openEndDateReplacement);
        std::vector<Date> callDatesPlusInf = schedule.dates();
        callDatesPlusInf.push_back(Date::maxDate());
        auto styles = buildScheduledVectorNormalised<std::string>(callData.styles(), callData.styleDates(),
                                                                  callDatesPlusInf, "Bermudan", true);
        auto prices = buildScheduledVectorNormalised<double>(callData.prices(), callData.priceDates(), callDatesPlusInf,
                                                             1.0, true);
        auto priceTypes = buildScheduledVectorNormalised<std::string>(callData.priceTypes(), callData.priceTypeDates(),
                                                                      callDatesPlusInf, "Clean", true);
        auto includeAccrual = buildScheduledVectorNormalised<bool>(
            callData.includeAccrual(), callData.includeAccrualDates(), callDatesPlusInf, true, true);
        auto isSoft = buildScheduledVectorNormalised<bool>(callData.isSoft(), callData.isSoftDates(), callDatesPlusInf,
                                                           false, true);
        auto triggerRatios = buildScheduledVectorNormalised<double>(
            callData.triggerRatios(), callData.triggerRatioDates(), callDatesPlusInf, 0.0, true);
        auto nOfMTriggers = buildScheduledVectorNormalised<std::string>(
            callData.nOfMTriggers(), callData.nOfMTriggerDates(), callDatesPlusInf, "0-of-0", true);

        for (Size i = 0; i < callDatesPlusInf.size() - 1; ++i) {
            ConvertibleBond2::CallabilityData::ExerciseType exerciseType;
            ConvertibleBond2::CallabilityData::PriceType priceType;
            if (styles[i] == "Bermudan") {
                exerciseType = ConvertibleBond2::CallabilityData::ExerciseType::OnThisDate;
            } else if (styles[i] == "American") {
                QL_REQUIRE(
                    callDatesPlusInf.size() > 2,
                    "for exercise style 'American' at least two dates (start, end) are required (call/put data)");
                if (i == callDatesPlusInf.size() - 2)
                    exerciseType = ConvertibleBond2::CallabilityData::ExerciseType::OnThisDate;
                else
                    exerciseType = ConvertibleBond2::CallabilityData::ExerciseType::FromThisDateOn;
            } else {
                QL_FAIL("invalid exercise style '" << styles[i] << "', expected Bermudan, American (call/put data)");
            }
            if (priceTypes[i] == "Clean") {
                priceType = ConvertibleBond2::CallabilityData::PriceType::Clean;
            } else if (priceTypes[i] == "Dirty") {
                priceType = ConvertibleBond2::CallabilityData::PriceType::Dirty;
            } else {
                QL_FAIL("invalid price type '" << priceTypes[i] << "', expected Clean, Dirty");
            }
            result.push_back(ConvertibleBond2::CallabilityData{callDatesPlusInf[i], exerciseType, prices[i], priceType,
                                                               includeAccrual[i], isSoft[i], triggerRatios[i]});
        }
    }
    return result;
} // buildCallabilityData()

ConvertibleBond2::MakeWholeData buildMakeWholeData(const ConvertibleBondData::CallabilityData& callData) {
    ConvertibleBond2::MakeWholeData d;
    if (callData.initialised() && callData.makeWholeData().initialised()) {
        auto const& crData = callData.makeWholeData().conversionRatioIncreaseData();
        if (crData.initialised()) {
            d.crIncreaseData = ConvertibleBond2::MakeWholeData::CrIncreaseData();
            (*d.crIncreaseData).cap = crData.cap().empty() ? Null<Real>() : parseReal(crData.cap());
            (*d.crIncreaseData).stockPrices = crData.stockPrices();
            std::vector<Date> dates;
            std::transform(crData.crIncreaseDates().begin(), crData.crIncreaseDates().end(), std::back_inserter(dates),
                           &parseDate);
            (*d.crIncreaseData).effectiveDates = dates;
            (*d.crIncreaseData).crIncrease = crData.crIncrease();
        }
    }
    return d;
} // buildMakeWholeData()

std::vector<ConvertibleBond2::ConversionRatioData>
buildConversionRatioData(const ConvertibleBondData::ConversionData& conversionData) {
    std::vector<ConvertibleBond2::ConversionRatioData> result;
    std::set<Date> tmp;
    if (conversionData.initialised()) {
        for (Size i = 0; i < conversionData.conversionRatios().size(); ++i) {
            Date d = conversionData.conversionRatioDates()[i].empty()
                         ? Date::minDate()
                         : parseDate(conversionData.conversionRatioDates()[i]);
            result.push_back({d, conversionData.conversionRatios()[i]});
            tmp.insert(d);
        }
    }
    // check that we have unique dates
    QL_REQUIRE(tmp.size() == result.size(), "Found " << result.size() << " conversion ratio definitions, but only "
                                                     << tmp.size()
                                                     << " unique start dates, please check for duplicates");
    return result;
} // buildConversionRatioData()

std::vector<ConvertibleBond2::ConversionRatioData>
buildConversionFixedAmountData(const ConvertibleBondData::ConversionData& conversionData) {
    std::vector<ConvertibleBond2::ConversionRatioData> result;
    std::set<Date> tmp;
    if (conversionData.initialised()) {
        for (Size i = 0; i < conversionData.fixedAmountConversionData().amounts().size(); ++i) {
            Date d = conversionData.fixedAmountConversionData().amountDates()[i].empty()
                         ? Date::minDate()
                         : parseDate(conversionData.fixedAmountConversionData().amountDates()[i]);
            result.push_back({d, conversionData.fixedAmountConversionData().amounts()[i]});
            tmp.insert(d);
        }
    }
    // check that we have unique dates
    QL_REQUIRE(tmp.size() == result.size(), "Found " << result.size()
                                                     << " conversion fixed amount definitions, but only " << tmp.size()
                                                     << " unique start dates, please check for duplicates");
    return result;
} // buildConversionFixedAmountData()

namespace {
Calendar getEqFxFixingCalendar(const QuantLib::ext::shared_ptr<EquityIndex2>& equity, const QuantLib::ext::shared_ptr<FxIndex>& fx) {
    if (fx == nullptr && equity == nullptr) {
        return NullCalendar();
    } else if (fx == nullptr && equity != nullptr) {
        return equity->fixingCalendar();
    } else if (fx != nullptr && equity == nullptr) {
        return fx->fixingCalendar();
    } else {
        return JointCalendar(equity->fixingCalendar(), fx->fixingCalendar());
    }
}
} // namespace

std::vector<ConvertibleBond2::ConversionData>
buildConversionData(const ConvertibleBondData::ConversionData& conversionData, RequiredFixings& requiredFixings,
                    const QuantLib::ext::shared_ptr<EquityIndex2>& equity, const QuantLib::ext::shared_ptr<FxIndex>& fx,
                    const std::string& fxIndexName, const Date& openEndDateReplacement) {
    std::vector<ConvertibleBond2::ConversionData> result;
    auto fixingCalendar = getEqFxFixingCalendar(equity, fx);
    if (conversionData.initialised() && conversionData.dates().hasData()) {
        QuantLib::Schedule schedule = makeSchedule(conversionData.dates(), openEndDateReplacement);
        std::vector<Date> conversionDatesPlusInf = schedule.dates();
        conversionDatesPlusInf.push_back(Date::maxDate());
        auto styles = buildScheduledVectorNormalised<std::string>(conversionData.styles(), conversionData.styleDates(),
                                                                  conversionDatesPlusInf, "Bermudan", true);

        // no need to check if initiliased, get empty vectors expanding to observations = None / barriers = 0.0
        auto cocoObservations = buildScheduledVectorNormalised<std::string>(
            conversionData.contingentConversionData().observations(),
            conversionData.contingentConversionData().observationDates(), conversionDatesPlusInf, "None", true);
        auto cocoBarriers = buildScheduledVectorNormalised<double>(
            conversionData.contingentConversionData().barriers(),
            conversionData.contingentConversionData().barrierDates(), conversionDatesPlusInf, 0.0, true);

        for (Size i = 0; i < conversionDatesPlusInf.size() - 1; ++i) {
            ConvertibleBond2::ConversionData::ExerciseType exerciseType;
            ConvertibleBond2::ConversionData::CocoType cocoType;

            if (styles[i] == "Bermudan") {
                exerciseType = ConvertibleBond2::ConversionData::ExerciseType::OnThisDate;
            } else if (styles[i] == "American") {
                QL_REQUIRE(
                    conversionDatesPlusInf.size() > 2,
                    "for exercise style 'American' at least two dates (start, end) are required (conversion data)");
                if (i == conversionDatesPlusInf.size() - 2)
                    exerciseType = ConvertibleBond2::ConversionData::ExerciseType::OnThisDate;
                else
                    exerciseType = ConvertibleBond2::ConversionData::ExerciseType::FromThisDateOn;
            } else {
                QL_FAIL("invalid exercise style '" << styles[i] << "', expected Bermudan, American (conversion data)");
            }

            QL_REQUIRE(equity != nullptr || cocoObservations[i] == "None",
                       "coco observations must be none if no equity underlying is given.");
            if (cocoObservations[i] == "Spot" ||
                exerciseType == ConvertibleBond2::ConversionData::ExerciseType::OnThisDate) {
                cocoType = ConvertibleBond2::ConversionData::CocoType::Spot;
            } else if (cocoObservations[i] == "StartOfPeriod") {
                cocoType = ConvertibleBond2::ConversionData::CocoType::StartOfPeriod;
                // for start of period coco checks we need the equity fixing
                requiredFixings.addFixingDate(fixingCalendar.adjust(conversionDatesPlusInf[i], Preceding),
                                              "EQ-" + equity->name(), conversionDatesPlusInf[i + 1] + 1);
                if (fx != nullptr) {
                    requiredFixings.addFixingDate(fixingCalendar.adjust(conversionDatesPlusInf[i], Preceding),
                                                  fxIndexName, conversionDatesPlusInf[i + 1] + 1);
                }
            } else if (cocoObservations[i] == "None") {
                cocoType = ConvertibleBond2::ConversionData::CocoType::None;
            } else {
                QL_FAIL("invalid coco observation style '" << cocoObservations[i]
                                                           << "', expected Spot, StartOfPeriod, None");
            }

            result.push_back(
                ConvertibleBond2::ConversionData{conversionDatesPlusInf[i], exerciseType, cocoType, cocoBarriers[i]});
        }
    }
    return result;
} // buildConversionData()

std::vector<ConvertibleBond2::MandatoryConversionData>
buildMandatoryConversionData(const ConvertibleBondData::ConversionData& conversionData) {
    std::vector<ConvertibleBond2::MandatoryConversionData> result;
    if (conversionData.initialised() && conversionData.mandatoryConversionData().initialised()) {
        if (conversionData.mandatoryConversionData().type() == "PEPS") {
            QL_REQUIRE(conversionData.mandatoryConversionData().pepsData().initialised(),
                       "expected peps detail data for mandatory conversion");
            result.push_back(ConvertibleBond2::MandatoryConversionData{
                parseDate(conversionData.mandatoryConversionData().date()),
                conversionData.mandatoryConversionData().pepsData().upperBarrier(),
                conversionData.mandatoryConversionData().pepsData().lowerBarrier(),
                conversionData.mandatoryConversionData().pepsData().upperConversionRatio(),
                conversionData.mandatoryConversionData().pepsData().lowerConversionRatio()});
        } else {
            QL_FAIL("invalid mandatory conversion type '" << conversionData.mandatoryConversionData().type()
                                                          << "', expected PEPS");
        }
    }
    return result;
}

std::vector<ConvertibleBond2::ConversionResetData>
buildConversionResetData(const ConvertibleBondData::ConversionData& conversionData, RequiredFixings& requiredFixings,
                         const QuantLib::ext::shared_ptr<EquityIndex2>& equity, const QuantLib::ext::shared_ptr<FxIndex>& fx,
                         const std::string& fxIndexName, const Date& openEndDateReplacement) {
    std::vector<ConvertibleBond2::ConversionResetData> result;
    auto fixingCalendar = getEqFxFixingCalendar(equity, fx);
    if (conversionData.initialised() && conversionData.conversionResetData().initialised()) {
        QL_REQUIRE(equity != nullptr || !conversionData.conversionResetData().initialised(),
                   "no conversion reset data must be specified if no equity underlying is given.");
        QuantLib::Schedule schedule =
            makeSchedule(conversionData.conversionResetData().dates(), openEndDateReplacement);
        std::vector<Date> resetDatesPlusInf = schedule.dates();
        resetDatesPlusInf.push_back(Date::maxDate());
        auto references = buildScheduledVectorNormalised<std::string>(
            conversionData.conversionResetData().references(), conversionData.conversionResetData().referenceDates(),
            resetDatesPlusInf, "InitialConversionPrice", true);
        auto thresholds = buildScheduledVectorNormalised<double>(conversionData.conversionResetData().thresholds(),
                                                                 conversionData.conversionResetData().thresholdDates(),
                                                                 resetDatesPlusInf, 0.0, true);
        auto gearings = buildScheduledVectorNormalised<double>(conversionData.conversionResetData().gearings(),
                                                               conversionData.conversionResetData().gearingDates(),
                                                               resetDatesPlusInf, 0.0, true);
        auto floors = buildScheduledVectorNormalised<double>(conversionData.conversionResetData().floors(),
                                                             conversionData.conversionResetData().floorDates(),
                                                             resetDatesPlusInf, 0.0, true);
        auto globalFloors = buildScheduledVectorNormalised<double>(
            conversionData.conversionResetData().globalFloors(),
            conversionData.conversionResetData().globalFloorDates(), resetDatesPlusInf, 0.0, true);
        for (Size i = 0; i < resetDatesPlusInf.size() - 1; ++i) {
            QL_REQUIRE(gearings[i] > 0.0 || close_enough(gearings[i], 0.0),
                       "conversion reset gearing at " << QuantLib::io::iso_date(resetDatesPlusInf[i])
                                                      << " must be non-negative (got " << gearings[i] << ")");
            QL_REQUIRE(floors[i] > 0.0 || close_enough(floors[i], 0.0),
                       "conversion reset floor at " << QuantLib::io::iso_date(resetDatesPlusInf[i])
                                                    << " must be non-negative (got " << floors[i]);
            QL_REQUIRE(globalFloors[i] > 0.0 || close_enough(globalFloors[i], 0.0),
                       "conversion reset global floor at " << QuantLib::io::iso_date(resetDatesPlusInf[i])
                                                           << " must be non-negative (got " << globalFloors[i]);
            ConvertibleBond2::ConversionResetData::ReferenceType referenceType;
            if (references[i] == "InitialConversionPrice") {
                referenceType = ConvertibleBond2::ConversionResetData::ReferenceType::InitialCP;
            } else if (references[i] == "CurrentConversionPrice") {
                referenceType = ConvertibleBond2::ConversionResetData::ReferenceType::CurrentCP;
            } else {
                QL_FAIL("invalid conversion reset reference type '"
                        << references[i] << "', expected InitialConversionPrice, CurrentConversionPrice");
            }
            result.push_back(ConvertibleBond2::ConversionResetData{referenceType, resetDatesPlusInf[i], thresholds[i],
                                                                   gearings[i], floors[i], globalFloors[i]});
            // on reset dates we need the equity fixing
            requiredFixings.addFixingDate(fixingCalendar.adjust(resetDatesPlusInf[i], Preceding),
                                          "EQ-" + equity->name());
            if (fx != nullptr) {
                requiredFixings.addFixingDate(fixingCalendar.adjust(resetDatesPlusInf[i], Preceding), fxIndexName);
            }
        }
    }
    return result;
} // buildConversionResetData()

std::vector<ConvertibleBond2::DividendProtectionData>
buildDividendProtectionData(const ConvertibleBondData::DividendProtectionData& dividendProtectionData,
                            RequiredFixings& requiredFixings, const QuantLib::ext::shared_ptr<EquityIndex2>& equity,
                            const QuantLib::ext::shared_ptr<FxIndex>& fx, const std::string& fxIndexName,
                            const Date& openEndDateReplacement) {
    std::vector<ConvertibleBond2::DividendProtectionData> result;
    auto fixingCalendar = getEqFxFixingCalendar(equity, fx);
    QL_REQUIRE(equity != nullptr || !dividendProtectionData.initialised(),
               "no dividend protection data must be given if no equity underlying is given.");
    if (dividendProtectionData.initialised()) {
        Schedule schedule = makeSchedule(dividendProtectionData.dates(), openEndDateReplacement);
        QL_REQUIRE(
            schedule.dates().size() >= 2,
            "dividend protection schedule must have at least two dates (effective dp start and first protection date)");
        std::vector<Date> divDates(std::next(schedule.dates().begin(), 1), schedule.dates().end());
        divDates.push_back(Date::maxDate());
        auto styles = buildScheduledVectorNormalised<std::string>(dividendProtectionData.adjustmentStyles(),
                                                                  dividendProtectionData.adjustmentStyleDates(),
                                                                  divDates, "CrUpOnly", true);
        auto types = buildScheduledVectorNormalised<std::string>(dividendProtectionData.dividendTypes(),
                                                                 dividendProtectionData.dividendTypeDates(), divDates,
                                                                 "Absolute", true);
        auto thresholds = buildScheduledVectorNormalised<double>(
            dividendProtectionData.thresholds(), dividendProtectionData.thresholdDates(), divDates, 0.0, true);
        for (Size i = 0; i < divDates.size() - 1; ++i) {
            ConvertibleBond2::DividendProtectionData::AdjustmentStyle adjustmentStyle;
            ConvertibleBond2::DividendProtectionData::DividendType dividendType;
            if (styles[i] == "CrUpOnly") {
                adjustmentStyle = ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly;
            } else if (styles[i] == "CrUpDown") {
                adjustmentStyle = ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown;
            } else if (styles[i] == "CrUpOnly2") {
                adjustmentStyle = ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly2;
                QL_REQUIRE(
                    types[i] == "Absolute",
                    "divident prototection adjustment style 'CrUpOnly2' is only allowed with dividend type 'Absolute'");
            } else if (styles[i] == "CrUpDown2") {
                adjustmentStyle = ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown2;
                QL_REQUIRE(
                    types[i] == "Absolute",
                    "dividend prototection adjustment style 'CrUpDown2' is only allowed with dividend type 'Absolute'");
            } else if (styles[i] == "PassThroughUpOnly") {
                adjustmentStyle = ConvertibleBond2::DividendProtectionData::AdjustmentStyle::PassThroughUpOnly;
            } else if (styles[i] == "PassThroughUpDown") {
                adjustmentStyle = ConvertibleBond2::DividendProtectionData::AdjustmentStyle::PassThroughUpDown;
            } else {
                QL_FAIL(
                    "invalid dividend protection adjustment style '"
                    << styles[i]
                    << "', expected CrUpOnly, CrUpDown, CrUpOnly2, CrUpDown2, PassThroughUpOnly, PassThroughUpDown");
            }
            if (types[i] == "Absolute") {
                dividendType = ConvertibleBond2::DividendProtectionData::DividendType::Absolute;
            } else if (types[i] == "Relative") {
                dividendType = ConvertibleBond2::DividendProtectionData::DividendType::Relative;
            } else {
                QL_FAIL("invalid dividend protection dividend type '" << types[i] << "', expected Absolute, Relative");
            }
            result.push_back(
                ConvertibleBond2::DividendProtectionData{i == 0 ? schedule.dates()[0] : divDates[i - 1] + 1,
                                                         divDates[i], adjustmentStyle, dividendType, thresholds[i]});
            // on protection dates we need the equity fixing for CrUpOnly, CrUpDown adjustment styles
            if (adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly ||
                adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown ||
                adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly2 ||
                adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown2) {
                requiredFixings.addFixingDate(fixingCalendar.adjust(schedule.dates()[i + 1], Preceding),
                                              "EQ-" + equity->name());
                if (fx != nullptr) {
                    requiredFixings.addFixingDate(fixingCalendar.adjust(schedule.dates()[i + 1], Preceding),
                                                  fxIndexName);
                }
            }
        }
    }
    return result;
} // buildDividendProtectionData()

} // namespace

void ConvertibleBond::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    DLOG("ConvertibleBond::build() called for trade " << id());

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has Convertible Bond underlyings
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    auto builder = QuantLib::ext::dynamic_pointer_cast<ConvertibleBondEngineBuilder>(engineFactory->builder("ConvertibleBond"));
    QL_REQUIRE(builder, "ConvertibleBond::build(): could not cast to ConvertibleBondBuilder, this is unexpected");

    data_ = originalData_;
    data_.populateFromBondReferenceData(engineFactory->referenceData());

    // build converible underlying bond, add to required fixings

    ore::data::Bond underlyingBond(Envelope(), data_.bondData());
    underlyingBond.build(engineFactory);
    requiredFixings_.addData(underlyingBond.requiredFixings());
    auto qlUnderlyingBond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(underlyingBond.instrument()->qlInstrument());
    QL_REQUIRE(qlUnderlyingBond,
               "ConvertibleBond::build(): internal error, could not cast underlying bond to QuantLib::Bond");
    auto qlUnderlyingBondCoupons = qlUnderlyingBond->cashflows();
    qlUnderlyingBondCoupons.erase(
        std::remove_if(qlUnderlyingBondCoupons.begin(), qlUnderlyingBondCoupons.end(),
                       [](QuantLib::ext::shared_ptr<CashFlow> c) { return QuantLib::ext::dynamic_pointer_cast<Coupon>(c) == nullptr; }),
        qlUnderlyingBondCoupons.end());

    // get open end date replacement from vanilla builder to handle perpetuals

    QuantLib::ext::shared_ptr<EngineBuilder> vanillaBuilder = engineFactory->builder("Bond");
    QL_REQUIRE(builder, "ConvertibleBond::build(): internal error, vanilla builder is null");
    std::string openEndDateStr = vanillaBuilder->modelParameter("OpenEndDateReplacement", {}, false, "");
    Date openEndDateReplacement = getOpenEndDateReplacement(openEndDateStr, parseCalendar(data_.bondData().calendar()));

    // check whether the underlying bond is set up as perpetual (i.e. without maturity date)

    bool isPerpetual = false;
    for (auto const& d : data_.bondData().coupons()) {
        for (auto const& r : d.schedule().rules()) {
            isPerpetual = isPerpetual || r.endDate().empty();
        }
    }

    DLOG("isPerpetual=" << std::boolalpha << isPerpetual << ", openEndDateReplacement=" << openEndDateReplacement);

    // get equity index and fx index

    auto config = builder->configuration(MarketContext::pricing);

    QuantLib::ext::shared_ptr<EquityIndex2> equity;
    if (!data_.conversionData().equityUnderlying().name().empty()) {
        equity = *engineFactory->market()->equityCurve(data_.conversionData().equityUnderlying().name(), config);
    }

    QL_REQUIRE((equity == nullptr && data_.conversionData().fixedAmountConversionData().initialised()) ||
                   (equity != nullptr && !data_.conversionData().fixedAmountConversionData().initialised()),
               "ConvertibleBondEngineBuilder::build(): exactly one of equity underlying or fixed amount conversion "
               "must be specified");

    QuantLib::ext::shared_ptr<FxIndex> fx;
    if (equity != nullptr && !equity->currency().empty()) {
        if (equity->currency().code() != data_.bondData().currency()) {
            QL_REQUIRE(!data_.conversionData().fxIndex().empty(),
                       "ConvertibleBondEngineBuilder::build(): FXIndex required in conversion data, since eq ccy ("
                           << equity->currency().code() << ") not equal bond ccy (" << data_.bondData().currency()
                           << ")");
            fx = buildFxIndex(data_.conversionData().fxIndex(), data_.bondData().currency(), equity->currency().code(),
                              engineFactory->market(), config);
        }
    } else if (equity == nullptr && data_.conversionData().fixedAmountConversionData().initialised()) {
        if (data_.conversionData().fixedAmountConversionData().currency() != data_.bondData().currency()) {
            QL_REQUIRE(!data_.conversionData().fxIndex().empty(),
                       "ConvertibleBondEngineBuilder::build(): FXIndex required in conversion data, since fixed amount "
                       "conversion  ccy ("
                           << data_.conversionData().fixedAmountConversionData().currency() << ") not equal bond ccy ("
                           << data_.bondData().currency() << ")");
            fx = buildFxIndex(data_.conversionData().fxIndex(), data_.bondData().currency(),
                              data_.conversionData().fixedAmountConversionData().currency(), engineFactory->market(),
                              config);
        }
    }

    // for cross currency, add required FX fixings for conversion and dividend history

    if (fx != nullptr) {

        Date d0 = qlUnderlyingBond->startDate();
        Date d1 = qlUnderlyingBond->maturityDate();

        // FIXME, the following only works, if we have the dividends loaded at this point...
        if (equity != nullptr) {
            auto div = equity->dividendFixings();
            for (auto const& d : div) {
                if (d.exDate >= d0) {
                    requiredFixings_.addFixingDate(fx->fixingCalendar().adjust(d.exDate, Preceding),
                                                   data_.conversionData().fxIndex());
                }
            }
        }

        Date today = Settings::instance().evaluationDate();
        d0 = std::min(d0, today);

        // ...as a workaround, we add all fx fixings from min(today, bond start date) to maturity
        // -> this also covers the required fx fixings for conversion, so we don't have to add them separately
        for (Date d = d0; d <= d1; ++d) {
            requiredFixings_.addFixingDate(fx->fixingCalendar().adjust(d, Preceding), data_.conversionData().fxIndex(),
                                           Date::maxDate(), false, false);
        }
    }

    // the multiplier, basically the number of bonds and a sign for long / short positions

    Real multiplier = data_.bondData().bondNotional() * (data_.bondData().isPayer() ? -1.0 : 1.0);

    // build convertible data

    ConvertibleBond2::ExchangeableData exchangeableData = buildExchangeableData(data_.conversionData());
    std::vector<ConvertibleBond2::CallabilityData> callData =
        buildCallabilityData(data_.callData(), openEndDateReplacement);
    ConvertibleBond2::MakeWholeData makeWholeCrIncreaseData = buildMakeWholeData(data_.callData());
    std::vector<ConvertibleBond2::CallabilityData> putData =
        buildCallabilityData(data_.putData(), openEndDateReplacement);
    // for fixed amounts the model will provide an equity with constant unit spot rate, so that
    // we can treat the amount as a ratio
    std::vector<ConvertibleBond2::ConversionRatioData> conversionRatioData =
        equity != nullptr ? buildConversionRatioData(data_.conversionData())
                          : buildConversionFixedAmountData(data_.conversionData());
    std::vector<ConvertibleBond2::ConversionData> conversionData = buildConversionData(
        data_.conversionData(), requiredFixings_, equity, fx, data_.conversionData().fxIndex(), openEndDateReplacement);
    std::vector<ConvertibleBond2::MandatoryConversionData> mandatoryConversionData =
        buildMandatoryConversionData(data_.conversionData());
    std::vector<ConvertibleBond2::ConversionResetData> conversionResetData = buildConversionResetData(
        data_.conversionData(), requiredFixings_, equity, fx, data_.conversionData().fxIndex(), openEndDateReplacement);
    std::vector<ConvertibleBond2::DividendProtectionData> dividendProtectionData =
        buildDividendProtectionData(data_.dividendProtectionData(), requiredFixings_, equity, fx,
                                    data_.conversionData().fxIndex(), openEndDateReplacement);

    // build convertible bond instrument and attach pricing engine

    // get the last relevant date of the convertible bond, this is used as the last calibration date for the model
    Date lastDate = qlUnderlyingBond->maturityDate();
    if (!dividendProtectionData.empty()) {
        lastDate = std::max(lastDate, dividendProtectionData.back().protectionDate);
    }

    QL_REQUIRE(data_.conversionData().initialised(), "ConvertibleBond::build(): conversion data required");
    auto qlConvertible = QuantLib::ext::make_shared<ConvertibleBond2>(
        qlUnderlyingBond->settlementDays(), qlUnderlyingBond->calendar(), qlUnderlyingBond->issueDate(),
        qlUnderlyingBondCoupons, exchangeableData, callData, makeWholeCrIncreaseData, putData, conversionRatioData,
        conversionData, mandatoryConversionData, conversionResetData, dividendProtectionData,
        data_.detachable().empty() ? false : parseBool(data_.detachable()), isPerpetual);
    qlConvertible->setPricingEngine(builder->engine(
        id(), data_.bondData().currency(), data_.bondData().creditCurveId(), data_.bondData().hasCreditRisk(),
        data_.bondData().securityId(), data_.bondData().referenceCurveId(), exchangeableData.isExchangeable, equity, fx,
        data_.conversionData().exchangeableData().equityCreditCurve(), qlUnderlyingBond->startDate(), lastDate));
    setSensitivityTemplate(*builder);

    // set up other trade member variables

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlConvertible, multiplier);
    npvCurrency_ = notionalCurrency_ = data_.bondData().currency();
    maturity_ = qlUnderlyingBond->maturityDate();
    notional_ = qlUnderlyingBond->notional();
    legs_ = {qlUnderlyingBond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {data_.bondData().isPayer()};
}

void ConvertibleBond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    originalData_.fromXML(XMLUtils::getChildNode(node, "ConvertibleBondData"));
    data_ = originalData_;
}

XMLNode* ConvertibleBond::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, originalData_.toXML(doc));
    return node;
}

void ConvertibleBondTrsUnderlyingBuilder::build(
    const std::string& parentId, const QuantLib::ext::shared_ptr<Trade>& underlying, const std::vector<Date>& valuationDates,
    const std::vector<Date>& paymentDates, const std::string& fundingCurrency,
    const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, QuantLib::ext::shared_ptr<QuantLib::Index>& underlyingIndex,
    Real& underlyingMultiplier, std::map<std::string, double>& indexQuantities,
    std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices, Real& initialPrice,
    std::string& assetCurrency, std::string& creditRiskCurrency,
    std::map<std::string, SimmCreditQualifierMapping>& creditQualifierMapping,
    const std::function<QuantLib::ext::shared_ptr<QuantExt::FxIndex>(
        const QuantLib::ext::shared_ptr<Market> market, const std::string& configuration, const std::string& domestic,
        const std::string& foreign, std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices)>&
        getFxIndex,
    const std::string& underlyingDerivativeId, RequiredFixings& fixings, std::vector<Leg>& returnLegs) const {
    auto t = QuantLib::ext::dynamic_pointer_cast<ore::data::ConvertibleBond>(underlying);
    QL_REQUIRE(t, "could not cast to ore::data::Bond, this is unexpected");
    auto qlBond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(underlying->instrument()->qlInstrument());
    QL_REQUIRE(qlBond, "expected QuantLib::Bond, could not cast");

    BondIndexBuilder bondIndexBuilder(t->bondData(), true, false, NullCalendar(), true, engineFactory);
    underlyingIndex = bondIndexBuilder.bondIndex();

    underlyingIndex = bondIndexBuilder.bondIndex();
    underlyingMultiplier = t->data().bondData().bondNotional();

    indexQuantities[underlyingIndex->name()] = underlyingMultiplier;
    if (initialPrice != Null<Real>())
        initialPrice = qlBond->notional(valuationDates.front()) * bondIndexBuilder.priceAdjustment(initialPrice);

    assetCurrency = t->data().bondData().currency();
    auto fxIndex = getFxIndex(engineFactory->market(), engineFactory->configuration(MarketContext::pricing),
                              assetCurrency, fundingCurrency, fxIndices);
    auto returnLeg =
        makeBondTRSLeg(valuationDates, paymentDates, bondIndexBuilder, initialPrice, fxIndex);

    // add required bond and fx fixings for bondIndex
    returnLegs.push_back(returnLeg);
    bondIndexBuilder.addRequiredFixings(fixings, returnLeg);

    creditRiskCurrency = t->data().bondData().currency();
    creditQualifierMapping[securitySpecificCreditCurveName(t->bondData().securityId(), t->bondData().creditCurveId())] =
        SimmCreditQualifierMapping(t->data().bondData().securityId(), t->data().bondData().creditGroup());
    creditQualifierMapping[t->bondData().creditCurveId()] =
        SimmCreditQualifierMapping(t->data().bondData().securityId(), t->data().bondData().creditGroup());
}

void ConvertibleBondTrsUnderlyingBuilder::updateUnderlying(const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData,
                                                           QuantLib::ext::shared_ptr<Trade>& underlying,
                                                           const std::string& parentId) const {

    /* If the underlying is a bond, but the security id is actually pointing to reference data of a non-vanilla bond
       flavour like a convertible bond, callable bond, etc., we change the underlying to that non-vanilla bond flavour
       here on the fly. This way we can reference a bond from a TRS without knowing its flavour. */

    if (underlying->tradeType() == "Bond") {
        auto bond = QuantLib::ext::dynamic_pointer_cast<ore::data::Bond>(underlying);
        QL_REQUIRE(bond, "TRS::build(): internal error, could not cast underlying trade to bond");
        if (refData != nullptr &&
            refData->hasData(ConvertibleBondReferenceDatum::TYPE, bond->bondData().securityId())) {
            DLOG("Underlying trade type is bond, but security id '"
                 << bond->bondData().securityId()
                 << "' points to convertible bond in ref data, so we change the underlying trade type accordingly.");
            underlying = QuantLib::ext::make_shared<ConvertibleBond>(Envelope(), ConvertibleBondData(bond->bondData()));
            underlying->id() = parentId + "_underlying";
        }
    }
}

BondBuilder::Result ConvertibleBondBuilder::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                                  const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                                  const std::string& securityId) const {
    static long id = 0;
    ConvertibleBondData data(BondData(securityId, 1.0));
    data.populateFromBondReferenceData(referenceData);
    ConvertibleBond bond(Envelope(), data);
    bond.id() = "ConvertibleBondBuilder_" + securityId + "_" + std::to_string(id++);
    bond.build(engineFactory);

    QL_REQUIRE(bond.instrument(), "ConvertibleBondBuilder: constructed bond is null, this is unexpected");
    auto qlBond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(bond.instrument()->qlInstrument());

    QL_REQUIRE(
        bond.instrument() && bond.instrument()->qlInstrument(),
        "ConvertibleBondBuilder: constructed bond trade does not provide a valid ql instrument, this is unexpected "
        "(either the instrument wrapper or the ql instrument is null)");

    BondBuilder::Result res;
    res.bond = qlBond;
    res.hasCreditRisk = data.bondData().hasCreditRisk() && !data.bondData().creditCurveId().empty();
    res.currency = data.bondData().currency();
    res.creditCurveId = data.bondData().creditCurveId();
    res.securityId = data.bondData().securityId();
    res.creditGroup = data.bondData().creditGroup();
    res.priceQuoteMethod = data.bondData().priceQuoteMethod();
    res.priceQuoteBaseValue = data.bondData().priceQuoteBaseValue();

    auto builders = engineFactory->modelBuilders();
    auto b = std::find_if(builders.begin(), builders.end(),
                          [&bond](const std::pair<const std::string&, const QuantLib::ext::shared_ptr<ModelBuilder>>& p) {
                              return bond.id() == p.first;
                          });
    QL_REQUIRE(b != builders.end(), "ConvertibleBondBuilder: could not get model builder for bond '"
                                        << bond.id() << "' from engine factory - this is an internal error.");
    res.modelBuilder = b->second;

    return res;
}

} // namespace data
} // namespace ore
