/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2024 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/configuration/fxvolcurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

FXVolatilityCurveConfig::FXVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                 const Dimension& dimension, const vector<string>& expiries,
                                                 const vector<string>& strikes, const string& fxSpotID,
                                                 const string& fxForeignCurveID, const string& fxDomesticCurveID,
                                                 const DayCounter& dayCounter, const Calendar& calendar,
                                                 const SmileInterpolation& interp, const string& conventionsID,
                                                 const std::vector<Size>& smileDelta, const string& smileExtrapolation)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), expiries_(expiries), dayCounter_(dayCounter),
      calendar_(calendar), fxSpotID_(fxSpotID), fxForeignYieldCurveID_(fxForeignCurveID),
      fxDomesticYieldCurveID_(fxDomesticCurveID), conventionsID_(conventionsID), smileDelta_(smileDelta),
      smileInterpolation_(interp), smileExtrapolation_(smileExtrapolation) {
    populateRequiredCurveIds();
}

FXVolatilityCurveConfig::FXVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                 const Dimension& dimension, const string& baseVolatility1,
                                                 const string& baseVolatility2, const string& fxIndexTag)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), baseVolatility1_(baseVolatility1),
      baseVolatility2_(baseVolatility2), fxIndexTag_(fxIndexTag) {
    populateRequiredCurveIds();
}

const vector<string>& FXVolatilityCurveConfig::quotes() {
    if (quotes_.size() == 0) {
        vector<string> tokens;
        boost::split(tokens, fxSpotID(), boost::is_any_of("/"));
        QL_REQUIRE(tokens.size() == 3, "Expected 3 tokens FX/CCY1/CCY2 in fxSpotID (" << fxSpotID() << ")");
        quotes_.push_back("FX/RATE/" + tokens[1] + "/" + tokens[2]);
        string base = "FX_OPTION/RATE_LNVOL/" + tokens[1] + "/" + tokens[2] + "/";
        for (auto e : expiries_) {
            quotes_.push_back(base + e + "/ATM");
            if (dimension_ == Dimension::SmileVannaVolga || dimension_ == Dimension::SmileBFRR) {
                for (auto const& d : smileDelta_) {
                    quotes_.push_back(base + e + "/" + to_string(d) + "RR");
                    quotes_.push_back(base + e + "/" + to_string(d) + "BF");
                }
            } else if (dimension_ == Dimension::SmileDelta || dimension_ == Dimension::SmileAbsolute) {
                for (auto d : deltas_) {
                    quotes_.push_back(base + e + "/" + d);
                }
            }
        }
    }
    return quotes_;
}

void FXVolatilityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "FXVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    string dim = XMLUtils::getChildValue(node, "Dimension", true);
    string cal = XMLUtils::getChildValue(node, "Calendar");
    string smileInterp = XMLUtils::getChildValue(node, "SmileInterpolation");

    fxSpotID_ = XMLUtils::getChildValue(node, "FXSpotID", true);

    vector<string> tokens;
    boost::split(tokens, fxSpotID_, boost::is_any_of("/"));
    QL_REQUIRE(tokens.size() == 3, "Expected 3 tokens FX/CCY1/CCY2 in fxSpotID (" << fxSpotID_ << ")");
    if (cal == "") {
        cal = tokens[1] + "," + tokens[2];
    }
    calendar_ = parseCalendar(cal);

    string dc = XMLUtils::getChildValue(node, "DayCounter");
    if (dc == "")
        dc = "A365";
    dayCounter_ = parseDayCounter(dc);

    if (dim == "ATMTriangulated") {
        dimension_ = Dimension::ATMTriangulated;
        baseVolatility1_ = XMLUtils::getChildValue(node, "BaseVolatility1", true);
        baseVolatility2_ = XMLUtils::getChildValue(node, "BaseVolatility2", true);

        string fxIndexTag = XMLUtils::getChildValue(node, "FXIndexTag");
        if (fxIndexTag == "")
            fxIndexTag = "GENERIC";
        fxIndexTag_ = fxIndexTag;
    } else {
        if (dim == "ATM") {
            dimension_ = Dimension::ATM;
        } else if (dim == "Smile") {

            conventionsID_ = XMLUtils::getChildValue(node, "Conventions", false);
            string smileType = XMLUtils::getChildValue(node, "SmileType");
            if (smileType == "" || smileType == "VannaVolga") {
                dimension_ = Dimension::SmileVannaVolga;

                // only read smile interpolation method if dimension is smile.
                if (smileInterp == "") {
                    smileInterpolation_ = SmileInterpolation::VannaVolga2; // default to VannaVolga 2nd approximation
                } else if (smileInterp == "VannaVolga1") {
                    smileInterpolation_ = SmileInterpolation::VannaVolga1;
                } else if (smileInterp == "VannaVolga2") {
                    smileInterpolation_ = SmileInterpolation::VannaVolga2;
                } else {
                    QL_FAIL("SmileInterpolation " << smileInterp << " not supported");
                }

                string sDelta = XMLUtils::getChildValue(node, "SmileDelta");
                if (sDelta == "")
                    smileDelta_ = {25};
                else
                    smileDelta_ = parseListOfValues<Size>(sDelta, &parseInteger);
            } else if (smileType == "Delta") {
                dimension_ = Dimension::SmileDelta;
                // only read smile interpolation and extrapolation method if dimension is smile.
                if (smileInterp == "" || smileInterp == "Linear") {
                    smileInterpolation_ = SmileInterpolation::Linear;
                } else if (smileInterp == "Cubic") {
                    smileInterpolation_ = SmileInterpolation::Cubic;
                } else {
                    QL_FAIL("SmileInterpolation " << smileInterp << " not supported");
                }

                smileExtrapolation_ = XMLUtils::getChildValue(node, "SmileExtrapolation", false, "Flat");

                deltas_ = XMLUtils::getChildrenValuesAsStrings(node, "Deltas", true);

                // check that these are valid deltas
                for (auto d : deltas_) {
                    QL_REQUIRE(d == "ATM" || d.back() == 'P' || d.back() == 'C',
                               "this is not a valid value for delta, " << d);
                    if (d != "ATM") {
                        parseReal(d.substr(0, d.size() - 1));
                    }
                }
            } else if (smileType == "BFRR") {
                dimension_ = Dimension::SmileBFRR;
                if (smileInterp == "" || smileInterp == "Cubic") {
                    smileInterpolation_ = SmileInterpolation::Cubic;
                } else if (smileInterp == "Linear") {
                    smileInterpolation_ = SmileInterpolation::Linear;
                } else {
                    QL_FAIL("SmileInterpolation " << smileInterp << " not supported");
                }
                string sDelta = XMLUtils::getChildValue(node, "SmileDelta");
                if (sDelta == "")
                    smileDelta_ = {10, 25};
                else
                    smileDelta_ = parseListOfValues<Size>(sDelta, &parseInteger);
            } else if (smileType == "Absolute") {
                dimension_ = Dimension::SmileAbsolute;
                if (smileInterp == "" || smileInterp == "Cubic") {
                    smileInterpolation_ = SmileInterpolation::Cubic;
                } else if (smileInterp == "Linear") {
                    smileInterpolation_ = SmileInterpolation::Linear;
                } else {
                    QL_FAIL("SmileInterpolation " << smileInterp << " not supported");
                }
            } else {
                QL_FAIL("SmileType '" << smileType << "' not supported, expected VannaVolga, Delta, BFRR");
            }
        } else {
            QL_FAIL("Dimension " << dim << " not supported yet");
        }
        expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);

        bool curvesRequired = dimension_ == Dimension::SmileVannaVolga || dimension_ == Dimension::SmileDelta ||
                              dimension_ == Dimension::SmileBFRR;
        fxForeignYieldCurveID_ = XMLUtils::getChildValue(node, "FXForeignCurveID", curvesRequired);
        fxDomesticYieldCurveID_ = XMLUtils::getChildValue(node, "FXDomesticCurveID", curvesRequired);
    }

    if (auto tmp = XMLUtils::getChildNode(node, "Report")) {
        reportConfig_.fromXML(tmp);
    }

    populateRequiredCurveIds();
}

XMLNode* FXVolatilityCurveConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("FXVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    if (dimension_ == Dimension::ATM) {
        XMLUtils::addChild(doc, node, "Dimension", "ATM");
    } else if (dimension_ == Dimension::ATMTriangulated) {
        XMLUtils::addChild(doc, node, "Dimension", "ATMTriangulated");
        XMLUtils::addChild(doc, node, "FXSpotID", fxSpotID_);
        XMLUtils::addChild(doc, node, "FXIndexTag", fxIndexTag_);
        XMLUtils::addChild(doc, node, "BaseVolatility1", baseVolatility1_);
        XMLUtils::addChild(doc, node, "BaseVolatility2", baseVolatility2_);

        return node;
    } else if (dimension_ == Dimension::SmileVannaVolga) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "VannaVolga");
        // only write smile interpolation if dimension is smile
        if (smileInterpolation_ == SmileInterpolation::VannaVolga1) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "VannaVolga1");
        } else if (smileInterpolation_ == SmileInterpolation::VannaVolga2) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "VannaVolga2");
        } else {
            QL_FAIL("Unknown SmileInterpolation in FXVolatilityCurveConfig::toXML()");
        }
        XMLUtils::addGenericChildAsList(doc, node, "SmileDelta", deltas_);
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
    } else if (dimension_ == Dimension::SmileDelta) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "Delta");
        // only write smile interpolation if dimension is smile
        if (smileInterpolation_ == SmileInterpolation::Linear) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Linear");
        } else if (smileInterpolation_ == SmileInterpolation::Cubic) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Cubic");
        } else {
            QL_FAIL("Unknown SmileInterpolation in FXVolatilityCurveConfig::toXML()");
        }
        if (!smileExtrapolation_.empty())
            XMLUtils::addChild(doc, node, "SmileExtrapolation", smileExtrapolation_);
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
        XMLUtils::addGenericChildAsList(doc, node, "Deltas", deltas_);
    } else if (dimension_ == Dimension::SmileBFRR) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "BFRR");
        if (smileInterpolation_ == SmileInterpolation::Linear) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Linear");
        } else if (smileInterpolation_ == SmileInterpolation::Cubic) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Cubic");
        } else {
            QL_FAIL("Unknown SmileInterpolation in FXVolatilityCurveConfig::toXML()");
        }
        XMLUtils::addGenericChildAsList(doc, node, "SmileDelta", smileDelta_);
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
    } else if (dimension_ == Dimension::SmileAbsolute) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "Absolute");
        if (smileInterpolation_ == SmileInterpolation::Linear) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Linear");
        } else if (smileInterpolation_ == SmileInterpolation::Cubic) {
            XMLUtils::addChild(doc, node, "SmileInterpolation", "Cubic");
        } else {
            QL_FAIL("Unknown SmileInterpolation in FXVolatilityCurveConfig::toXML()");
        }
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
    } else {
        QL_FAIL("Unknown Dimension in FXVolatilityCurveConfig::toXML()");
    }
    XMLUtils::addGenericChildAsList(doc, node, "Expiries", expiries_);
    XMLUtils::addChild(doc, node, "FXSpotID", fxSpotID_);
    if (!fxForeignYieldCurveID_.empty())
        XMLUtils::addChild(doc, node, "FXForeignCurveID", fxForeignYieldCurveID_);
    if (!fxDomesticYieldCurveID_.empty())
        XMLUtils::addChild(doc, node, "FXDomesticCurveID", fxDomesticYieldCurveID_);
    XMLUtils::addChild(doc, node, "Calendar", to_string(calendar_));
    XMLUtils::addChild(doc, node, "DayCounter", to_string(dayCounter_));
    XMLUtils::appendNode(node, reportConfig_.toXML(doc));
    
    return node;
}

void FXVolatilityCurveConfig::populateRequiredCurveIds() {
    if (!fxDomesticYieldCurveID_.empty() && !fxForeignYieldCurveID_.empty()) {
        std::vector<string> domTokens, forTokens;
        split(domTokens, fxDomesticYieldCurveID_, boost::is_any_of("/"));
        split(forTokens, fxForeignYieldCurveID_, boost::is_any_of("/"));

        if (domTokens.size() == 3 && domTokens[0] == "Yield") {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(domTokens[2]);
        } else if (domTokens.size() == 1) {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(fxDomesticYieldCurveID_);
        } else {
            QL_FAIL("Cannot determine the required domestic yield curve for fx vol curve " << curveID_);
        }

        if (forTokens.size() == 3 && forTokens[0] == "Yield") {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(forTokens[2]);
        } else if (forTokens.size() == 1) {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(fxForeignYieldCurveID_);
        } else {
            QL_FAIL("Cannot determine the required foreign yield curve for fx vol curve " << curveID_);
        }
    }

    if (dimension_ == Dimension::ATMTriangulated) {
        requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(baseVolatility1_);
        requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(baseVolatility2_);

        vector<string> tokens;
        boost::split(tokens, fxSpotID_, boost::is_any_of("/"));
        QL_REQUIRE(tokens.size() == 3, "unexpected fxSpot format: " << fxSpotID_);
        auto forTarget = tokens[1];
        auto domTarget = tokens[2];

        // we need to include inverse ccy pairs as well
        QL_REQUIRE(baseVolatility1_.size() == 6, "invalid ccy pair length");
        QL_REQUIRE(baseVolatility2_.size() == 6, "invalid ccy pair length");
        auto forBase1 = baseVolatility1_.substr(0, 3);
        auto domBase1 = baseVolatility1_.substr(3);
        auto forBase2 = baseVolatility2_.substr(0, 3);
        auto domBase2 = baseVolatility2_.substr(3);

        requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(domBase1 + forBase1);
        requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(domBase2 + forBase2);

        // we need to establish the common currency between the two pairs to include the correlations
        std::string baseCcy = "";
        if (forBase1 == forBase2 || forBase1 == domBase2) {
            baseCcy = forBase1;
        } else {
            QL_REQUIRE(domBase1 == forBase2 || domBase1 == domBase2, "no common currency found for baseVolatilities");
            baseCcy = domBase1;
        }

        // straight pair
        std::string forIndex = "FX-" + fxIndexTag_ + "-" + forTarget + "-" + baseCcy;
        std::string domIndex = "FX-" + fxIndexTag_ + "-" + domTarget + "-" + baseCcy;
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(forIndex + "&" + domIndex);
        // inverse pair
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(domIndex + "&" + forIndex);
        // inverse fx index1
        std::string forIndexInverse = "FX-" + fxIndexTag_ + "-" + baseCcy + "-" + forTarget;
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(forIndexInverse + "&" + domIndex);
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(domIndex + "&" + forIndexInverse);
        // inverse fx index2
        std::string domIndexInverse = "FX-" + fxIndexTag_ + "-" + baseCcy + "-" + domTarget;
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(forIndex + "&" + domIndexInverse);
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(domIndexInverse + "&" + forIndex);
        // both fx indices inverted
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(forIndexInverse + "&" + domIndexInverse);
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(domIndexInverse + "&" + forIndexInverse);
    }
}
} // namespace data
} // namespace ore
