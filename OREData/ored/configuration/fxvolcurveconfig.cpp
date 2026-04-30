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
#include <ored/marketdata/curvespecparser.hpp>
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
    : FXVolatilityCurveConfig(curveID, curveDescription, dimension, expiries, strikes, fxSpotID,
                              fxForeignCurveID, fxDomesticCurveID, dayCounter, calendar,
                              to_string(interp), conventionsID, smileDelta, smileExtrapolation) {}

FXVolatilityCurveConfig::FXVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                 const Dimension& dimension, const vector<string>& expiries,
                                                 const vector<string>& strikes, const string& fxSpotID,
                                                 const string& fxForeignCurveID, const string& fxDomesticCurveID,
                                                 const DayCounter& dayCounter, const Calendar& calendar,
                                                 const string& interp, const string& conventionsID,
                                                 const std::vector<Size>& smileDelta, const string& smileExtrapolation)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), expiries_(expiries), dayCounter_(dayCounter),
      calendar_(calendar), fxSpotID_(fxSpotID), fxForeignYieldCurveID_(fxForeignCurveID),
      fxDomesticYieldCurveID_(fxDomesticCurveID), conventionsID_(conventionsID), smileDelta_(smileDelta),
      smileInterpolationStr_(interp), smileInterpolation_(SmileInterpolation::Linear), smileExtrapolation_(smileExtrapolation) {
    syncSmileInterpolation();
}

void FXVolatilityCurveConfig::syncSmileInterpolation() {
    // smileInterpolationStr_ is the canonical field: it holds the raw string from XML / constructor and
    // supports SVI variant names (e.g. "CorbettaEtAl2019Essvi", "Mingone2022EssviGJ") that have no enum counterpart.
    // smileInterpolation_ is derived from it and kept for backward compatibility with callers that
    // switch on the enum. SVI variants and any unrecognised strings fall back to Linear.
    if (smileInterpolationStr_ == "VannaVolga1") smileInterpolation_ = SmileInterpolation::VannaVolga1;
    else if (smileInterpolationStr_ == "VannaVolga2") smileInterpolation_ = SmileInterpolation::VannaVolga2;
    else if (smileInterpolationStr_ == "Cubic") smileInterpolation_ = SmileInterpolation::Cubic;
    else smileInterpolation_ = SmileInterpolation::Linear;
}

FXVolatilityCurveConfig::FXVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                                 const Dimension& dimension, const string& baseVolatility1,
                                                 const string& baseVolatility2, const string& fxIndexTag)
    : CurveConfig(curveID, curveDescription), dimension_(dimension), baseVolatility1_(baseVolatility1),
      baseVolatility2_(baseVolatility2), fxIndexTag_(fxIndexTag) {}

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
                    smileInterpolationStr_ = "VannaVolga2"; // default
                } else if (smileInterp == "VannaVolga1" || smileInterp == "VannaVolga2") {
                    smileInterpolationStr_ = smileInterp;
                } else {
                    QL_FAIL("SmileInterpolation " << smileInterp << " not supported for VannaVolga");
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
                    smileInterpolationStr_ = "Linear";
                } else if (smileInterp == "Cubic") {
                    smileInterpolationStr_ = "Cubic";
                } else {
                    // Could be an SVI variant — store it verbatim, validated at build time
                    smileInterpolationStr_ = smileInterp;
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
                    smileInterpolationStr_ = "Cubic";
                } else if (smileInterp == "Linear") {
                    smileInterpolationStr_ = "Linear";
                } else {
                    // Could be an SVI variant — store it verbatim, validated at build time
                    smileInterpolationStr_ = smileInterp;
                }
                string sDelta = XMLUtils::getChildValue(node, "SmileDelta");
                if (sDelta == "")
                    smileDelta_ = {10, 25};
                else
                    smileDelta_ = parseListOfValues<Size>(sDelta, &parseInteger);
            } else if (smileType == "Absolute") {
                dimension_ = Dimension::SmileAbsolute;
                if (smileInterp == "" || smileInterp == "Cubic") {
                    smileInterpolationStr_ = "Cubic";
                } else if (smileInterp == "Linear") {
                    smileInterpolationStr_ = "Linear";
                } else {
                    // Could be an SVI variant — store it verbatim, validated at build time
                    smileInterpolationStr_ = smileInterp;
                }
            } else {
                QL_FAIL("SmileType '" << smileType << "' not supported, expected VannaVolga, Delta, BFRR, Absolute");
            }
        } else {
            QL_FAIL("Dimension " << dim << " not supported yet");
        }
        expiries_ = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);

        bool curvesRequired = dimension_ == Dimension::SmileVannaVolga || dimension_ == Dimension::SmileDelta ||
                              dimension_ == Dimension::SmileBFRR;
        fxForeignYieldCurveID_ = XMLUtils::getChildValue(node, "FXForeignCurveID", curvesRequired);
        // the domestic curve id is always optional, if not given / empty it is set in the curve builder
        fxDomesticYieldCurveID_ = XMLUtils::getChildValue(node, "FXDomesticCurveID", false);
    }

    timeInterpolation_ =
        parseFxVolatilityTimeInterpolation(XMLUtils::getChildValue(node, "TimeInterpolation", false, "V"));
    timeWeighting_ = XMLUtils::getChildValue(node, "TimeWeighting", false);

    butterflyErrorTolerance_ = parseReal(XMLUtils::getChildValue(node, "ButterflyErrorTolerance", false, "0.01"));

    if (auto tmp = XMLUtils::getChildNode(node, "Report")) {
        reportConfig_.fromXML(tmp);
    }

    syncSmileInterpolation();

    parametricSmileConfiguration_ = QuantLib::ext::nullopt;
    if (XMLNode* n = XMLUtils::getChildNode(node, "ParametricSmileConfiguration")) {
        parametricSmileConfiguration_ = ParametricSmileConfiguration();
        parametricSmileConfiguration_->fromXML(n);
    }
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
        if (!smileInterpolationStr_.empty())
            XMLUtils::addChild(doc, node, "SmileInterpolation", smileInterpolationStr_);
        XMLUtils::addGenericChildAsList(doc, node, "SmileDelta", deltas_);
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
    } else if (dimension_ == Dimension::SmileDelta) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "Delta");
        if (!smileInterpolationStr_.empty())
            XMLUtils::addChild(doc, node, "SmileInterpolation", smileInterpolationStr_);
        if (!smileExtrapolation_.empty())
            XMLUtils::addChild(doc, node, "SmileExtrapolation", smileExtrapolation_);
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
        XMLUtils::addGenericChildAsList(doc, node, "Deltas", deltas_);
    } else if (dimension_ == Dimension::SmileBFRR) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "BFRR");
        if (!smileInterpolationStr_.empty())
            XMLUtils::addChild(doc, node, "SmileInterpolation", smileInterpolationStr_);
        XMLUtils::addGenericChildAsList(doc, node, "SmileDelta", smileDelta_);
        XMLUtils::addChild(doc, node, "Conventions", to_string(conventionsID_));
    } else if (dimension_ == Dimension::SmileAbsolute) {
        XMLUtils::addChild(doc, node, "Dimension", "Smile");
        XMLUtils::addChild(doc, node, "SmileType", "Absolute");
        if (!smileInterpolationStr_.empty())
            XMLUtils::addChild(doc, node, "SmileInterpolation", smileInterpolationStr_);
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
    XMLUtils::addChild(doc, node, "TimeInterpolation", to_string(timeInterpolation_));
    XMLUtils::addChild(doc, node, "TimeWeighting", timeWeighting_);
    XMLUtils::addChild(doc, node, "ButterflyErrorTolerance", butterflyErrorTolerance_);
    XMLUtils::appendNode(node, reportConfig_.toXML(doc));

    if (parametricSmileConfiguration_)
        XMLUtils::appendNode(node, parametricSmileConfiguration_->toXML(doc));
    
    return node;
}

void FXVolatilityCurveConfig::populateRequiredIds() const {

    if (!fxDomesticYieldCurveID_.empty()) {
        std::vector<string> tokens;
        split(tokens, fxDomesticYieldCurveID_, boost::is_any_of("/"));
        if (tokens.size() == 3 && tokens[0] == "Yield") {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(tokens[2]);
        } else if (tokens.size() == 1) {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(fxDomesticYieldCurveID_);
        } else {
            QL_FAIL("Cannot determine the required domestic yield curve for fx vol curve " << curveID_);
        }
    }

    if (!fxForeignYieldCurveID_.empty()) {
        std::vector<string> tokens;
        split(tokens, fxForeignYieldCurveID_, boost::is_any_of("/"));
        if (tokens.size() == 3 && tokens[0] == "Yield") {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(tokens[2]);
        } else if (tokens.size() == 1) {
            requiredCurveIds_[CurveSpec::CurveType::Yield].insert(fxForeignYieldCurveID_);
        } else {
            QL_FAIL("Cannot determine the required foreign yield curve for fx vol curve " << curveID_);
        }
    }

    if (dimension_ == Dimension::ATMTriangulated) {
        requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(baseVolatility1_);
        requiredCurveIds_[CurveSpec::CurveType::FXVolatility].insert(baseVolatility2_);

        auto spec = QuantLib::ext::dynamic_pointer_cast<FXSpotSpec>(parseCurveSpec(fxSpotID_));
        auto forTarget = spec->unitCcy();
        auto domTarget = spec->ccy();

        // we need to include inverse ccy pairs as well
        QL_REQUIRE(baseVolatility1_.size() == 6, "invalid ccy pair length");
        QL_REQUIRE(baseVolatility2_.size() == 6, "invalid ccy pair length");
        auto forBase1 = baseVolatility1_.substr(0, 3);
        auto domBase1 = baseVolatility1_.substr(3);
        auto forBase2 = baseVolatility2_.substr(0, 3);
        auto domBase2 = baseVolatility2_.substr(3);

        // Note: we do not add the inverse pairs (e.g. USDEUR, CHFEUR) as required here,
        // because buildATMTriangulated() looks up base vols by their original config IDs
        // (e.g. EURUSD, EURCHF) and handles inversion internally.

        // we need to establish the common currency between the two pairs to include the correlations
        std::string baseCcy = "";
        if (forBase1 == forBase2 || forBase1 == domBase2) {
            baseCcy = forBase1;
        } else {
            QL_REQUIRE(domBase1 == forBase2 || domBase1 == domBase2, "no common currency found for baseVolatilities");
            baseCcy = domBase1;
        }

        // We only declare one correlation pair as required; getCorrelationCurve() will
        // search all 8 permutations (straight, inverse, swapped) at runtime.
        std::string forIndex = "FX-" + fxIndexTag_ + "-" + forTarget + "-" + baseCcy;
        std::string domIndex = "FX-" + fxIndexTag_ + "-" + domTarget + "-" + baseCcy;
        requiredCurveIds_[CurveSpec::CurveType::Correlation].insert(forIndex + "&" + domIndex);
    }
}

FXVolatilityCurveConfig::TimeInterpolation parseFxVolatilityTimeInterpolation(const std::string& s) {
    if (s == "V") {
        return FXVolatilityCurveConfig::TimeInterpolation::V;
    } else if (s == "V2T") {
        return FXVolatilityCurveConfig::TimeInterpolation::V2T;
    } else {
        QL_FAIL("FxVolatilityCurveConfig::TimeInterpolation(): '" << s << "' not recognized, expected 'V' or 'V2T'");
    }
}

std::ostream& operator<<(std::ostream& out, FXVolatilityCurveConfig::TimeInterpolation t) {
    if (t == FXVolatilityCurveConfig::TimeInterpolation::V)
        return out << "V";
    else if (t == FXVolatilityCurveConfig::TimeInterpolation::V2T)
        return out << "V2T";
    else {
        QL_FAIL("operator<<(FXVolatilityCurveConfig::TimeInterpolation): enum "
                << static_cast<int>(t) << " not recognized. Internal error, contact dev.");
    }
}

std::ostream& operator<<(std::ostream& out, FXVolatilityCurveConfig::SmileInterpolation s) {
    switch (s) {
    case FXVolatilityCurveConfig::SmileInterpolation::VannaVolga1: return out << "VannaVolga1";
    case FXVolatilityCurveConfig::SmileInterpolation::VannaVolga2: return out << "VannaVolga2";
    case FXVolatilityCurveConfig::SmileInterpolation::Linear: return out << "Linear";
    case FXVolatilityCurveConfig::SmileInterpolation::Cubic: return out << "Cubic";
    default:
        QL_FAIL("operator<<(FXVolatilityCurveConfig::SmileInterpolation): enum "
                << static_cast<int>(s) << " not recognized. Internal error, contact dev.");
    }
}

} // namespace data
} // namespace ore
