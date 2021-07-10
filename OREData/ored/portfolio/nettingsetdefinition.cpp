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

void CSA::validate(string nettingSetId) {
    QL_REQUIRE(csaCurrency_.size() == 3, "NettingSetDefinition build error;"
                                             << " csa currency should be a three-letter ISO code");

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
        LOG("NettingSet " << nettingSetId << " has CSA margining frequency (" << marginCallFreq_ << ", "
                          << marginPostFreq_ << ") longer than assumed margin period of risk " << mpr_);
    }

    for (Size i = 0; i < eligCollatCcys_.size(); i++) {
        QL_REQUIRE(eligCollatCcys_[i].size() == 3, "NettingSetDefinition build error;"
                                                       << " three-letter ISO code expected");
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
    DLOG("NettingSetDefinition built from XML... " << nettingSetId_);
}

NettingSetDefinition::NettingSetDefinition(const string& nettingSetId, const string& ctp)
    : nettingSetId_(nettingSetId), ctp_(ctp), activeCsaFlag_(false) {
    validate();
    DLOG("uncollateralised NettingSetDefinition built... " << nettingSetId_);
}

NettingSetDefinition::NettingSetDefinition(const string& nettingSetId, const string& ctp, const string& bilateral,
                                           const string& csaCurrency, const string& index, const Real& thresholdPay,
                                           const Real& thresholdRcv, const Real& mtaPay, const Real& mtaRcv,
                                           const Real& iaHeld, const string& iaType, const string& marginCallFreq,
                                           const string& marginPostFreq, const string& mpr, const Real& collatSpreadPay,
                                           const Real& collatSpreadRcv, const vector<string>& eligCollatCcys,
                                           bool applyInitialMargin, const string& initialMarginType)
    : nettingSetId_(nettingSetId), ctp_(ctp), activeCsaFlag_(true) {

    csa_ =
        boost::make_shared<CSA>(parseCsaType(bilateral), csaCurrency, index, thresholdPay, thresholdRcv, mtaPay, mtaRcv,
                                iaHeld, iaType, parsePeriod(marginCallFreq), parsePeriod(marginPostFreq),
                                parsePeriod(mpr), collatSpreadPay, collatSpreadRcv, eligCollatCcys, applyInitialMargin,
                                parseCsaType(initialMarginType));

    validate();
    DLOG("collateralised NettingSetDefinition built... " << nettingSetId_);
}

void NettingSetDefinition::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "NettingSet");

    // Read in the mandatory nodes.
    nettingSetId_ = XMLUtils::getChildValue(node, "NettingSetId", true);
    ctp_ = XMLUtils::getChildValue(node, "Counterparty", true);
    activeCsaFlag_ = XMLUtils::getChildValueAsBool(node, "ActiveCSAFlag", true);

    // Load "CSA" information, if necessary
    if (activeCsaFlag_) {
        XMLNode* csaChild = XMLUtils::getChildNode(node, "CSADetails");
        XMLUtils::checkNode(csaChild, "CSADetails");

        string csaTypeStr = XMLUtils::getChildValue(csaChild, "Bilateral", true);
        string csaCurrency = XMLUtils::getChildValue(csaChild, "CSACurrency", true);
        string index = XMLUtils::getChildValue(csaChild, "Index", true);
        Real thresholdPay = XMLUtils::getChildValueAsDouble(csaChild, "ThresholdPay", true);
        Real thresholdRcv = XMLUtils::getChildValueAsDouble(csaChild, "ThresholdReceive", true);
        Real mtaPay = XMLUtils::getChildValueAsDouble(csaChild, "MinimumTransferAmountPay", true);
        Real mtaRcv = XMLUtils::getChildValueAsDouble(csaChild, "MinimumTransferAmountReceive", true);
        string mprStr = XMLUtils::getChildValue(csaChild, "MarginPeriodOfRisk", true);
        Real collatSpreadRcv = XMLUtils::getChildValueAsDouble(csaChild, "CollateralCompoundingSpreadReceive", true);
        Real collatSpreadPay = XMLUtils::getChildValueAsDouble(csaChild, "CollateralCompoundingSpreadPay", true);

        XMLNode* freqChild = XMLUtils::getChildNode(csaChild, "MarginingFrequency");
        XMLUtils::checkNode(freqChild, "MarginingFrequency");
        string marginCallFreqStr = XMLUtils::getChildValue(freqChild, "CallFrequency", true);
        string marginPostFreqStr = XMLUtils::getChildValue(freqChild, "PostFrequency", true);

        XMLNode* iaChild = XMLUtils::getChildNode(csaChild, "IndependentAmount");
        XMLUtils::checkNode(iaChild, "IndependentAmount");
        Real iaHeld = XMLUtils::getChildValueAsDouble(iaChild, "IndependentAmountHeld", true);
        string iaType = XMLUtils::getChildValue(iaChild, "IndependentAmountType", true);

        XMLNode* collatChild = XMLUtils::getChildNode(csaChild, "EligibleCollaterals");
        XMLUtils::checkNode(collatChild, "EligibleCollaterals");
        vector<string> eligCollatCcys = XMLUtils::getChildrenValues(collatChild, "Currencies", "Currency", true);

        bool applyInitialMargin = false;
        XMLNode* applyInitialMarginNode = XMLUtils::getChildNode(csaChild, "ApplyInitialMargin");
        if (applyInitialMarginNode)
            applyInitialMargin = XMLUtils::getChildValueAsBool(csaChild, "ApplyInitialMargin", true);

        string initialMarginType = XMLUtils::getChildValue(csaChild, "InitialMarginType", false);
        if (initialMarginType == "")
            initialMarginType = "Bilateral";

        csa_ = boost::make_shared<CSA>(parseCsaType(csaTypeStr), csaCurrency, index, thresholdPay, thresholdRcv, mtaPay,
                                       mtaRcv, iaHeld, iaType, parsePeriod(marginCallFreqStr),
                                       parsePeriod(marginPostFreqStr), parsePeriod(mprStr), collatSpreadPay,
                                       collatSpreadRcv, eligCollatCcys, applyInitialMargin,
                                       parseCsaType(initialMarginType));
    }

    validate();
}

XMLNode* NettingSetDefinition::toXML(XMLDocument& doc) {
    // Allocate a node.
    XMLNode* node = doc.allocNode("NettingSet");

    // Add the mandatory members.
    XMLUtils::addChild(doc, node, "NettingSetId", nettingSetId_);
    XMLUtils::addChild(doc, node, "Counterparty", ctp_);
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

        XMLNode* freqSubNode = doc.allocNode("MarginingFrquency");
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

        XMLUtils::addChild(doc, node, "ApplyInitialMargin", csa_->applyInitialMargin());
        XMLUtils::addChild(doc, node, "InitialMarginType", to_string(csa_->initialMarginType()));
    }

    return node;
}

void NettingSetDefinition::validate() {

    QL_REQUIRE(nettingSetId_.size() > 0, "NettingSetDefinition build error; no netting set Id");

    QL_REQUIRE(ctp_.size() > 0, "NettingSetDefinition build error; no counterparty specified for " << nettingSetId_);

    if (activeCsaFlag_) {
        QL_REQUIRE(csa_, "CSA not defined yet");
        csa_->validate(nettingSetId_);
    }
}
} // namespace data
} // namespace ore
