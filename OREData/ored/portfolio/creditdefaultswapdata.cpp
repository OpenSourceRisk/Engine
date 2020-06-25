/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <boost/lexical_cast.hpp>
#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;
using namespace QuantExt;
using std::ostream;

namespace ore {
namespace data {

CdsTier parseCdsTier(const string& s) {
    if (s == "SNRFOR") {
        return CdsTier::SNRFOR;
    } else if (s == "SUBLT2") {
        return CdsTier::SUBLT2;
    } else if (s == "SNRLAC") {
        return CdsTier::SNRLAC;
    } else if (s == "SECDOM") {
        return CdsTier::SECDOM;
    } else if (s == "JRSUBUT2") {
        return CdsTier::JRSUBUT2;
    } else if (s == "PREFT1") {
        return CdsTier::PREFT1;
    } else {
        QL_FAIL("Could not parse \"" << s << "\" to CdsTier");
    }
}

ostream& operator<<(ostream& out, const CdsTier& cdsTier) {
    switch (cdsTier) {
    case CdsTier::SNRFOR:
        return out << "SNRFOR";
    case CdsTier::SUBLT2:
        return out << "SUBLT2";
    case CdsTier::SNRLAC:
        return out << "SNRLAC";
    case CdsTier::SECDOM:
        return out << "SECDOM";
    case CdsTier::JRSUBUT2:
        return out << "JRSUBUT2";
    case CdsTier::PREFT1:
        return out << "PREFT1";
    default:
        QL_FAIL("Do not recognise CdsTier " << static_cast<int>(cdsTier));
    }
}

CdsDocClause parseCdsDocClause(const string& s) {
    if (s == "CR") {
        return CdsDocClause::CR;
    } else if (s == "MM") {
        return CdsDocClause::MM;
    } else if (s == "MR") {
        return CdsDocClause::MR;
    } else if (s == "XR") {
        return CdsDocClause::XR;
    } else if (s == "CR14") {
        return CdsDocClause::CR14;
    } else if (s == "MM14") {
        return CdsDocClause::MM14;
    } else if (s == "MR14") {
        return CdsDocClause::MR14;
    } else if (s == "XR14") {
        return CdsDocClause::XR14;
    } else {
        QL_FAIL("Could not parse \"" << s << "\" to CdsDocClause");
    }
}

ostream& operator<<(ostream& out, const CdsDocClause& cdsDocClause) {
    switch (cdsDocClause) {
    case CdsDocClause::CR:
        return out << "CR";
    case CdsDocClause::MM:
        return out << "MM";
    case CdsDocClause::MR:
        return out << "MR";
    case CdsDocClause::XR:
        return out << "XR";
    case CdsDocClause::CR14:
        return out << "CR14";
    case CdsDocClause::MM14:
        return out << "MM14";
    case CdsDocClause::MR14:
        return out << "MR14";
    case CdsDocClause::XR14:
        return out << "XR14";
    default:
        QL_FAIL("Do not recognise CdsDocClause " << static_cast<int>(cdsDocClause));
    }
}

CdsReferenceInformation::CdsReferenceInformation(const string& referenceEntityId, CdsTier tier,
                                                 const Currency& currency, CdsDocClause docClause)
    : referenceEntityId_(referenceEntityId), tier_(tier), currency_(currency), docClause_(docClause) {
    populateId();
}

void CdsReferenceInformation::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceInformation");
    referenceEntityId_ = XMLUtils::getChildValue(node, "ReferenceEntityId", true);
    tier_ = parseCdsTier(XMLUtils::getChildValue(node, "Tier", true));
    currency_ = parseCurrency(XMLUtils::getChildValue(node, "Currency", true));
    docClause_ = parseCdsDocClause(XMLUtils::getChildValue(node, "DocClause", true));
    populateId();
}

XMLNode* CdsReferenceInformation::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ReferenceInformation");
    XMLUtils::addChild(doc, node, "ReferenceEntityId", referenceEntityId_);
    XMLUtils::addChild(doc, node, "Tier", to_string(tier_));
    XMLUtils::addChild(doc, node, "Currency", currency_.code());
    XMLUtils::addChild(doc, node, "DocClause", to_string(docClause_));
    return node;
}

void CdsReferenceInformation::populateId() {
    id_ = referenceEntityId_ + "|" + to_string(tier_) + "|" + currency_.code() + "|" + to_string(docClause_);
}

bool tryParseCdsInformation(const string& strInfo, CdsReferenceInformation& cdsInfo) {

    DLOG("tryParseCdsInformation: attempting to parse " << strInfo);

    // As in documentation comment, expect strInfo of form ID|TIER|CCY|DOCCLAUSE
    vector<string> tokens;
    boost::split(tokens, strInfo, boost::is_any_of("|"));

    if (tokens.size() != 4) {
        TLOG("String " << strInfo << " not of form ID|TIER|CCY|DOCCLAUSE so parsing failed");
        return false;
    }

    CdsTier cdsTier;
    if (!tryParse<CdsTier>(tokens[1], cdsTier, &parseCdsTier)) {
        return false;
    }

    Currency ccy;
    if (!tryParse<Currency>(tokens[2], ccy, &parseCurrency)) {
        return false;
    }

    CdsDocClause cdsDocClause;
    if (!tryParse<CdsDocClause>(tokens[3], cdsDocClause, &parseCdsDocClause)) {
        return false;
    }

    cdsInfo = CdsReferenceInformation(tokens[0], cdsTier, ccy, cdsDocClause);

    return true;
}

CreditDefaultSwapData::CreditDefaultSwapData(
    const string& issuerId, const CdsReferenceInformation& referenceInformation, const LegData& leg,
    bool settlesAccrual, const QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
    const Date& protectionStart, const Date& upfrontDate, Real upfrontFee, Real recoveryRate,
    const std::string& referenceObligation)
    : issuerId_(issuerId), leg_(leg), settlesAccrual_(settlesAccrual), protectionPaymentTime_(protectionPaymentTime),
      protectionStart_(protectionStart), upfrontDate_(upfrontDate), upfrontFee_(upfrontFee),
      recoveryRate_(recoveryRate), referenceObligation_(referenceObligation),
      referenceInformation_(referenceInformation) {}

void CreditDefaultSwapData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CreditDefaultSwapData");
    issuerId_ = XMLUtils::getChildValue(node, "IssuerId", false);

    // May get an explicit CreditCurveId node. If so, we use it. Otherwise, we must have ReferenceInformation node.
    XMLNode* tmp = XMLUtils::getChildNode(node, "CreditCurveId");
    if (tmp) {
        creditCurveId_ = XMLUtils::getNodeValue(tmp);
    } else {
        tmp = XMLUtils::getChildNode(node, "ReferenceInformation");
        QL_REQUIRE(tmp, "Need either a CreditCurveId or ReferenceInformation node in CreditDefaultSwapData");
        CdsReferenceInformation ref;
        ref.fromXML(tmp);
        referenceInformation_ = ref;
        creditCurveId_ = ref.id();
    }

    settlesAccrual_ = XMLUtils::getChildValueAsBool(node, "SettlesAccrual", false); // default = Y
    protectionPaymentTime_ = CreditDefaultSwap::ProtectionPaymentTime::atDefault;   // set default
    // for backwards compatibility only
    if (auto c = XMLUtils::getChildNode(node, "PaysAtDefaultTime"))
        if (!parseBool(XMLUtils::getNodeValue(c)))
            protectionPaymentTime_ = CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
    // new node overrides deprecated one, if both should be given
    if (auto c = XMLUtils::getChildNode(node, "ProtectionPaymentTime")) {
        if (XMLUtils::getNodeValue(c) == "atDefault")
            protectionPaymentTime_ = CreditDefaultSwap::ProtectionPaymentTime::atDefault;
        else if (XMLUtils::getNodeValue(c) == "atPeriodEnd")
            protectionPaymentTime_ = CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
        else if (XMLUtils::getNodeValue(c) == "atMaturity")
            protectionPaymentTime_ = CreditDefaultSwap::ProtectionPaymentTime::atMaturity;
        else {
            QL_FAIL("protection payment time '" << XMLUtils::getNodeValue(c)
                                                << "' not known, expected atDefault, atPeriodEnd, atMaturity");
        }
    }
    tmp = XMLUtils::getChildNode(node, "ProtectionStart");
    if (tmp)
        protectionStart_ = parseDate(XMLUtils::getNodeValue(tmp)); // null date if empty or missing
    else
        protectionStart_ = Date();
    tmp = XMLUtils::getChildNode(node, "UpfrontDate");
    if (tmp)
        upfrontDate_ = parseDate(XMLUtils::getNodeValue(tmp)); // null date if empty or mssing
    else
        upfrontDate_ = Date();

    // zero if empty or missing
    upfrontFee_ = Null<Real>();
    string strUpfrontFee = XMLUtils::getChildValue(node, "UpfrontFee", false);
    if (!strUpfrontFee.empty()) {
        upfrontFee_ = parseReal(strUpfrontFee);
    }

    if (upfrontDate_ == Date()) {
        QL_REQUIRE(close_enough(upfrontFee_, 0.0) || upfrontFee_ == Null<Real>(),
                   "CreditDefaultSwapData::fromXML(): UpfronFee ("
                       << upfrontFee_ << ") must be empty or zero if no upfront date is given");
        upfrontFee_ = Null<Real>();
    }

    // Recovery rate is Null<Real>() on a standard CDS i.e. if "FixedRecoveryRate" field is not populated.
    recoveryRate_ = Null<Real>();
    string strRecoveryRate = XMLUtils::getChildValue(node, "FixedRecoveryRate", false);
    if (!strRecoveryRate.empty()) {
        recoveryRate_ = parseReal(strRecoveryRate);
    }

    leg_.fromXML(XMLUtils::getChildNode(node, "LegData"));
}

XMLNode* CreditDefaultSwapData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("CreditDefaultSwapData");
    XMLUtils::addChild(doc, node, "IssuerId", issuerId_);

    // We either have reference information or an explicit credit curve ID
    if (referenceInformation_) {
        XMLUtils::appendNode(node, referenceInformation_->toXML(doc));
    } else {
        XMLUtils::addChild(doc, node, "CreditCurveId", creditCurveId_);
    }

    XMLUtils::addChild(doc, node, "SettlesAccrual", settlesAccrual_);
    if (protectionPaymentTime_ == CreditDefaultSwap::ProtectionPaymentTime::atDefault)
        XMLUtils::addChild(doc, node, "ProtectionPaymentTime", "atDefault");
    else if (protectionPaymentTime_ == CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd)
        XMLUtils::addChild(doc, node, "ProtectionPaymentTime", "atPeriodEnd");
    else if (protectionPaymentTime_ == CreditDefaultSwap::ProtectionPaymentTime::atMaturity)
        XMLUtils::addChild(doc, node, "ProtectionPaymentTime", "atMaturity");
    else {
        QL_FAIL("CreditDefaultSwapData::toXML(): unexpected protectionPaymentTime_");
    }
    if (protectionStart_ != Date()) {
        std::ostringstream tmp;
        tmp << QuantLib::io::iso_date(protectionStart_);
        XMLUtils::addChild(doc, node, "ProtectionStart", tmp.str());
    }
    if (upfrontDate_ != Date()) {
        std::ostringstream tmp;
        tmp << QuantLib::io::iso_date(upfrontDate_);
        XMLUtils::addChild(doc, node, "UpfrontDate", tmp.str());
    }
    if (upfrontFee_ != Null<Real>())
        XMLUtils::addChild(doc, node, "UpfrontFee", upfrontFee_);

    if (recoveryRate_ != Null<Real>())
        XMLUtils::addChild(doc, node, "FixedRecoveryRate", recoveryRate_);

    XMLUtils::appendNode(node, leg_.toXML(doc));
    return node;
}

const string& CreditDefaultSwapData::creditCurveId() const {
    if (referenceInformation_) {
        return referenceInformation_->id();
    } else {
        return creditCurveId_;
    }
}

} // namespace data
} // namespace ore
