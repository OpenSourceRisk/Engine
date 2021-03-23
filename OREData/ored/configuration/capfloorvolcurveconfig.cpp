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

#include <ored/configuration/capfloorvolcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>

using boost::assign::list_of;
using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::DayCounter;
using QuantLib::Natural;
using std::ostream;
using std::set;
using std::string;
using std::vector;

namespace ore {
namespace data {

typedef boost::bimap<string, CapFloorVolatilityCurveConfig::VolatilityType> BmType;
const BmType volatilityTypeMap =
    list_of<BmType::value_type>("Normal", CapFloorVolatilityCurveConfig::VolatilityType::Normal)(
        "Lognormal", CapFloorVolatilityCurveConfig::VolatilityType::Lognormal)(
        "ShiftedLognormal", CapFloorVolatilityCurveConfig::VolatilityType::ShiftedLognormal);

// Allowable interpolation strings for time and strike interpolation
// BackwardFlat is not allowed for strike interpolation but that is handled elsewhere.
const set<string> validInterps = {"Linear", "LinearFlat", "BackwardFlat", "Cubic", "CubicFlat"};

CapFloorVolatilityCurveConfig::CapFloorVolatilityCurveConfig(
    const string& curveID, const string& curveDescription, const VolatilityType& volatilityType, bool extrapolate,
    bool flatExtrapolation, bool inlcudeAtm, const vector<string>& tenors, const vector<string>& strikes,
    const DayCounter& dayCounter, Natural settleDays, const Calendar& calendar,
    const BusinessDayConvention& businessDayConvention, const string& iborIndex, const string& discountCurve,
    const string& interpolationMethod, const string& interpolateOn, const string& timeInterpolation,
    const string& strikeInterpolation, const vector<string>& atmTenors, const BootstrapConfig& bootstrapConfig)
    : CurveConfig(curveID, curveDescription), volatilityType_(volatilityType), extrapolate_(extrapolate),
      flatExtrapolation_(flatExtrapolation), includeAtm_(inlcudeAtm), tenors_(tenors), strikes_(strikes),
      dayCounter_(dayCounter), settleDays_(settleDays), calendar_(calendar),
      businessDayConvention_(businessDayConvention), iborIndex_(iborIndex), discountCurve_(discountCurve),
      interpolationMethod_(interpolationMethod), interpolateOn_(interpolateOn), timeInterpolation_(timeInterpolation),
      strikeInterpolation_(strikeInterpolation), atmTenors_(atmTenors), bootstrapConfig_(bootstrapConfig) {

    // Set extrapolation string. "Linear" just means extrapolation allowed and non-flat.
    extrapolation_ = !extrapolate_ ? "None" : (flatExtrapolation_ ? "Flat" : "Linear");

    // Set type_
    configureType();

    // Check that we have a valid configuration
    validate();

    // Populate required curve ids
    populateRequiredCurveIds();

    // Populate quotes
    populateQuotes();
}

void CapFloorVolatilityCurveConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CapFloorVolatility");
    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    // Set the volatility type
    configureVolatilityType(XMLUtils::getChildValue(node, "VolatilityType", true));

    // Set the extrapolation variables
    extrapolation_ = XMLUtils::getChildValue(node, "Extrapolation", true);
    configureExtrapolation(extrapolation_);

    // The mandatory variables
    includeAtm_ = XMLUtils::getChildValueAsBool(node, "IncludeAtm", true);
    calendar_ = parseCalendar(XMLUtils::getChildValue(node, "Calendar", true));
    dayCounter_ = parseDayCounter(XMLUtils::getChildValue(node, "DayCounter", true));
    businessDayConvention_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "BusinessDayConvention", true));
    iborIndex_ = XMLUtils::getChildValue(node, "IborIndex", true);
    discountCurve_ = XMLUtils::getChildValue(node, "DiscountCurve", true);

    // Settlement days (optional)
    settleDays_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "SettlementDays")) {
        Integer d = parseInteger(XMLUtils::getNodeValue(n));
        QL_REQUIRE(d >= 0, "SettlementDays (" << d << ") must be non-negative");
        settleDays_ = static_cast<Natural>(d);
    }

    // Variable on which to interpolate (optional)
    interpolateOn_ = "TermVolatilities";
    if (XMLNode* n = XMLUtils::getChildNode(node, "InterpolateOn")) {
        interpolateOn_ = XMLUtils::getNodeValue(n);
    }

    // Interpolation in time direction (optional)
    timeInterpolation_ = "LinearFlat";
    if (XMLNode* n = XMLUtils::getChildNode(node, "TimeInterpolation")) {
        timeInterpolation_ = XMLUtils::getNodeValue(n);
    }

    // Interpolation in strike direction (optional)
    strikeInterpolation_ = "LinearFlat";
    if (XMLNode* n = XMLUtils::getChildNode(node, "StrikeInterpolation")) {
        strikeInterpolation_ = XMLUtils::getNodeValue(n);
    }

    // Tenors and strikes. Optional as we may have an ATM curve and hence only AtmTenors.
    tenors_ = XMLUtils::getChildrenValuesAsStrings(node, "Tenors", false);
    strikes_ = XMLUtils::getChildrenValuesAsStrings(node, "Strikes", false);

    // Optional flag, if set to true some tenor/strike quotes can be omitted
    optionalQuotes_ = XMLUtils::getChildValueAsBool(node, "OptionalQuotes", false, false);

    // Interpolation for cap floor term volatilities (optional)
    interpolationMethod_ = "BicubicSpline";
    if (XMLNode* n = XMLUtils::getChildNode(node, "InterpolationMethod")) {
        interpolationMethod_ = XMLUtils::getNodeValue(n);
    }

    // Tenors for ATM volatilities.
    atmTenors_ = XMLUtils::getChildrenValuesAsStrings(node, "AtmTenors", false);
    QL_REQUIRE(!tenors_.empty() || !atmTenors_.empty(), "Tenors and AtmTenors cannot both be empty");
    if (includeAtm_ && atmTenors_.empty()) {
        // swap so tenors_ is empty
        atmTenors_.swap(tenors_);
    }

    // Optional bootstrap configuration
    if (XMLNode* n = XMLUtils::getChildNode(node, "BootstrapConfig")) {
        bootstrapConfig_.fromXML(n);
    }

    // Set type_
    configureType();

    // Check that we have a valid configuration
    validate();

    // Populate required curve ids
    populateRequiredCurveIds();

    // Populate quotes
    populateQuotes();
}

XMLNode* CapFloorVolatilityCurveConfig::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CapFloorVolatility");
    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    XMLUtils::addChild(doc, node, "VolatilityType", toString(volatilityType_));
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);
    XMLUtils::addChild(doc, node, "InterpolationMethod", interpolationMethod_);
    XMLUtils::addChild(doc, node, "IncludeAtm", includeAtm_);
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addGenericChildAsList(doc, node, "Tenors", tenors_);
    XMLUtils::addGenericChildAsList(doc, node, "Strikes", strikes_);
    XMLUtils::addChild(doc, node, "OptionalQuotes", optionalQuotes_);
    XMLUtils::addChild(doc, node, "IborIndex", iborIndex_);
    XMLUtils::addChild(doc, node, "DiscountCurve", discountCurve_);
    XMLUtils::addGenericChildAsList(doc, node, "AtmTenors", atmTenors_);
    XMLUtils::addChild(doc, node, "SettlementDays", static_cast<int>(settleDays_));
    XMLUtils::addChild(doc, node, "InterpolateOn", interpolateOn_);
    XMLUtils::addChild(doc, node, "TimeInterpolation", timeInterpolation_);
    XMLUtils::addChild(doc, node, "StrikeInterpolation", strikeInterpolation_);
    XMLUtils::appendNode(node, bootstrapConfig_.toXML(doc));

    return node;
}

typedef QuantExt::CapFloorTermVolSurfaceExact::InterpolationMethod CftvsInterp;
CftvsInterp CapFloorVolatilityCurveConfig::interpolationMethod() const {
    if (interpolationMethod_ == "BicubicSpline") {
        return CftvsInterp::BicubicSpline;
    } else if (interpolationMethod_ == "Bilinear") {
        return CftvsInterp::Bilinear;
    } else {
        QL_FAIL("Invalid InterpolationMethod " << interpolationMethod_);
    }
}

string CapFloorVolatilityCurveConfig::toString(VolatilityType type) const {
    QL_REQUIRE(volatilityTypeMap.right.count(type) > 0,
               "Volatility type (" << static_cast<int>(type) << ") is not valid");
    return volatilityTypeMap.right.at(type);
}

void CapFloorVolatilityCurveConfig::populateRequiredCurveIds() {
    if (!discountCurve().empty())
        requiredCurveIds_[CurveSpec::CurveType::Yield].insert(parseCurveSpec(discountCurve())->curveConfigID());
}

string CapFloorVolatilityCurveConfig::iborTenor() const {
    string tenor;
    // Ibor index term and currency (do not allow for convention based ibor indices here)
    boost::shared_ptr<IborIndex> index = parseIborIndex(iborIndex_, tenor);
    return tenor;
}

const string& CapFloorVolatilityCurveConfig::currency() const {
    string tenor;
    // Ibor index term and currency (do not allow for convention based ibor indices here)
    boost::shared_ptr<IborIndex> index = parseIborIndex(iborIndex_, tenor);
    return index->currency().code();
}

void CapFloorVolatilityCurveConfig::populateQuotes() {

    // Cap floor quotes are for the form:
    // CAPFLOOR/(RATE_LNVOL|RATE_NVOL|RATE_SLNVOL)/<CCY>/<TENOR>/<IBOR_TENOR>/<ATM>/<RELATIVE>/<STRIKE>
    string ccy = currency();
    string tenor = iborTenor();

    // Volatility quote stem
    MarketDatum::QuoteType qType = quoteType();
    string stem = "CAPFLOOR/" + to_string(qType) + "/" + ccy + "/";

    // Cap floor matrix quotes. So, ATM flag is false i.e. 0 and RELATIVE flag is false also as strikes are absolute.
    for (const string& t : tenors_) {
        for (const string& s : strikes_) {
            quotes_.push_back(stem + t + "/" + tenor + "/0/0/" + s);
        }
    }

    // ATM quotes. So, ATM flag is true i.e. 1 and RELATIVE flag is true with strike set to 0.
    if (type_ == Type::Atm || type_ == Type::SurfaceWithAtm) {
        for (const string& t : atmTenors_) {
            quotes_.push_back(stem + t + "/" + tenor + "/1/1/0");
        }
    }

    // Cap floor shift quote depends only on the currency and ibor tenor and is of the form:
    // CAPFLOOR/SHIFT/<CCY>/<IBOR_TERM>
    if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        quotes_.push_back("CAPFLOOR/SHIFT/" + ccy + "/" + tenor);
    }
}

void CapFloorVolatilityCurveConfig::configureExtrapolation(const string& extrapolation) {
    QL_REQUIRE(extrapolation == "Linear" || extrapolation == "Flat" || extrapolation == "None",
               "Extrapolation must be one of Linear, Flat or None");
    extrapolate_ = extrapolation == "None" ? false : true;
    flatExtrapolation_ = extrapolation == "Linear" ? false : true;
}

void CapFloorVolatilityCurveConfig::configureVolatilityType(const std::string& type) {
    QL_REQUIRE(volatilityTypeMap.left.count(type) > 0, "Volatility type value '" << type << "' is not valid");
    volatilityType_ = volatilityTypeMap.left.at(type);
}

void CapFloorVolatilityCurveConfig::configureType() {
    type_ = tenors_.empty() ? Type::Atm : (includeAtm_ ? Type::SurfaceWithAtm : Type::Surface);
    // type_ = strikes_.empty() ? Type::Atm : (includeAtm_ ? Type::SurfaceWithAtm : Type::Surface);
}

void CapFloorVolatilityCurveConfig::validate() const {
    QL_REQUIRE(interpolateOn_ == "TermVolatilities" || interpolateOn_ == "OptionletVolatilities",
               "InterpolateOn (" << interpolateOn_ << ") must be TermVolatilities or OptionletVolatilities");
    QL_REQUIRE(validInterps.count(timeInterpolation_) == 1,
               "TimeInterpolation, " << timeInterpolation_ << ", not recognised");
    QL_REQUIRE(validInterps.count(strikeInterpolation_) == 1,
               "StrikeInterpolation, " << strikeInterpolation_ << ", not recognised");
    QL_REQUIRE(strikeInterpolation_ != "BackwardFlat", "BackwardFlat StrikeInterpolation is not allowed");
    if (!tenors_.empty()) {
        QL_REQUIRE(!strikes_.empty(), "Strikes cannot be empty when configuring a surface");
    }
}

MarketDatum::QuoteType CapFloorVolatilityCurveConfig::quoteType() const {
    switch (volatilityType_) {
    case CapFloorVolatilityCurveConfig::VolatilityType::Lognormal:
        return MarketDatum::QuoteType::RATE_LNVOL;
    case CapFloorVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
        return MarketDatum::QuoteType::RATE_SLNVOL;
    case CapFloorVolatilityCurveConfig::VolatilityType::Normal:
        return MarketDatum::QuoteType::RATE_NVOL;
    default:
        QL_FAIL("Unknown VolatilityType (" << static_cast<int>(volatilityType_) << ")");
    }
}

VolatilityType volatilityType(CapFloorVolatilityCurveConfig::VolatilityType type) {
    switch (type) {
    case CapFloorVolatilityCurveConfig::VolatilityType::Lognormal:
    case CapFloorVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
        return ShiftedLognormal;
    case CapFloorVolatilityCurveConfig::VolatilityType::Normal:
        return Normal;
    default:
        QL_FAIL("Unknown VolatilityType (" << static_cast<int>(type) << ")");
    }
}

} // namespace data
} // namespace ore
