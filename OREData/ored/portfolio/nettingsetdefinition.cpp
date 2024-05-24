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

#include <ored/portfolio/nettingsetdefinition.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <utility>

namespace ore {
namespace data {

CSA::Type parseCsaType(const string& s) {
    static map<string, CSA::Type> t = {
        {"Bilateral", CSA::Bilateral}, {"CallOnly", CSA::CallOnly}, {"PostOnly", CSA::PostOnly}};

    auto it = t.find(s);
    if (it != t.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to CSA::Type");
    }
}

std::ostream& operator<<(std::ostream& out, CSA::Type t) {
    switch (t) {
    case CSA::Bilateral:
        return out << "Bilateral";
    case CSA::CallOnly:
        return out << "CallOnly";
    case CSA::PostOnly:
        return out << "PostOnly";
    default:
        QL_FAIL("csa type not covered");
    }
}

void CSA::invertCSA() {
    if (type_ != Bilateral) {
        type_ = (type_ == CallOnly ? PostOnly : CallOnly);
    }
    if (initialMarginType_ != Bilateral) {
        initialMarginType_ = (initialMarginType_ == CallOnly ? PostOnly : CallOnly);
    }
    std::swap(collatSpreadPay_, collatSpreadRcv_);
    std::swap(thresholdPay_, thresholdRcv_);
    std::swap(mtaPay_, mtaRcv_);
    iaHeld_ *= -1;
    std::swap(marginCallFreq_, marginPostFreq_);
}

void CSA::validate() {
    QL_REQUIRE(csaCurrency_.size() == 3, "NettingSetDefinition build error;"
                                             << " CSA currency should be a three-letter ISO code");

    QL_REQUIRE(thresholdPay_ >= 0, "NettingSetDefinition build error; negative thresholdPay");
    QL_REQUIRE(thresholdRcv_ >= 0, "NettingSetDefinition build error; negative thresholdRcv");
    QL_REQUIRE(mtaPay_ >= 0, "NettingSetDefinition build error; negative mtaPay");
    QL_REQUIRE(mtaRcv_ >= 0, "NettingSetDefinition build error; negative mtaRcv");
    QL_REQUIRE(iaType_ == "FIXED", "NettingSetDefinition build error;"
                                       << " unsupported independent amount type; " << iaType_);

    QL_REQUIRE(marginCallFreq_ > Period(0, Days) && marginPostFreq_ > Period(0, Days),
               "NettingSetDefinition build error;"
                   << " non-positive margining frequency");
    QL_REQUIRE(mpr_ >= Period(0, Days), "NettingSetDefinition build error;"
                                            << " negative margin period of risk");
    if (mpr_ < marginCallFreq_ || mpr_ < marginPostFreq_) {
        LOG("NettingSetDefinition has CSA margining frequency ("
            << marginCallFreq_ << ", " << marginPostFreq_ << ") longer than assumed margin period of risk " << mpr_);
    }

    for (Size i = 0; i < eligCollatCcys_.size(); i++) {
        QL_REQUIRE(eligCollatCcys_[i].size() == 3,
                   "NettingSetDefinition build error;"
                       << "EligibleCollaterals currency should be a three-letter ISO code");
    }

    // unilateral CSA - set threshold near infinity to disable margining
    switch (type_) {
    case CallOnly: {
        thresholdPay_ = std::numeric_limits<double>::max();
        break;
    }
    case PostOnly: {
        thresholdRcv_ = std::numeric_limits<double>::max();
    }
    default:
        break;
    }
}

NettingSetDefinition::NettingSetDefinition(XMLNode* node) {
    fromXML(node);
    DLOG(nettingSetDetails_ << ": NettingSetDefinition built from XML... ");
}

NettingSetDefinition::NettingSetDefinition(const NettingSetDetails& nettingSetDetails)
    : nettingSetDetails_(nettingSetDetails), activeCsaFlag_(false) {
    validate();
    DLOG(nettingSetDetails_ << ": uncollateralised NettingSetDefinition built.");
}

NettingSetDefinition::NettingSetDefinition(const NettingSetDetails& nettingSetDetails, const string& bilateral,
                                           const string& csaCurrency, const string& index, const Real& thresholdPay,
                                           const Real& thresholdRcv, const Real& mtaPay, const Real& mtaRcv,
                                           const Real& iaHeld, const string& iaType, const string& marginCallFreq,
                                           const string& marginPostFreq, const string& mpr, const Real& collatSpreadPay,
                                           const Real& collatSpreadRcv, const vector<string>& eligCollatCcys,
                                           bool applyInitialMargin, const string& initialMarginType,
                                           const bool calculateIMAmount, const bool calculateVMAmount,
                                           const string& nonExemptIMRegulations)
    : nettingSetDetails_(nettingSetDetails), activeCsaFlag_(true) {

    csa_ = QuantLib::ext::make_shared<CSA>(
        parseCsaType(bilateral), csaCurrency, index, thresholdPay, thresholdRcv, mtaPay, mtaRcv, iaHeld, iaType,
        parsePeriod(marginCallFreq), parsePeriod(marginPostFreq), parsePeriod(mpr), collatSpreadPay, collatSpreadRcv,
        eligCollatCcys, applyInitialMargin, parseCsaType(initialMarginType), calculateIMAmount, calculateVMAmount,
        nonExemptIMRegulations);

    validate();
    DLOG(nettingSetDetails_ << ": collateralised NettingSetDefinition built. ");
}

void NettingSetDefinition::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "NettingSet");

    // Read in the mandatory nodes.
    XMLNode* nettingSetDetailsNode = XMLUtils::getChildNode(node, "NettingSetDetails");
    if (nettingSetDetailsNode) {
        nettingSetDetails_.fromXML(nettingSetDetailsNode);
    } else {
        nettingSetId_ = XMLUtils::getChildValue(node, "NettingSetId", false);
        nettingSetDetails_ = NettingSetDetails(nettingSetId_);
    }

    activeCsaFlag_ = XMLUtils::getChildValueAsBool(node, "ActiveCSAFlag", false, true);

    // Load "CSA" information, if necessary
    if (activeCsaFlag_) {
        XMLNode* csaChild = XMLUtils::getChildNode(node, "CSADetails");
        XMLUtils::checkNode(csaChild, "CSADetails");

        string csaTypeStr = XMLUtils::getChildValue(csaChild, "Bilateral", false);
        if (csaTypeStr.empty())
            csaTypeStr = "Bilateral";
        string csaCurrency = XMLUtils::getChildValue(csaChild, "CSACurrency", false);
        string index = XMLUtils::getChildValue(csaChild, "Index", false);
        Real thresholdPay = XMLUtils::getChildValueAsDouble(csaChild, "ThresholdPay", false, 0.0);
        Real thresholdRcv = XMLUtils::getChildValueAsDouble(csaChild, "ThresholdReceive", false, 0.0);
        Real mtaPay = XMLUtils::getChildValueAsDouble(csaChild, "MinimumTransferAmountPay", false, 0.0);
        Real mtaRcv = XMLUtils::getChildValueAsDouble(csaChild, "MinimumTransferAmountReceive", false, 0.0);
        string mprStr = XMLUtils::getChildValue(csaChild, "MarginPeriodOfRisk", false);
        if (mprStr.empty())
            mprStr = "2W";
        Real collatSpreadRcv =
            XMLUtils::getChildValueAsDouble(csaChild, "CollateralCompoundingSpreadReceive", false, 0.0);
        Real collatSpreadPay = XMLUtils::getChildValueAsDouble(csaChild, "CollateralCompoundingSpreadPay", false, 0.0);

        string marginCallFreqStr, marginPostFreqStr;
        if (XMLNode* freqChild = XMLUtils::getChildNode(csaChild, "MarginingFrequency")) {
            marginCallFreqStr = XMLUtils::getChildValue(freqChild, "CallFrequency", false);
            marginPostFreqStr = XMLUtils::getChildValue(freqChild, "PostFrequency", false);
        }
        if (marginCallFreqStr.empty())
            marginCallFreqStr = "1D";
        if (marginPostFreqStr.empty())
            marginPostFreqStr = "1D";

        Real iaHeld = 0.0;
        string iaType;
        if (XMLNode* iaChild = XMLUtils::getChildNode(csaChild, "IndependentAmount")) {
            iaHeld = XMLUtils::getChildValueAsDouble(iaChild, "IndependentAmountHeld", false, 0.0);
            iaType = XMLUtils::getChildValue(iaChild, "IndependentAmountType", false);
        }
        if (iaType.empty())
            iaType = "FIXED";

        vector<string> eligCollatCcys;
        if (XMLNode* collatChild = XMLUtils::getChildNode(csaChild, "EligibleCollaterals")) {
            eligCollatCcys = XMLUtils::getChildrenValues(collatChild, "Currencies", "Currency", false);
        }

        bool applyInitialMargin = XMLUtils::getChildValueAsBool(csaChild, "ApplyInitialMargin", false, false);

        string initialMarginType = XMLUtils::getChildValue(csaChild, "InitialMarginType", false);
        if (initialMarginType.empty())
            initialMarginType = "Bilateral";

        bool calculateIMAmount = XMLUtils::getChildValueAsBool(csaChild, "CalculateIMAmount", false, false);
        bool calculateVMAmount = XMLUtils::getChildValueAsBool(csaChild, "CalculateVMAmount", false, false);

        string nonExemptIMRegulations = XMLUtils::getChildValue(csaChild, "NonExemptIMRegulations", false);

        csa_ = QuantLib::ext::make_shared<CSA>(parseCsaType(csaTypeStr), csaCurrency, index, thresholdPay, thresholdRcv, mtaPay,
                                       mtaRcv, iaHeld, iaType, parsePeriod(marginCallFreqStr),
                                       parsePeriod(marginPostFreqStr), parsePeriod(mprStr), collatSpreadPay,
                                       collatSpreadRcv, eligCollatCcys, applyInitialMargin,
                                       parseCsaType(initialMarginType), calculateIMAmount, calculateVMAmount,
                                       nonExemptIMRegulations);
    }

    validate();
}

XMLNode* NettingSetDefinition::toXML(XMLDocument& doc) const {
    // Allocate a node.
    XMLNode* node = doc.allocNode("NettingSet");

    // Add the mandatory members.
    if (nettingSetDetails_.emptyOptionalFields()) {
        XMLUtils::addChild(doc, node, "NettingSetId", nettingSetId_);
    } else {
        XMLUtils::appendNode(node, nettingSetDetails_.toXML(doc));
    }
    XMLUtils::addChild(doc, node, "ActiveCSAFlag", activeCsaFlag_);

    XMLNode* csaSubNode = doc.allocNode("CSADetails");
    XMLUtils::appendNode(node, csaSubNode);

    if (activeCsaFlag_) {
        QL_REQUIRE(csa_, "CSA details not defined");

        XMLUtils::addChild(doc, csaSubNode, "Bilateral", to_string(csa_->type()));
        XMLUtils::addChild(doc, csaSubNode, "CSACurrency", csa_->csaCurrency());
        XMLUtils::addChild(doc, csaSubNode, "ThresholdPay", csa_->thresholdPay());
        XMLUtils::addChild(doc, csaSubNode, "ThresholdReceive", csa_->thresholdRcv());
        XMLUtils::addChild(doc, csaSubNode, "MinimumTransferAmountPay", csa_->mtaPay());
        XMLUtils::addChild(doc, csaSubNode, "MinimumTransferAmountReceive", csa_->mtaRcv());
        XMLUtils::addChild(doc, csaSubNode, "MarginPeriodOfRisk", to_string(csa_->marginPeriodOfRisk()));
        XMLUtils::addChild(doc, csaSubNode, "CollateralCompoundingSpreadPay", csa_->collatSpreadPay());
        XMLUtils::addChild(doc, csaSubNode, "CollateralCompoundingSpreadReceive", csa_->collatSpreadRcv());

        XMLNode* freqSubNode = doc.allocNode("MarginingFrequency");
        XMLUtils::appendNode(csaSubNode, freqSubNode);
        XMLUtils::addChild(doc, freqSubNode, "CallFrequency", to_string(csa_->marginCallFrequency()));
        XMLUtils::addChild(doc, freqSubNode, "PostFrequency", to_string(csa_->marginPostFrequency()));

        XMLNode* iaSubNode = doc.allocNode("IndependentAmount");
        XMLUtils::appendNode(csaSubNode, iaSubNode);
        XMLUtils::addChild(doc, iaSubNode, "IndependentAmountHeld", csa_->independentAmountHeld());
        XMLUtils::addChild(doc, iaSubNode, "IndependentAmountType", csa_->independentAmountType());

        XMLNode* collatSubNode = doc.allocNode("EligibleCollaterals");
        XMLUtils::appendNode(csaSubNode, collatSubNode);
        XMLUtils::addChildren(doc, collatSubNode, "Currencies", "Currency", csa_->eligCollatCcys());

        XMLUtils::addChild(doc, csaSubNode, "ApplyInitialMargin", csa_->applyInitialMargin());
        XMLUtils::addChild(doc, csaSubNode, "InitialMarginType", to_string(csa_->initialMarginType()));
        XMLUtils::addChild(doc, csaSubNode, "CalculateIMAmount", csa_->calculateIMAmount());
        XMLUtils::addChild(doc, csaSubNode, "CalculateVMAmount", csa_->calculateVMAmount());
        XMLUtils::addChild(doc, csaSubNode, "NonExemptIMRegulations", csa_->nonExemptIMRegulations());
    }

    return node;
}

void NettingSetDefinition::validate() {
    string nettingSetLog = nettingSetDetails_.empty() ? nettingSetId_ : ore::data::to_string(nettingSetDetails_);
    LOG(nettingSetLog << ": Validating netting set definition");
    QL_REQUIRE(nettingSetId_.size() > 0 || !nettingSetDetails_.empty(),
               "NettingSetDefinition build error; no netting set ID or netting set details");

    if (activeCsaFlag_) {
        QL_REQUIRE(csa_, "CSA not defined yet");
        string nettingSetLog = nettingSetDetails_.empty() ? nettingSetId_ : ore::data::to_string(nettingSetDetails_);
        LOG(nettingSetLog << ": Validating netting set definition's CSA details");
        csa_->validate();
    }
}
} // namespace data
} // namespace ore
