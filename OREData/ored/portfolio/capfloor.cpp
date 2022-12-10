/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/portfolio/capfloor.hpp>
#include <ored/portfolio/durationadjustedcmslegdata.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void CapFloor::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CapFloor::build() called for trade " << id() << ", leg type is " << legData_.legType());

    QL_REQUIRE((legData_.legType() == "Floating") || (legData_.legType() == "CMS") ||
                   (legData_.legType() == "DurationAdjustedCMS") || (legData_.legType() == "CMSSpread") || (legData_.legType() == "CPI") ||
                   (legData_.legType() == "YY"),
               "CapFloor build error, LegType must be Floating, CMS, DurationAdjustedCMS, CMSSpread, CPI or YY");

    QL_REQUIRE(caps_.size() > 0 || floors_.size() > 0, "CapFloor build error, no cap rates or floor rates provided");
    QuantLib::CapFloor::Type capFloorType;
    if (floors_.size() == 0) {
        capFloorType = QuantLib::CapFloor::Cap;
    } else if (caps_.size() == 0) {
        capFloorType = QuantLib::CapFloor::Floor;
    } else {
        capFloorType = QuantLib::CapFloor::Collar;
    }

    legs_.clear();
    boost::shared_ptr<EngineBuilder> builder;
    std::string underlyingIndex, qlIndexName;
    boost::shared_ptr<QuantLib::Instrument> qlInstrument;

    // Account for long / short multiplier. In the following we expect the qlInstrument to be set up
    // as a long cap resp. a long floor resp. as a collar which by definition is a long cap + short floor
    // (this is opposite to the definition of a leg with naked option = true!)
    // The isPayer flag in the leg data is ignored.
    Real multiplier = (parsePositionType(longShort_) == Position::Long ? 1.0 : -1.0);

    if (legData_.legType() == "Floating") {

        boost::shared_ptr<FloatingLegData> floatData =
            boost::dynamic_pointer_cast<FloatingLegData>(legData_.concreteLegData());
        QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << legData_.legType());
        underlyingIndex = floatData->index();
        Handle<IborIndex> hIndex =
            engineFactory->market()->iborIndex(underlyingIndex, engineFactory->configuration(MarketContext::pricing));
        QL_REQUIRE(!hIndex.empty(), "Could not find ibor index " << underlyingIndex << " in market.");
        boost::shared_ptr<IborIndex> index = hIndex.currentLink();
        qlIndexName = index->name();

        QL_REQUIRE(floatData->caps().empty() && floatData->floors().empty(),
                   "CapFloor build error, Floating leg section must not have caps and floors");

        bool isBmaIndex = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(index) != nullptr;

        if (!isBmaIndex && !floatData->hasSubPeriods()) {
            // For the cases where we support caps and floors in the regular way, we build a floating leg with
            // the nakedOption flag set to true, this avoids maintaining all features in legs with associated
            // coupon pricers and at the same time in the QuaantLib::CapFloor instrument and pricing engine.
            // Supported cases are
            // - Ibor coupon without sub periods (hasSubPeriods = false)
            // - compounded ON coupon, averaged ON coupon
            // The other cases are handled below.
            LegData tmpLegData = legData_;
            boost::shared_ptr<FloatingLegData> tmpFloatData = boost::make_shared<FloatingLegData>(*floatData);
            tmpFloatData->floors() = floors_;
            tmpFloatData->caps() = caps_;
            tmpFloatData->nakedOption() = true;
            tmpLegData.concreteLegData() = tmpFloatData;
            legs_.push_back(engineFactory->legBuilder(tmpLegData.legType())
                                ->buildLeg(tmpLegData, engineFactory, requiredFixings_,
                                           engineFactory->configuration(MarketContext::pricing)));
            // if both caps and floors are given, we have to use a payer leg, since in this case
            // the StrippedCappedFlooredCoupon used to extract the naked options assumes a long floor
            // and a short cap while we have documented a collar to be a short floor and long cap
            qlInstrument =
                boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>{!floors_.empty() && !caps_.empty()});
            qlInstrument->setPricingEngine(
                boost::make_shared<DiscountingSwapEngine>(engineFactory->market()->discountCurve(legData_.currency())));
            maturity_ = CashFlows::maturityDate(legs_.front());
        } else {
            // For the cases where we don't have regular cap / floor support we treat the index approximately as an Ibor
            // index and build an QuantLib::CapFloor with associated pricing engine. These cases comprise:
            // - BMA coupons
            // - Ibor coupons with sub periods (hasSubPeriods = true)

            ALOG("CapFloor trade " << id() << " on a) BMA or b) sub periods Ibor (index = '" << underlyingIndex
                                   << "') built, will treat the index approximately as an ibor index");
            builder = engineFactory->builder(tradeType_);
            legs_.push_back(makeIborLeg(legData_, index, engineFactory));

            // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
            if (floors_.size() > 1) {
                QL_REQUIRE(floors_.size() == legs_[0].size(),
                           "The number of floor rates provided does not match the number of schedule periods");
            }

            if (caps_.size() > 1) {
                QL_REQUIRE(caps_.size() == legs_[0].size(),
                           "The number of cap rates provided does not match the number of schedule periods");
            }

            // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
            if (floors_.size() == 1)
                floors_.resize(legs_[0].size(), floors_[0]);

            if (caps_.size() == 1)
                caps_.resize(legs_[0].size(), caps_[0]);

            // Create QL CapFloor instrument
            qlInstrument = boost::make_shared<QuantLib::CapFloor>(capFloorType, legs_[0], caps_, floors_);

            boost::shared_ptr<CapFloorEngineBuilder> capFloorBuilder =
                boost::dynamic_pointer_cast<CapFloorEngineBuilder>(builder);
            qlInstrument->setPricingEngine(capFloorBuilder->engine(underlyingIndex));

            maturity_ = boost::dynamic_pointer_cast<QuantLib::CapFloor>(qlInstrument)->maturityDate();
        }

    } else if (legData_.legType() == "CMS") {
        builder = engineFactory->builder("Swap");

        boost::shared_ptr<CMSLegData> cmsData = boost::dynamic_pointer_cast<CMSLegData>(legData_.concreteLegData());
        QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");

        underlyingIndex = cmsData->swapIndex();
        Handle<SwapIndex> hIndex =
            engineFactory->market()->swapIndex(underlyingIndex, builder->configuration(MarketContext::pricing));
        QL_REQUIRE(!hIndex.empty(), "Could not find swap index " << underlyingIndex << " in market.");

        boost::shared_ptr<SwapIndex> index = hIndex.currentLink();
        qlIndexName = index->name();

        LegData tmpLegData = legData_;
        boost::shared_ptr<CMSLegData> tmpFloatData = boost::make_shared<CMSLegData>(*cmsData);
        tmpFloatData->floors() = floors_;
        tmpFloatData->caps() = caps_;
        tmpFloatData->nakedOption() = true;
        tmpLegData.concreteLegData() = tmpFloatData;
        legs_.push_back(engineFactory->legBuilder(tmpLegData.legType())
                            ->buildLeg(tmpLegData, engineFactory, requiredFixings_,
                                       engineFactory->configuration(MarketContext::pricing)));
        // if both caps and floors are given, we have to use a payer leg, since in this case
        // the StrippedCappedFlooredCoupon used to extract the naked options assumes a long floor
        // and a short cap while we have documented a collar to be a short floor and long cap
        qlInstrument = boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>{!floors_.empty() && !caps_.empty()});
        qlInstrument->setPricingEngine(
            boost::make_shared<DiscountingSwapEngine>(engineFactory->market()->discountCurve(legData_.currency())));
        maturity_ = CashFlows::maturityDate(legs_.front());

    } else if (legData_.legType() == "DurationAdjustedCMS") {
        auto cmsData = boost::dynamic_pointer_cast<DurationAdjustedCmsLegData>(legData_.concreteLegData());
        QL_REQUIRE(cmsData, "Wrong LegType, expected DurationAdjustedCmsLegData");
        LegData tmpLegData = legData_;
        auto tmpCmsData = boost::make_shared<DurationAdjustedCmsLegData>(*cmsData);
        tmpCmsData->floors() = floors_;
        tmpCmsData->caps() = caps_;
        tmpCmsData->nakedOption() = true;
        tmpLegData.concreteLegData() = tmpCmsData;
        legs_.push_back(engineFactory->legBuilder(tmpLegData.legType())
                            ->buildLeg(tmpLegData, engineFactory, requiredFixings_,
                                       engineFactory->configuration(MarketContext::pricing)));
        // if both caps and floors are given, we have to use a payer leg, since in this case
        // the StrippedCappedFlooredCoupon used to extract the naked options assumes a long floor
        // and a short cap while we have documented a collar to be a short floor and long cap
        qlInstrument = boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>{!floors_.empty() && !caps_.empty()});
        qlInstrument->setPricingEngine(
            boost::make_shared<DiscountingSwapEngine>(engineFactory->market()->discountCurve(legData_.currency())));
        maturity_ = CashFlows::maturityDate(legs_.front());
    }
    else if (legData_.legType() == "CMSSpread") {
        builder = engineFactory->builder("Swap");
        boost::shared_ptr<CMSSpreadLegData> cmsSpreadData = boost::dynamic_pointer_cast<CMSSpreadLegData>(legData_.concreteLegData());
        QL_REQUIRE(cmsSpreadData, "Wrong LegType, expected CMSSpread");	
        LegData tmpLegData = legData_;
        boost::shared_ptr<CMSSpreadLegData> tmpFloatData = boost::make_shared<CMSSpreadLegData>(*cmsSpreadData);
        tmpFloatData->floors() = floors_;
        tmpFloatData->caps() = caps_;
        tmpFloatData->nakedOption() = true;
        tmpLegData.concreteLegData() = tmpFloatData;
        legs_.push_back(engineFactory->legBuilder(tmpLegData.legType())
                            ->buildLeg(tmpLegData, engineFactory, requiredFixings_,
                                       engineFactory->configuration(MarketContext::pricing)));
        // if both caps and floors are given, we have to use a payer leg, since in this case
        // the StrippedCappedFlooredCoupon used to extract the naked options assumes a long floor
        // and a short cap while we have documented a collar to be a short floor and long cap
        qlInstrument = boost::make_shared<QuantLib::Swap>(legs_, std::vector<bool>{!floors_.empty() && !caps_.empty()});
        qlInstrument->setPricingEngine(
            boost::make_shared<DiscountingSwapEngine>(engineFactory->market()->discountCurve(legData_.currency())));
        maturity_ = CashFlows::maturityDate(legs_.front());
    }
    else if (legData_.legType() == "CPI") {
        DLOG("CPI CapFloor Type " << capFloorType << " ID " << id());

        builder = engineFactory->builder("CpiCapFloor");

        boost::shared_ptr<CPILegData> cpiData = boost::dynamic_pointer_cast<CPILegData>(legData_.concreteLegData());
        QL_REQUIRE(cpiData, "Wrong LegType, expected CPI");

        underlyingIndex = cpiData->index();
        Handle<ZeroInflationIndex> zeroIndex = engineFactory->market()->zeroInflationIndex(
            underlyingIndex, builder->configuration(MarketContext::pricing));
        qlIndexName = zeroIndex->name();

        // the cpi leg uses the first schedule date as the start date, which only makes sense if there are at least
        // two dates in the schedule, otherwise the only date in the schedule is the pay date of the cf and a a separate
        // start date is expected; if both the separate start date and a schedule with more than one date is given
        Date startDate;
        Schedule schedule = makeSchedule(legData_.schedule());

        const string& start = cpiData->startDate();
        if (schedule.size() < 2) {
            QL_REQUIRE(!start.empty(), "Only one schedule date, a 'StartDate' must be given.");
            startDate = parseDate(start);
        } else if (!start.empty()) {
            DLOG("Schedule with more than 2 dates was provided. The first schedule date "
                 << io::iso_date(schedule.dates().front()) << " is used as the start date. The 'StartDate' of " << start
                 << " is not used.");
            startDate = schedule.dates().front();
        }

        Real baseCPI = cpiData->baseCPI();

        boost::shared_ptr<InflationSwapConvention> cpiSwapConvention = nullptr;

        auto inflationConventions =
            InstrumentConventions::instance().conventions()->get(underlyingIndex + "_INFLATIONSWAP", Convention::Type::InflationSwap);

        if (inflationConventions.first)
            cpiSwapConvention = boost::dynamic_pointer_cast<InflationSwapConvention>(inflationConventions.second);

        Period observationLag;
        if (cpiData->observationLag().empty()) {
            QL_REQUIRE(cpiSwapConvention, "observationLag is not specified in legData and couldn't find convention for "
                                              << underlyingIndex
                                              << ". Please add field to trade xml or add convention");
            DLOG("Build CPI Leg and use observation lag from standard inflationswap convention");
            observationLag = cpiSwapConvention->observationLag();
        } else {
            observationLag = parsePeriod(cpiData->observationLag());
        }

        CPI::InterpolationType interpolationMethod;
        if (cpiData->interpolation().empty()) {
            QL_REQUIRE(cpiSwapConvention, "observationLag is not specified in legData and couldn't find convention for "
                                              << underlyingIndex
                                              << ". Please add field to trade xml or add convention");
            DLOG("Build CPI Leg and use observation lag from standard inflationswap convention");
            interpolationMethod = cpiSwapConvention->interpolated() ? CPI::Linear : CPI::Flat;
        } else {
            interpolationMethod = parseObservationInterpolation(cpiData->interpolation());
        }

        Calendar cal = zeroIndex->fixingCalendar();
        BusinessDayConvention conv = Unadjusted; // not used in the CPI CapFloor engine

        QL_REQUIRE(!zeroIndex.empty(), "Zero Inflation Index is empty");

        legs_.push_back(makeCPILeg(legData_, zeroIndex.currentLink(), engineFactory));

        // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
        if (floors_.size() > 1) {
            QL_REQUIRE(floors_.size() == legs_[0].size(),
                       "The number of floor rates provided does not match the number of schedule periods");
        }

        if (caps_.size() > 1) {
            QL_REQUIRE(caps_.size() == legs_[0].size(),
                       "The number of cap rates provided does not match the number of schedule periods");
        }

        // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
        if (floors_.size() == 1)
            floors_.resize(legs_[0].size(), floors_[0]);

        if (caps_.size() == 1)
            caps_.resize(legs_[0].size(), caps_[0]);

        boost::shared_ptr<CpiCapFloorEngineBuilder> capFloorBuilder =
            boost::dynamic_pointer_cast<CpiCapFloorEngineBuilder>(builder);

        // Create QL CPI CapFloor instruments and add to a composite
        qlInstrument = boost::make_shared<CompositeInstrument>();
        maturity_ = Date::minDate();
        for (Size i = 0; i < legs_[0].size(); ++i) {
            DLOG("Create composite " << i);
            Real nominal, gearing;
            Date paymentDate;
            boost::shared_ptr<CPICoupon> coupon = boost::dynamic_pointer_cast<CPICoupon>(legs_[0][i]);
            boost::shared_ptr<CPICashFlow> cashflow = boost::dynamic_pointer_cast<CPICashFlow>(legs_[0][i]);
            if (coupon) {
                nominal = coupon->nominal();
                gearing = coupon->fixedRate() * coupon->accrualPeriod();
                paymentDate = coupon->date();
            } else if (cashflow) {
                nominal = cashflow->notional();
                gearing = 1.0; // no gearing here
                paymentDate = cashflow->date();
            } else {
                QL_FAIL("Failed to interpret CPI flow");
            }

            if (capFloorType == QuantLib::CapFloor::Cap || capFloorType == QuantLib::CapFloor::Collar) {
                boost::shared_ptr<CPICapFloor> capfloor =
                    boost::make_shared<CPICapFloor>(Option::Call, nominal, startDate, baseCPI, paymentDate, cal, conv, cal, conv, caps_[i], zeroIndex,
                    observationLag, interpolationMethod);
                capfloor->setPricingEngine(capFloorBuilder->engine(underlyingIndex));
                boost::dynamic_pointer_cast<QuantLib::CompositeInstrument>(qlInstrument)->add(capfloor, gearing);
                maturity_ = std::max(maturity_, capfloor->payDate());
            }

            if (capFloorType == QuantLib::CapFloor::Floor || capFloorType == QuantLib::CapFloor::Collar) {
                // for collars we want a long cap, short floor
                Real sign = capFloorType == QuantLib::CapFloor::Floor ? 1.0 : -1.0;
                boost::shared_ptr<CPICapFloor> capfloor =
                    boost::make_shared<CPICapFloor>(Option::Put, nominal, startDate, baseCPI, paymentDate, cal, conv, cal, conv, floors_[i], zeroIndex,
                    observationLag, interpolationMethod);
                capfloor->setPricingEngine(capFloorBuilder->engine(underlyingIndex));
                boost::dynamic_pointer_cast<QuantLib::CompositeInstrument>(qlInstrument)->add(capfloor, sign * gearing);
                maturity_ = std::max(maturity_, capfloor->payDate());
            }
        }

    } else if (legData_.legType() == "YY") {
        builder = engineFactory->builder("YYCapFloor");

        boost::shared_ptr<YoYLegData> yyData = boost::dynamic_pointer_cast<YoYLegData>(legData_.concreteLegData());
        QL_REQUIRE(yyData, "Wrong LegType, expected YY");

        underlyingIndex = yyData->index();
        Handle<YoYInflationIndex> yoyIndex;
        // look for yoy inflation index
        yoyIndex =
            engineFactory->market()->yoyInflationIndex(underlyingIndex, builder->configuration(MarketContext::pricing));
        qlIndexName = yoyIndex->name();

        // we must have either an yoy or a zero inflation index in the market, if no yoy curve, get the zero
        // and create a yoy index from it
        if (yoyIndex.empty()) {
            Handle<ZeroInflationIndex> zeroIndex = engineFactory->market()->zeroInflationIndex(
                underlyingIndex, builder->configuration(MarketContext::pricing));
            QL_REQUIRE(!zeroIndex.empty(), "Could not find inflation index (of type either zero or yoy) "
                                               << underlyingIndex << " in market.");
            yoyIndex = Handle<YoYInflationIndex>(boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                zeroIndex.currentLink(), zeroIndex->interpolated()));
        }

        legs_.push_back(makeYoYLeg(legData_, yoyIndex.currentLink(), engineFactory));

        // If a vector of cap/floor rates are provided, ensure they align with the number of schedule periods
        if (floors_.size() > 1) {
            QL_REQUIRE(floors_.size() == legs_[0].size(),
                       "The number of floor rates provided does not match the number of schedule periods");
        }

        if (caps_.size() > 1) {
            QL_REQUIRE(caps_.size() == legs_[0].size(),
                       "The number of cap rates provided does not match the number of schedule periods");
        }

        // If one cap/floor rate is given, extend the vector to align with the number of schedule periods
        if (floors_.size() == 1)
            floors_.resize(legs_[0].size(), floors_[0]);

        if (caps_.size() == 1)
            caps_.resize(legs_[0].size(), caps_[0]);

        // Create QL YoY Inflation CapFloor instrument
        if (capFloorType == QuantLib::CapFloor::Cap) {
            qlInstrument = boost::shared_ptr<YoYInflationCapFloor>(new YoYInflationCap(legs_[0], caps_));
        } else if (capFloorType == QuantLib::CapFloor::Floor) {
            qlInstrument = boost::shared_ptr<YoYInflationCapFloor>(new YoYInflationFloor(legs_[0], floors_));
        } else if (capFloorType == QuantLib::CapFloor::Collar) {
            qlInstrument = boost::shared_ptr<YoYInflationCapFloor>(
                new YoYInflationCapFloor(QuantLib::YoYInflationCapFloor::Collar, legs_[0], caps_, floors_));
        } else {
            QL_FAIL("unknown YoYInflation cap/floor type");
        }

        boost::shared_ptr<YoYCapFloorEngineBuilder> capFloorBuilder =
            boost::dynamic_pointer_cast<YoYCapFloorEngineBuilder>(builder);
        qlInstrument->setPricingEngine(capFloorBuilder->engine(underlyingIndex));

        // Wrap the QL instrument in a vanilla instrument

        maturity_ = boost::dynamic_pointer_cast<QuantLib::YoYInflationCapFloor>(qlInstrument)->maturityDate();
    } else {
        QL_FAIL("Invalid legType " << legData_.legType() << " for CapFloor");
    }

    // Fill in remaining Trade member data

    QL_REQUIRE(legs_.size() == 1, "internal error, expected one leg in cap floor builder, got " << legs_.size());

    legCurrencies_.push_back(legData_.currency());
    legPayers_.push_back(false); // already accounted for via the instrument multiplier
    npvCurrency_ = legData_.currency();
    notionalCurrency_ = legData_.currency();
    notional_ = currentNotional(legs_[0]);

    // add premiums

    std::vector<boost::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, 1.0, premiumData_,
                                                -multiplier, parseCurrency(legData_.currency()), engineFactory,
                                                engineFactory->configuration(MarketContext::pricing)));

    // set instrument
    instrument_ =
        boost::make_shared<VanillaInstrument>(qlInstrument, multiplier, additionalInstruments, additionalMultipliers);

    // add required fixings
    if (!qlIndexName.empty() && !underlyingIndex.empty()) {
        auto fdg = boost::make_shared<FixingDateGetter>(requiredFixings_);
        for (auto const& l : legs_)
            addToRequiredFixings(l, fdg);
    }

    Date startDate = Date::maxDate();
    for (auto const& l : legs_) {
        if (!l.empty()) {
            startDate = std::min(startDate, l.front()->date());
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(l.front());
            if (coupon)
                startDate = std::min(startDate, coupon->accrualStartDate());                
        }
    }

    additionalData_["startDate"] = to_string(startDate);
}

const std::map<std::string, boost::any>& CapFloor::additionalData() const {
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();

    additionalData_["legType"] = legData_.legType();
    additionalData_["isPayer"] = legData_.isPayer();
    additionalData_["notionalCurrency"] = legData_.currency();
    for (Size j = 0; j < legs_[0].size(); ++j) {
        boost::shared_ptr<CashFlow> flow = legs_[0][j];
        // pick flow with earliest future payment date on this leg
        if (flow->date() > asof) {
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(flow);
            if (coupon) {
                Real currentNotional = 0;
                try {
                    currentNotional = coupon->nominal();
                } catch (std::exception& e) {
                    ALOG("current notional could not be determined for trade " << id()
                                                                               << ", set to zero: " << e.what());
                }
                additionalData_["currentNotional"] = currentNotional;

                boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(flow);
                if (frc) {
                    additionalData_["index"] = frc->index()->name();
                }
            }
            break;
        }
    }
    if (legs_[0].size() > 0) {
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[0][0]);
        if (coupon) {
            Real originalNotional = 0.0;
            try {
                originalNotional = coupon->nominal();
            } catch (std::exception& e) {
                ALOG("original nominal could not be determined for trade " << id() << ", set to zero: " << e.what());
            }
            additionalData_["originalNotional"] = originalNotional;
        }
    }

    vector<Real> amounts;
    vector<Date> paymentDates;
    vector<Real> currentNotionals;
    vector<Rate> rates;
    vector<Date> fixingDates;
    vector<Rate> indexFixings;
    vector<Spread> spreads;
    vector<Rate> caps;
    vector<Rate> effectiveCaps;
    vector<Volatility> capletVols;
    vector<Real> capletPrices;
    vector<Rate> floors;
    vector<Rate> effectiveFloors;
    vector<Volatility> floorletVols;
    vector<Real> floorletPrices;

    try {
        for (const auto& flow : legs_[0]) {
	    // pick flow with earliest future payment date on this leg
	    if (flow->date() > asof) {
	        amounts.push_back(flow->amount());
		paymentDates.push_back(flow->date());
		boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(flow);
		if (coupon) {
		    currentNotionals.push_back(coupon->nominal());
		    rates.push_back(coupon->rate());
		    boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(flow);
		    if (frc) {
		        fixingDates.push_back(frc->fixingDate());
			indexFixings.push_back(frc->indexFixing());
			spreads.push_back(frc->spread());

			// The below code adds cap/floor levels, vols, and prices
			// for capped/floored Ibor coupons

			boost::shared_ptr<StrippedCappedFlooredCoupon> strippedCfc =
			    boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(flow);
			if (!strippedCfc)
			    continue;

			boost::shared_ptr<CappedFlooredCoupon> cfc = strippedCfc->underlying();
			boost::shared_ptr<IborCouponPricer> pricer =
			    boost::dynamic_pointer_cast<IborCouponPricer>(cfc->pricer());
			if (pricer && (cfc->fixingDate() > asof)) {
			    // We write the vols if an Ibor coupon pricer is found and the fixing date is in the future
			    if (cfc->isCapped()) {
			        caps.push_back(cfc->cap());
				const Rate effectiveCap = cfc->effectiveCap();
				effectiveCaps.push_back(effectiveCap);
				capletVols.push_back(
                                    pricer->capletVolatility()->volatility(cfc->fixingDate(), effectiveCap));
				capletPrices.push_back(pricer->capletPrice(effectiveCap) * coupon->nominal());
			    }
			    if (cfc->isFloored()) {
			        floors.push_back(cfc->floor());
				const Rate effectiveFloor = cfc->effectiveFloor();
				effectiveFloors.push_back(effectiveFloor);
				floorletVols.push_back(
                                    pricer->capletVolatility()->volatility(cfc->fixingDate(), effectiveFloor));
				floorletPrices.push_back(pricer->floorletPrice(effectiveFloor) * coupon->nominal());
			    }
			}
		    }
		}
	    }
	}

	additionalData_["amounts"] = amounts;
	additionalData_["paymentDates"] = paymentDates;
	additionalData_["currentNotionals"] = currentNotionals;
	additionalData_["rates"] = rates;
	additionalData_["fixingDates"] = fixingDates;
	additionalData_["indexFixings"] = indexFixings;
	additionalData_["spreads"] = spreads;
	if (caps.size() > 0) {
	    additionalData_["caps"] = caps;
	    additionalData_["effectiveCaps"] = effectiveCaps;
	    additionalData_["capletVols"] = capletVols;
	    additionalData_["capletPrices"] = capletPrices;
	}
	if (floors.size() > 0) {
	    additionalData_["floors"] = floors;
	    additionalData_["effectiveFloors"] = effectiveFloors;
	    additionalData_["floorletVols"] = floorletVols;
	    additionalData_["floorletPrices"] = floorletPrices;
	}

    } catch (std::exception& e) {
        ALOG("error getting additional data for capfloor trade " << id() << ". " << e.what());
    }

    return additionalData_;
}

void CapFloor::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* capFloorNode = XMLUtils::getChildNode(node, "CapFloorData");
    longShort_ = XMLUtils::getChildValue(capFloorNode, "LongShort", true);
    legData_.fromXML(XMLUtils::getChildNode(capFloorNode, "LegData"));
    caps_ = XMLUtils::getChildrenValuesAsDoubles(capFloorNode, "Caps", "Cap");
    floors_ = XMLUtils::getChildrenValuesAsDoubles(capFloorNode, "Floors", "Floor");
    premiumData_.fromXML(capFloorNode);
}

XMLNode* CapFloor::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* capFloorNode = doc.allocNode("CapFloorData");
    XMLUtils::appendNode(node, capFloorNode);
    XMLUtils::addChild(doc, capFloorNode, "LongShort", longShort_);
    XMLUtils::appendNode(capFloorNode, legData_.toXML(doc));
    XMLUtils::addChildren(doc, capFloorNode, "Caps", "Cap", caps_);
    XMLUtils::addChildren(doc, capFloorNode, "Floors", "Floor", floors_);
    XMLUtils::appendNode(capFloorNode, premiumData_.toXML(doc));
    return node;
}

} // namespace data
} // namespace ore
