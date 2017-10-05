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

#include <boost/algorithm/string.hpp>
#include <ored/configuration/swaptionvolcurveconfig.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, SwaptionVolatilityCurveConfig::VolatilityType t) {
    switch (t) {
        case SwaptionVolatilityCurveConfig::VolatilityType::Lognormal:
            return out << "RATE_LNVOL";
        case SwaptionVolatilityCurveConfig::VolatilityType::Normal:
            return out << "RATE_NVOL";
        case SwaptionVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
            return out << "RATE_SLNVOL";
        default:
            QL_FAIL("unknown VolatilityType(" << Integer(t) << ")");
    }
}

SwaptionVolatilityCurveConfig::SwaptionVolatilityCurveConfig(
    const string& curveID, const string& curveDescription, const Dimension& dimension,
    const VolatilityType& volatilityType, const bool extrapolate, const bool flatExtrapolation,
    const vector<Period>& optionTenors, const vector<Period>& swapTenors, const DayCounter& dayCounter,
    const Calendar& calendar, const BusinessDayConvention& businessDayConvention, const string& shortSwapIndexBase,
    const string& swapIndexBase, const vector<Period>& smileOptionTenors, const vector<Period>& smileSwapTenors,
    const vector<Real>& smileSpreads)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), volatilityType_(volatilityType),
      extrapolate_(extrapolate), flatExtrapolation_(flatExtrapolation), optionTenors_(optionTenors),
      swapTenors_(swapTenors), dayCounter_(dayCounter), calendar_(calendar),
      businessDayConvention_(businessDayConvention), shortSwapIndexBase_(shortSwapIndexBase),
      swapIndexBase_(swapIndexBase), smileOptionTenors_(smileOptionTenors), smileSwapTenors_(smileSwapTenors),
      smileSpreads_(smileSpreads) {

    QL_REQUIRE(dimension == Dimension::ATM || dimension == Dimension::Smile, "Invalid dimension");

    if (dimension != Dimension::Smile) {
        QL_REQUIRE(smileOptionTenors.size() == 0 && smileSwapTenors.size() == 0 && smileSpreads.size() == 0,
                   "Smile tenors/strikes/spreads should only be set when dim=Smile");
    }
}

const vector<string>& SwaptionVolatilityCurveConfig::quotes() {

    if (quotes_.size() == 0) {
        boost::shared_ptr<SwapIndex> index = parseSwapIndex(swapIndexBase_);
        Currency ccy = index->currency();
       
        std::stringstream ssBase;
        ssBase << "SWAPTION/" << volatilityType_ << "/" << ccy.code() << "/";
        string base = ssBase.str();

        if (dimension_ == Dimension::ATM) {
            for (auto o : optionTenors_) {
                for (auto s : swapTenors_) {
                    std::stringstream ss;
                    ss << base << to_string(o) << "/" << to_string(s) << "/ATM";
                    quotes_.push_back(ss.str());
                }
            }

        } else {
            for (auto o : smileOptionTenors_) {
                for (auto s : smileSwapTenors_) {
                    for (auto sp : smileSpreads_) {
                        std::stringstream ss;
                        ss << base << to_string(o) << "/" << to_string(s) << "/Smile/" << to_string(sp);
                        quotes_.push_back(ss.str());
                    }
                }
            }
        }
        for (auto q : quotes_)
            std::cout<<q<<std::endl;
    }
    return quotes_;
}

void SwaptionVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "SwaptionVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    if (dim == "ATM") {
        dimension_ = Dimension::ATM;
    } else if (dim == "Smile") {
        dimension_ = Dimension::Smile;
    } else {
        QL_FAIL("Dimension " << dim << " not recognized");
    }

    string volType = XMLUtils::getChildValue(node, "VolatilityType", true);
    if (volType == "Normal") {
        volatilityType_ = VolatilityType::Normal;
    } else if (volType == "Lognormal") {
        volatilityType_ = VolatilityType::Lognormal;
    } else if (volType == "ShiftedLognormal") {
        volatilityType_ = VolatilityType::ShiftedLognormal;
    } else {
        QL_FAIL("Volatility type " << volType << " not recognized");
    }

    string extr = XMLUtils::getChildValue(node, "Extrapolation", true);
    extrapolate_ = true;
    flatExtrapolation_ = true;
    if (extr == "Linear") {
        flatExtrapolation_ = false;
    } else if (extr == "Flat") {
        flatExtrapolation_ = true;
    } else if (extr == "None") {
        extrapolate_ = false;
    } else {
        QL_FAIL("Extrapolation " << extr << " not recognized");
    }

    optionTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "OptionTenors", true);
    swapTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "SwapTenors", true);

    string cal = XMLUtils::getChildValue(node, "Calendar", true);
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter", true);
    dayCounter_ = parseDayCounter(dc);

    string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    businessDayConvention_ = parseBusinessDayConvention(bdc);

    shortSwapIndexBase_ = XMLUtils::getChildValue(node, "ShortSwapIndexBase", true);
    swapIndexBase_ = XMLUtils::getChildValue(node, "SwapIndexBase", true);

    // optional smile stuff
    smileOptionTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "SmileOptionTenors");
    smileSwapTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "SmileSwapTenors");
    smileSpreads_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "SmileSpreads");
}

XMLNode* SwaptionVolatilityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("SwaptionVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else if (dimension_ == Dimension::Smile) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
    } else {
        QL_FAIL("Unkown Dimension in SwaptionVolatilityCurveConfig::toXML()");
    }

    if (volatilityType_ == VolatilityType::Normal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
    } else if (volatilityType_ == VolatilityType::Lognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
    } else if (volatilityType_ == VolatilityType::ShiftedLognormal) {
        XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
    } else {
        QL_FAIL("Unknown VolatilityType in SwaptionVolatilityCurveConfig::toXML()");
    }

    string extr_str = flatExtrapolation_ ? "Flat" : "Linear";
    if (!extrapolate_)
        extr_str = "None";
    XMLUtils::addChild(doc, node, "Extrapolation", extr_str);

    XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);
    XMLUtils::addGenericChildAsList(doc, node, "SwapTenors", swapTenors_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
    XMLUtils::addChild(doc, node, "ShortSwapIndexBase", shortSwapIndexBase_);
    XMLUtils::addChild(doc, node, "SwapIndexBase", swapIndexBase_);

    if (dimension_ == Dimension::Smile) {
        XMLUtils::addGenericChildAsList(doc, node, "SmileOptionTenors", smileOptionTenors_);
        XMLUtils::addGenericChildAsList(doc, node, "SmileSwapTenors", smileSwapTenors_);
        XMLUtils::addGenericChildAsList(doc, node, "SmileSpreads", smileSpreads_);
    }

    return node;
}
} // namespace data
} // namespace ore
