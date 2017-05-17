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
#include <ql/utilities/dataparsers.hpp>

namespace ore {
namespace data {

NettingSetDefinition::NettingSetDefinition(XMLNode* node) : isLoaded_(false), isBuilt_(false) {
    fromXML(node);
    build();
    DLOG("NettingSetDefinition built from XML... " << nettingSetId_);
}

NettingSetDefinition::NettingSetDefinition(const string& nettingSetId, const string& ctp)
    : nettingSetId_(nettingSetId), ctp_(ctp), activeCsaFlag_(false) {
    isLoaded_ = true;
    isBuilt_ = false;
    build();
    DLOG("uncollateralised NettingSetDefinition built... " << nettingSetId_);
}

NettingSetDefinition::NettingSetDefinition(const string& nettingSetId, const string& ctp, const string& bilateral,
                                           const string& csaCurrency, const string& index, const Real& thresholdPay,
                                           const Real& thresholdRcv, const Real& mtaPay, const Real& mtaRcv,
                                           const Real& iaHeld, const string& iaType, const string& marginCallFreq,
                                           const string& marginPostFreq, const string& mpr, const Real& collatSpreadPay,
                                           const Real& collatSpreadRcv, const vector<string>& eligCollatCcys)
    : nettingSetId_(nettingSetId), ctp_(ctp), activeCsaFlag_(true), csaTypeStr_(bilateral), csaCurrency_(csaCurrency),
      index_(index), thresholdPay_(thresholdPay), thresholdRcv_(thresholdRcv), mtaPay_(mtaPay), mtaRcv_(mtaRcv),
      iaHeld_(iaHeld), iaType_(iaType), marginCallFreqStr_(marginCallFreq), marginPostFreqStr_(marginPostFreq),
      mprStr_(mpr), collatSpreadPay_(collatSpreadPay), collatSpreadRcv_(collatSpreadRcv),
      eligCollatCcys_(eligCollatCcys) {

    QL_REQUIRE(activeCsaFlag_,
               "NettingSetDefinition construction error... " << nettingSetId_ << "; this constructor is intended for "
                                                             << "collateralised netting sets, or uncollateralised sets "
                                                             << "use alternative constructor.");

    isLoaded_ = true;
    isBuilt_ = false; // gets overwritten after invocation of build()
    build();
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

        csaTypeStr_ = XMLUtils::getChildValue(csaChild, "Bilateral", true);
        csaCurrency_ = XMLUtils::getChildValue(csaChild, "CSACurrency", true);
        index_ = XMLUtils::getChildValue(csaChild, "Index", true);
        thresholdPay_ = XMLUtils::getChildValueAsDouble(csaChild, "ThresholdPay", true);
        thresholdRcv_ = XMLUtils::getChildValueAsDouble(csaChild, "ThresholdReceive", true);
        mtaPay_ = XMLUtils::getChildValueAsDouble(csaChild, "MinimumTransferAmountPay", true);
        mtaRcv_ = XMLUtils::getChildValueAsDouble(csaChild, "MinimumTransferAmountReceive", true);
        mprStr_ = XMLUtils::getChildValue(csaChild, "MarginPeriodOfRisk", true);
        collatSpreadRcv_ = XMLUtils::getChildValueAsDouble(csaChild, "CollateralCompoundingSpreadReceive", true);
        collatSpreadPay_ = XMLUtils::getChildValueAsDouble(csaChild, "CollateralCompoundingSpreadPay", true);

        XMLNode* freqChild = XMLUtils::getChildNode(csaChild, "MarginingFrequency");
        XMLUtils::checkNode(freqChild, "MarginingFrequency");
        marginCallFreqStr_ = XMLUtils::getChildValue(freqChild, "CallFrequency", true);
        marginPostFreqStr_ = XMLUtils::getChildValue(freqChild, "PostFrequency", true);

        XMLNode* iaChild = XMLUtils::getChildNode(csaChild, "IndependentAmount");
        XMLUtils::checkNode(iaChild, "IndependentAmount");
        iaHeld_ = XMLUtils::getChildValueAsDouble(iaChild, "IndependentAmountHeld", true);
        iaType_ = XMLUtils::getChildValue(iaChild, "IndependentAmountType", true);

        XMLNode* collatChild = XMLUtils::getChildNode(csaChild, "EligibleCollaterals");
        XMLUtils::checkNode(collatChild, "EligibleCollaterals");
        eligCollatCcys_ = XMLUtils::getChildrenValues(collatChild, "Currencies", "Currency", true);
    }
    isLoaded_ = true;
}

XMLNode* NettingSetDefinition::toXML(XMLDocument& doc) {
    // Allocate a node.
    XMLNode* node = doc.allocNode("NettingSet");

    // Add the mandatory members.
    XMLUtils::addChild(doc, node, "NettingSetId", nettingSetId_);
    XMLUtils::addChild(doc, node, "Counterparty_", ctp_);
    XMLUtils::addChild(doc, node, "ActiveCSAFlag", activeCsaFlag_);

    XMLNode* csaSubNode = doc.allocNode("CSADetails");
    XMLUtils::appendNode(node, csaSubNode);
    if (activeCsaFlag_) {
        XMLUtils::addChild(doc, csaSubNode, "Bilateral", csaTypeStr_);
        XMLUtils::addChild(doc, csaSubNode, "CSACurrency", csaCurrency_);
        XMLUtils::addChild(doc, csaSubNode, "ThresholdPay", thresholdPay_);
        XMLUtils::addChild(doc, csaSubNode, "ThresholdReceive", thresholdRcv_);
        XMLUtils::addChild(doc, csaSubNode, "MinimumTransferAmountPay", mtaPay_);
        XMLUtils::addChild(doc, csaSubNode, "MinimumTransferAmountReceive", mtaRcv_);
        XMLUtils::addChild(doc, csaSubNode, "MarginPeriodOfRisk", mprStr_);
        XMLUtils::addChild(doc, csaSubNode, "CollateralCompoundingSpreadPay", collatSpreadPay_);
        XMLUtils::addChild(doc, csaSubNode, "CollateralCompoundingSpreadReceive", collatSpreadRcv_);

        XMLNode* freqSubNode = doc.allocNode("MarginingFrquency");
        XMLUtils::appendNode(csaSubNode, freqSubNode);
        XMLUtils::addChild(doc, freqSubNode, "CallFrequency", marginCallFreqStr_);
        XMLUtils::addChild(doc, freqSubNode, "PostFrequency", marginPostFreqStr_);

        XMLNode* iaSubNode = doc.allocNode("IndependentAmount");
        XMLUtils::appendNode(csaSubNode, iaSubNode);
        XMLUtils::addChild(doc, iaSubNode, "IndependentAmountHeld", iaHeld_);
        XMLUtils::addChild(doc, iaSubNode, "IndependentAmountType", iaType_);

        XMLNode* collatSubNode = doc.allocNode("EligibleCollaterals");
        XMLUtils::appendNode(csaSubNode, collatSubNode);
        XMLUtils::addChildren(doc, collatSubNode, "Currencies", "Currency", eligCollatCcys_);
    }
    return node;
}

void NettingSetDefinition::build() {

    QL_REQUIRE(isLoaded_, "NettingSetDefinition build error; object data not correctly loaded" << nettingSetId_);

    QL_REQUIRE(!isBuilt_, "NettingSetDefinition build error; already built " << nettingSetId_);

    QL_REQUIRE(nettingSetId_.size() > 0, "NettingSetDefinition build error; no netting set Id");

    QL_REQUIRE(ctp_.size() > 0, "NettingSetDefinition build error; no counterparty specified for " << nettingSetId_);

    if (activeCsaFlag_) {
        QL_REQUIRE(csaTypeStr_.size() > 0, "NettingSetDefinition build error; csa-type not defined");
        if (csaTypeStr_ == "Bilateral")
            csaType_ = Bilateral;
        else if (csaTypeStr_ == "CallOnly")
            csaType_ = CallOnly;
        else if (csaTypeStr_ == "PostOnly")
            csaType_ = PostOnly;
        else
            QL_FAIL("NettingSetDefinition build error;"
                    << " unsupported csa-type");

        QL_REQUIRE(csaCurrency_.size() == 3,
                   "NettingSetDefinition build error;"
                       << " csa currency should be a three-letter ISO code");

        QL_REQUIRE(thresholdPay_ >= 0, "NettingSetDefinition build error; negative thresholdPay");
        QL_REQUIRE(thresholdRcv_ >= 0, "NettingSetDefinition build error; negative thresholdRcv");
        QL_REQUIRE(mtaPay_ >= 0, "NettingSetDefinition build error; negative mtaPay");
        QL_REQUIRE(mtaRcv_ >= 0, "NettingSetDefinition build error; negative mtaRcv");
        QL_REQUIRE(iaType_ == "FIXED",
                   "NettingSetDefinition build error;"
                       << " unsupported independent amount type; " << iaType_);

        // assumption - input defined as string (e.g. 10d, 3W, 6M, 4Y)
        marginCallFreq_ = QuantLib::PeriodParser::parse(marginCallFreqStr_);
        marginPostFreq_ = QuantLib::PeriodParser::parse(marginPostFreqStr_);
        mpr_ = QuantLib::PeriodParser::parse(mprStr_);

        QL_REQUIRE(marginCallFreq_ > Period(0, Days) && marginPostFreq_ > Period(0, Days),
                   "NettingSetDefinition build error;"
                       << " non-positive margining frequency");
        QL_REQUIRE(mpr_ >= Period(0, Days),
                   "NettingSetDefinition build error;"
                       << " negative margin period of risk");
        if (mpr_ < marginCallFreq_ || mpr_ < marginPostFreq_) {
            LOG("NettingSet " << nettingSetId_ << " has CSA margining frequency (" << marginCallFreq_ << ", "
                              << marginPostFreq_ << ") longer than assumed margin period of risk " << mpr_);
        }

        for (unsigned i = 0; i < eligCollatCcys_.size(); i++) {
            QL_REQUIRE(eligCollatCcys_[i].size() == 3,
                       "NettingSetDefinition build error;"
                           << " three-letter ISO code expected");
        }

        // unilateral CSA - set threshold near infinity to disable margining
        switch (csaType_) {
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

    isBuilt_ = true;
}
}
}
