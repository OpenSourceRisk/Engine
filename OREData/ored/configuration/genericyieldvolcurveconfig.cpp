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
#include <ored/configuration/genericyieldvolcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, GenericYieldVolatilityCurveConfig::VolatilityType t) {
    switch (t) {
    case GenericYieldVolatilityCurveConfig::VolatilityType::Lognormal:
        return out << "RATE_LNVOL";
    case GenericYieldVolatilityCurveConfig::VolatilityType::Normal:
        return out << "RATE_NVOL";
    case GenericYieldVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
        return out << "RATE_SLNVOL";
    default:
        QL_FAIL("unknown VolatilityType(" << Integer(t) << ")");
    }
}

GenericYieldVolatilityCurveConfig::GenericYieldVolatilityCurveConfig(
    const std::string& underlyingLabel, const std::string& rootNodeLabel, const std::string& marketDatumInstrumentLabel,
    const std::string& qualifierLabel, const string& curveID, const string& curveDescription,
    const std::string& qualifier, const Dimension dimension, const VolatilityType volatilityType,
    const VolatilityType outputVolatilityType, const Interpolation interpolation, const Extrapolation extrapolation,
    const vector<string>& optionTenors, const vector<string>& underlyingTenors, const DayCounter& dayCounter,
    const Calendar& calendar, const BusinessDayConvention& businessDayConvention, const string& shortSwapIndexBase,
    const string& swapIndexBase, const vector<string>& smileOptionTenors, const vector<string>& smileUnderlyingTenors,
    const vector<string>& smileSpreads,
    const boost::optional<ParametricSmileConfiguration>& parametricSmileConfiguration)
    : CurveConfig(curveID, curveDescription), underlyingLabel_(underlyingLabel), rootNodeLabel_(rootNodeLabel),
      marketDatumInstrumentLabel_(marketDatumInstrumentLabel), qualifierLabel_(qualifierLabel), allowSmile_(true),
      requireSwapIndexBases_(false), qualifier_(qualifier), dimension_(dimension), volatilityType_(volatilityType),
      interpolation_(interpolation), extrapolation_(extrapolation), optionTenors_(optionTenors),
      underlyingTenors_(underlyingTenors), dayCounter_(dayCounter), calendar_(calendar),
      businessDayConvention_(businessDayConvention), shortSwapIndexBase_(shortSwapIndexBase),
      swapIndexBase_(swapIndexBase), smileOptionTenors_(smileOptionTenors),
      smileUnderlyingTenors_(smileUnderlyingTenors), smileSpreads_(smileSpreads),
      parametricSmileConfiguration_(parametricSmileConfiguration) {

    QL_REQUIRE(dimension == Dimension::ATM || dimension == Dimension::Smile, "Invalid dimension");

    if (dimension != Dimension::Smile) {
        QL_REQUIRE(smileOptionTenors.size() == 0 && smileUnderlyingTenors.size() == 0 && smileSpreads.size() == 0,
                   "Smile tenors/strikes/spreads should only be set when dim=Smile");
    }

    if (qualifier_.empty()) {
        qualifier_ = ccyFromSwapIndexBase(swapIndexBase_);
    }
}

GenericYieldVolatilityCurveConfig::GenericYieldVolatilityCurveConfig(
    const std::string& underlyingLabel, const std::string& rootNodeLabel, const std::string& qualifierLabel,
    const string& curveID, const string& curveDescription, const string& qualifier,
    const std::string& proxySourceCurveId, const std::string& proxySourceShortSwapIndexBase,
    const std::string& proxySourceSwapIndexBase, const std::string& proxyTargetShortSwapIndexBase,
    const std::string& proxyTargetSwapIndexBase)
    : CurveConfig(curveID, curveDescription), underlyingLabel_(underlyingLabel), rootNodeLabel_(rootNodeLabel),
      qualifierLabel_(qualifierLabel), allowSmile_(true), requireSwapIndexBases_(false),
      proxySourceShortSwapIndexBase_(proxySourceShortSwapIndexBase),
      proxySourceSwapIndexBase_(proxySourceSwapIndexBase),
      proxyTargetShortSwapIndexBase_(proxyTargetShortSwapIndexBase),
      proxyTargetSwapIndexBase_(proxyTargetSwapIndexBase) {

    if (qualifier_.empty()) {
        qualifier_ = ccyFromSwapIndexBase(proxyTargetSwapIndexBase_);
    }

    populateRequiredCurveIds();
}

void GenericYieldVolatilityCurveConfig::populateRequiredCurveIds() {
    if (!proxySourceCurveId_.empty()) {
        requiredCurveIds_[CurveSpec::CurveType::SwaptionVolatility].insert(
            parseCurveSpec(proxySourceCurveId_)->curveConfigID());
    }
}

const vector<string>& GenericYieldVolatilityCurveConfig::quotes() {

    if (quotes_.size() == 0 && proxySourceCurveId_.empty()) {
        std::stringstream ssBase;
        ssBase << marketDatumInstrumentLabel_ << "/" << volatilityType_ << "/" << qualifier_ << "/";
        if (!quoteTag_.empty())
            ssBase << quoteTag_ << "/";
        string base = ssBase.str();

        // add atm vols (always required)
        for (auto o : optionTenors_) {
            for (auto s : underlyingTenors_) {
                std::stringstream ss;
                ss << base << o << "/" << s << "/ATM";
                quotes_.push_back(ss.str());
            }
        }

        // add Smile Spreads, if dimension is Smile
        if (dimension_ == Dimension::Smile) {
            for (auto o : smileOptionTenors_) {
                for (auto s : smileUnderlyingTenors_) {
                    for (auto sp : smileSpreads_) {
                        std::stringstream ss;
                        ss << base << o << "/" << s << "/Smile/" << sp;
                        quotes_.push_back(ss.str());
                    }
                }
            }
        }
        // add SHIFT quotes, if vol type is SLN
        for (auto s : underlyingTenors_) {
            if (volatilityType_ == VolatilityType::ShiftedLognormal) {
                std::stringstream ss;
                ss << marketDatumInstrumentLabel_ << "/SHIFT/" << qualifier_ << "/"
                   << (quoteTag_.empty() ? "" : quoteTag_ + "/") << s;
                quotes_.push_back(ss.str());
            }
        }
    }
    return quotes_;
}

string GenericYieldVolatilityCurveConfig::ccyFromSwapIndexBase(const std::string& swapIndexBase) {
    std::vector<string> tokens;
    split(tokens, swapIndexBase, boost::is_any_of("-"));
    QL_REQUIRE(!tokens.empty() && tokens[0] != "",
               "GenericYieldVolatilityCurveConfig::fromXML(): can not derive qualifier from SwapIndexBase ("
                   << swapIndexBase << ")");
    return tokens[0];
}

void GenericYieldVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, rootNodeLabel_);

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    if (auto p = XMLUtils::getChildNode(node, "ProxyConfig")) {
        // read in proxy config

        auto source = XMLUtils::getChildNode(p, "Source");
        QL_REQUIRE(source != nullptr,
                   "GenericYieldVolatilityCurveConfig::fromXML(): ProxyConfig requires child node 'Source'");
        proxySourceCurveId_ = XMLUtils::getChildValue(source, "CurveId");
        proxySourceShortSwapIndexBase_ = XMLUtils::getChildValue(source, "ShortSwapIndexBase");
        proxySourceSwapIndexBase_ = XMLUtils::getChildValue(source, "SwapIndexBase");

        auto target = XMLUtils::getChildNode(p, "Target");
        QL_REQUIRE(target != nullptr,
                   "GenericYieldVolatilityCurveConfig::fromXML(): ProxyConfig requires child node 'Target'");
        proxyTargetShortSwapIndexBase_ = XMLUtils::getChildValue(target, "ShortSwapIndexBase");
        proxyTargetSwapIndexBase_ = XMLUtils::getChildValue(target, "SwapIndexBase");

        populateRequiredCurveIds();

    } else {
        // read in quote-based config
        if (allowSmile_) {
            string dim = XMLUtils::getChildValue(node, "Dimension", true);
            if (dim == "ATM") {
                dimension_ = Dimension::ATM;
            } else if (dim == "Smile") {
                dimension_ = Dimension::Smile;
            } else {
                QL_FAIL("Dimension " << dim << " not recognized");
            }
        } else {
            dimension_ = Dimension::ATM;
        }

        string volType = XMLUtils::getChildValue(node, "VolatilityType", true);
        if (volType == "Normal") {
            volatilityType_ = VolatilityType::Normal;
        } else if (volType == "Lognormal") {
            volatilityType_ = VolatilityType::Lognormal;
        } else if (volType == "ShiftedLognormal") {
            volatilityType_ = VolatilityType::ShiftedLognormal;
        } else {
            QL_FAIL(
                "VolatilityType '" << volType
                                   << "' not recognized. Expected one of 'Normal', 'Lognormal', 'ShiftedLognormal'.");
        }

        string outVolType = XMLUtils::getChildValue(node, "OutputVolatilityType", false);
        if (outVolType.empty())
            outputVolatilityType_ = volatilityType_;
        else if (outVolType == "Normal") {
            outputVolatilityType_ = VolatilityType::Normal;
        } else if (outVolType == "Lognormal") {
            outputVolatilityType_ = VolatilityType::Lognormal;
        } else if (outVolType == "ShiftedLognormal") {
            outputVolatilityType_ = VolatilityType::ShiftedLognormal;
        } else {
            QL_FAIL("OutputVolatilityType '"
                    << outVolType << "' not recognized. Expected one of 'Normal', 'Lognormal', 'ShiftedLognormal'.");
        }

        string interp = XMLUtils::getChildValue(node, "Interpolation", false, "Linear");
        if (interp == "Linear") {
            interpolation_ = Interpolation::Linear;
        } else {
            try {
                interpolation_ = Interpolation(static_cast<int>(parseSabrParametricVolatilityModelVariant(interp)));
            } catch (const std::exception& e) {
                QL_FAIL("Interpolation '" << interp << "' not recognized. Expected 'Linear' or a SABR variant ("
                                          << e.what() << ")");
            }
        }

        string extr = XMLUtils::getChildValue(node, "Extrapolation", false, "Flat");
        if (extr == "Linear") {
            extrapolation_ = Extrapolation::Linear;
        } else if (extr == "Flat") {
            extrapolation_ = Extrapolation::Flat;
        } else if (extr == "None") {
            extrapolation_ = Extrapolation::None;
        } else  {
            QL_FAIL("Extrapolation " << extr << " not recognized, expected one of 'Linear', 'Flat', 'None'.");
        }

        optionTenors_ = XMLUtils::getChildrenValuesAsStrings(node, "OptionTenors", true);
        underlyingTenors_ = XMLUtils::getChildrenValuesAsStrings(node, underlyingLabel_ + "Tenors", false);

        string cal = XMLUtils::getChildValue(node, "Calendar", true);
        calendar_ = parseCalendar(cal);

        string dc = XMLUtils::getChildValue(node, "DayCounter", true);
        dayCounter_ = parseDayCounter(dc);

        string bdc = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
        businessDayConvention_ = parseBusinessDayConvention(bdc);

        if (requireSwapIndexBases_ || dimension_ == Dimension::Smile) {
            shortSwapIndexBase_ = XMLUtils::getChildValue(node, "ShortSwapIndexBase", true);
            swapIndexBase_ = XMLUtils::getChildValue(node, "SwapIndexBase", true);
        }

        // smile data
        if (dimension_ == Dimension::Smile) {
            smileOptionTenors_ = XMLUtils::getChildrenValuesAsStrings(node, "SmileOptionTenors", true);
            smileUnderlyingTenors_ =
                XMLUtils::getChildrenValuesAsStrings(node, "Smile" + underlyingLabel_ + "Tenors", true);
            smileSpreads_ = XMLUtils::getChildrenValuesAsStrings(node, "SmileSpreads", true);
        }

        // read qualifier from explicit field
        if (qualifierLabel_ != "")
            qualifier_ = XMLUtils::getChildValue(node, qualifierLabel_, true);

        // derive qualifier (=ccy) from swapIndexBase if not given explicitly
        if (qualifier_ == "")
            qualifier_ = ccyFromSwapIndexBase(swapIndexBase_);

        // optional quote tag to include
        quoteTag_ = XMLUtils::getChildValue(node, "QuoteTag", false);

        // Optional parametric smile configuration
        if(XMLNode* n = XMLUtils::getChildNode(node, "ParametricSmileConfiguration")) {
            parametricSmileConfiguration_ = ParametricSmileConfiguration();
            parametricSmileConfiguration_->fromXML(n);
        }
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Report")) {
        reportConfig_.fromXML(tmp);
    }
}

XMLNode* GenericYieldVolatilityCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(rootNodeLabel_);
    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (!proxySourceCurveId_.empty()) {
        // write out proxy config
        auto proxy = XMLUtils::addChild(doc, node, "ProxyConfig");
        auto source = XMLUtils::addChild(doc, proxy, "Source");
        auto target = XMLUtils::addChild(doc, proxy, "Target");
        XMLUtils::addChild(doc, source, "CurveId", proxySourceCurveId_);
        XMLUtils::addChild(doc, source, "ShortSwapIndexBase", proxySourceShortSwapIndexBase_);
        XMLUtils::addChild(doc, source, "SwapIndexBase", proxySourceSwapIndexBase_);
        XMLUtils::addChild(doc, target, "ShortSwapIndexBase", proxyTargetShortSwapIndexBase_);
        XMLUtils::addChild(doc, target, "SwapIndexBase", proxyTargetSwapIndexBase_);
    } else {
        // write out quote based config
        if (qualifierLabel_ != "")
            XMLUtils::addChild(doc, node, qualifierLabel_, qualifier_);

        if (dimension_ == Dimension::ATM) {
            XMLUtils::addChild(doc, node, "Dimension", "ATM");
        } else if (dimension_ == Dimension::Smile) {
            XMLUtils::addChild(doc, node, "Dimension", "Smile");
        } else {
            QL_FAIL("Unknown Dimension in GenericYieldVolatilityCurveConfig::toXML()");
        }

        if (volatilityType_ == VolatilityType::Normal) {
            XMLUtils::addChild(doc, node, "VolatilityType", "Normal");
        } else if (volatilityType_ == VolatilityType::Lognormal) {
            XMLUtils::addChild(doc, node, "VolatilityType", "Lognormal");
        } else if (volatilityType_ == VolatilityType::ShiftedLognormal) {
            XMLUtils::addChild(doc, node, "VolatilityType", "ShiftedLognormal");
        } else {
            QL_FAIL("Unknown VolatilityType in GenericYieldVolatilityCurveConfig::toXML()");
        }

        if (outputVolatilityType_ == VolatilityType::Normal) {
            XMLUtils::addChild(doc, node, "OutputVolatilityType", "Normal");
        } else if (outputVolatilityType_ == VolatilityType::Lognormal) {
            XMLUtils::addChild(doc, node, "OutputVolatilityType", "Lognormal");
        } else if (outputVolatilityType_ == VolatilityType::ShiftedLognormal) {
            XMLUtils::addChild(doc, node, "OutputVolatilityType", "ShiftedLognormal");
        } else {
            QL_FAIL("Unknown OutputVolatilityType in GenericYieldVolatilityCurveConfig::toXML()");
        }

        std::string extr_string;
        if (extrapolation_ == Extrapolation::None) {
            extr_string = "None";
        } else if (extrapolation_ == Extrapolation::Linear) {
            extr_string = "Linear";
        } else if (extrapolation_ == Extrapolation::Flat) {
            extr_string = "Flat";
        }
        XMLUtils::addChild(doc, node, "Extrapolation", extr_string);

        XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
        XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
        XMLUtils::addChild(doc, node, "BusinessDayConvention", to_string(businessDayConvention_));
        XMLUtils::addGenericChildAsList(doc, node, "OptionTenors", optionTenors_);
        XMLUtils::addGenericChildAsList(doc, node, underlyingLabel_ + "Tenors", underlyingTenors_);

        if (requireSwapIndexBases_ || dimension_ == Dimension::Smile) {
            XMLUtils::addChild(doc, node, "ShortSwapIndexBase", shortSwapIndexBase_);
            XMLUtils::addChild(doc, node, "SwapIndexBase", swapIndexBase_);
        }

        if (dimension_ == Dimension::Smile) {
            XMLUtils::addGenericChildAsList(doc, node, "SmileOptionTenors", smileOptionTenors_);
            XMLUtils::addGenericChildAsList(doc, node, "Smile" + underlyingLabel_ + "Tenors", smileUnderlyingTenors_);
            XMLUtils::addGenericChildAsList(doc, node, "SmileSpreads", smileSpreads_);
        }

        if (!quoteTag_.empty()) {
            XMLUtils::addChild(doc, node, "QuoteTag", quoteTag_);
        }
    }

    XMLUtils::appendNode(node, reportConfig_.toXML(doc));

    return node;
}

} // namespace data
} // namespace ore
