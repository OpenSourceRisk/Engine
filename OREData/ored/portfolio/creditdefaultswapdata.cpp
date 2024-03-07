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
    } else if (s == "LIEN1") {
        return CdsTier::LIEN1;
    } else if (s == "LIEN2") {
        return CdsTier::LIEN2;
    } else if (s == "LIEN3") {
        return CdsTier::LIEN3;
    }
    QL_FAIL("Could not parse \"" << s << "\" to CdsTier");
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
    case CdsTier::LIEN1:
        return out << "LIEN1";
    case CdsTier::LIEN2:
        return out << "LIEN2";
    case CdsTier::LIEN3:
        return out << "LIEN3";
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

// TODO refactor to creditevents.cpp
IsdaRulesDefinitions parseIsdaRulesDefinitions(const string& s) {
    if (s == "2003") {
        return IsdaRulesDefinitions::y2003;
    } else if (s == "2014") {
        return IsdaRulesDefinitions::y2014;
    } else {
        QL_FAIL("Could not parse \"" << s << "\" to isdaRulesDefinitions");
    }
}

ostream& operator<<(ostream& out, const IsdaRulesDefinitions& isdaRulesDefinitions) {
    switch (isdaRulesDefinitions) {
    case IsdaRulesDefinitions::y2003:
        return out << "2003";
    case IsdaRulesDefinitions::y2014:
        return out << "2014";
    default:
        QL_FAIL("Do not recognise IsdaRulesDefinitions " << static_cast<int>(isdaRulesDefinitions));
    }
}

IsdaRulesDefinitions isdaRulesDefinitionsFromDocClause(const CdsDocClause& cdsDocClause) {
    switch (cdsDocClause) {
    case CdsDocClause::CR:
    case CdsDocClause::MR:
    case CdsDocClause::XR:
    case CdsDocClause::MM:
        return IsdaRulesDefinitions::y2003;
    case CdsDocClause::CR14:
    case CdsDocClause::MR14:
    case CdsDocClause::XR14:
    case CdsDocClause::MM14:
    default:
        return IsdaRulesDefinitions::y2014;
    }
}

CreditEventType parseCreditEventType(const string& s) {
    if (s == "BANKRUPTCY") {
        return CreditEventType::BANKRUPTCY;
    } else if (s == "FAILURE TO PAY") {
        return CreditEventType::FAILURE_TO_PAY;
    } else if (s == "RESTRUCTURING") {
        return CreditEventType::RESTRUCTURING;
    } else if (s == "OBLIGATION ACCELERATION") {
        return CreditEventType::OBLIGATION_ACCELERATION;
    } else if (s == "OBLIGATION DEFAULT") {
        return CreditEventType::OBLIGATION_DEFAULT;
    } else if (s == "REPUDIATION/MORATORIUM") {
        return CreditEventType::REPUDIATION_MORATORIUM;
    } else if (s == "GOVERNMENTAL INTERVENTION") {
        return CreditEventType::GOVERNMENTAL_INTERVENTION;
    } else {
        QL_FAIL("Could not parse \"" << s << "\" to a credit event.");
    }
}

ostream& operator<<(ostream& out, const CreditEventType& creditEventType) {
    switch (creditEventType) {
    case CreditEventType::BANKRUPTCY:
        return out << "BANKRUPTCY";
    case CreditEventType::FAILURE_TO_PAY:
        return out << "FAILURE TO PAY";
    case CreditEventType::RESTRUCTURING:
        return out << "RESTRUCTURING";
    case CreditEventType::OBLIGATION_ACCELERATION:
        return out << "OBLIGATION ACCELERATION";
    case CreditEventType::OBLIGATION_DEFAULT:
        return out << "OBLIGATION DEFAULT";
    case CreditEventType::REPUDIATION_MORATORIUM:
        return out << "REPUDIATION/MORATORIUM";
    case CreditEventType::GOVERNMENTAL_INTERVENTION:
        return out << "GOVERNMENTAL INTERVENTION";
    default:
        QL_FAIL("Do not recognise CreditEventType " << static_cast<int>(creditEventType));
    }
}

bool isTriggeredDocClause(CdsDocClause contractDocClause, CreditEventType creditEventType) {
    switch (creditEventType) {
    case ore::data::CreditEventType::BANKRUPTCY:
    case ore::data::CreditEventType::FAILURE_TO_PAY:
    case ore::data::CreditEventType::REPUDIATION_MORATORIUM:
        // all of the above include a failure to pay
        switch (contractDocClause) {
        case ore::data::CdsDocClause::CR:
        case ore::data::CdsDocClause::CR14:
        case ore::data::CdsDocClause::MM:
        case ore::data::CdsDocClause::MM14:
        case ore::data::CdsDocClause::MR:
        case ore::data::CdsDocClause::MR14:
        case ore::data::CdsDocClause::XR:
        case ore::data::CdsDocClause::XR14:
            return true;
        default:
            break;
        }
        break;
    case ore::data::CreditEventType::GOVERNMENTAL_INTERVENTION:
        // typically includes a conversion to shares with a write down
    case ore::data::CreditEventType::RESTRUCTURING:
        // it depends on the type of restructure!
        switch (contractDocClause) {
        case ore::data::CdsDocClause::CR:
        case ore::data::CdsDocClause::CR14:
        case ore::data::CdsDocClause::MM:
        case ore::data::CdsDocClause::MM14:
        case ore::data::CdsDocClause::MR:
        case ore::data::CdsDocClause::MR14:
            return true;
        case ore::data::CdsDocClause::XR:
        case ore::data::CdsDocClause::XR14:
            return false;
        default:
            break;
        }
    case ore::data::CreditEventType::OBLIGATION_ACCELERATION:
    case ore::data::CreditEventType::OBLIGATION_DEFAULT:
        // not necessarily a default itself, no examples in record.
        return false;
        break;
    default:
        break;
    }
    QL_FAIL("Could not recognize CreditEventType "
            << static_cast<int>(creditEventType) << " or CdsDocClause " << static_cast<int>(contractDocClause)
            << " when identifying whether a doc clause is triggrered for a given credit event type.");
    return false;
}

CreditEventTiers parseCreditEventTiers(const string& s) {
    if (s == "SNR") {
        return CreditEventTiers::SNR;
    } else if (s == "SUB") {
        return CreditEventTiers::SUB;
    } else if (s == "SNRLAC") {
        return CreditEventTiers::SNRLAC;
    } else if (s == "SNR/SUB") {
        return CreditEventTiers::SNR_SUB;
    } else if (s == "SNR/SNRLAC") {
        return CreditEventTiers::SNR_SNRLAC;
    } else if (s == "SUB/SNRLAC") {
        return CreditEventTiers::SUB_SNRLAC;
    } else if (s == "SNR/SUB/SNRLAC") {
        return CreditEventTiers::SNR_SUB_SNRLAC;
    } else {
        QL_FAIL("Could not parse \"" << s << "\" to a credit event tiers set.");
    }
}

ostream& operator<<(ostream& out, const CreditEventTiers& creditEventTiers) {
    switch (creditEventTiers) {
    case CreditEventTiers::SNR:
        return out << "SNR";
    case CreditEventTiers::SUB:
        return out << "SUB";
    case CreditEventTiers::SNRLAC:
        return out << "SNRLAC";
    case CreditEventTiers::SNR_SUB:
        return out << "SNR/SUB";
    case CreditEventTiers::SNR_SNRLAC:
        return out << "SNR/SNRLAC";
    case CreditEventTiers::SUB_SNRLAC:
        return out << "SUB/SNRLAC";
    case CreditEventTiers::SNR_SUB_SNRLAC:
        return out << "SNR/SUB/SNRLAC";
    default:
        QL_FAIL("Do not recognise CreditEventTiers " << static_cast<int>(creditEventTiers));
    }
}

bool isAuctionedSeniority(CdsTier contractTier, CreditEventTiers creditEventTiers) {
    switch (creditEventTiers) {
    case ore::data::CreditEventTiers::SNR:
        switch (contractTier) {
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
            return true;
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
        case ore::data::CdsTier::SNRLAC:
            return false;
        default:
            break;
        }
        break;
    case ore::data::CreditEventTiers::SUB:
        switch (contractTier) {
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
            return true;
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
        case ore::data::CdsTier::SNRLAC:
            return false;
        default:
            break;
        }
        break;
    case ore::data::CreditEventTiers::SNRLAC:
        switch (contractTier) {
        case ore::data::CdsTier::SNRLAC:
            return true;
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
            return false;
        default:
            break;
        }
        break;
    case ore::data::CreditEventTiers::SNR_SUB:
        switch (contractTier) {
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
            return true;
        case ore::data::CdsTier::SNRLAC:
            return false;
        default:
            break;
        }
        break;
    case ore::data::CreditEventTiers::SNR_SNRLAC:
        switch (contractTier) {
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
        case ore::data::CdsTier::SNRLAC:
            return true;
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
            return false;
        default:
            break;
        }
        break;
    case ore::data::CreditEventTiers::SUB_SNRLAC:
        switch (contractTier) {
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
        case ore::data::CdsTier::SNRLAC:
            return true;
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
            return false;
        default:
            break;
        }
        break;
    case ore::data::CreditEventTiers::SNR_SUB_SNRLAC:
        switch (contractTier) {
        case ore::data::CdsTier::SUBLT2:
        case ore::data::CdsTier::JRSUBUT2:
        case ore::data::CdsTier::SNRLAC:
        case ore::data::CdsTier::SNRFOR:
        case ore::data::CdsTier::SECDOM:
        case ore::data::CdsTier::PREFT1:
            return true;
        default:
            break;
        }
        break;
    default:
        break;
    }
    QL_FAIL("Could not recognize CreditEventTiers "
            << static_cast<int>(creditEventTiers) << " or CdsTier " << static_cast<int>(contractTier)
            << " when identifying the applicability if an event for a given contract tier.");
    return false;
}
// end TODO refactor to creditevents.cpp

CdsReferenceInformation::CdsReferenceInformation() : tier_(CdsTier::SNRFOR), docClause_(boost::none) {}

CdsReferenceInformation::CdsReferenceInformation(const string& referenceEntityId, CdsTier tier,
                                                 const Currency& currency, boost::optional<CdsDocClause> docClause)
    : referenceEntityId_(referenceEntityId), tier_(tier), currency_(currency), docClause_(docClause) {
    populateId();
}

void CdsReferenceInformation::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceInformation");
    referenceEntityId_ = XMLUtils::getChildValue(node, "ReferenceEntityId", true);
    tier_ = parseCdsTier(XMLUtils::getChildValue(node, "Tier", true));
    currency_ = parseCurrency(XMLUtils::getChildValue(node, "Currency", true));
    if (auto s = XMLUtils::getChildValue(node, "DocClause", false); !s.empty()) {
        docClause_ = parseCdsDocClause(s);
    }
    populateId();
}

XMLNode* CdsReferenceInformation::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ReferenceInformation");
    XMLUtils::addChild(doc, node, "ReferenceEntityId", referenceEntityId_);
    XMLUtils::addChild(doc, node, "Tier", to_string(tier_));
    XMLUtils::addChild(doc, node, "Currency", currency_.code());
    if(docClause_)
        XMLUtils::addChild(doc, node, "DocClause", to_string(*docClause_));
    return node;
}

void CdsReferenceInformation::populateId() {
    id_ = referenceEntityId_ + "|" + to_string(tier_) + "|" + currency_.code();
    if (docClause_)
        id_ += "|" + to_string(*docClause_);
}

CdsDocClause CdsReferenceInformation::docClause() const {
    QL_REQUIRE(docClause_, "CdsReferenceInforamtion::docClause(): docClause not set.");
    return *docClause_;
}

bool CdsReferenceInformation::hasDocClause() const { return docClause_ != boost::none; }

bool tryParseCdsInformation(string strInfo, CdsReferenceInformation& cdsInfo) {

    DLOG("tryParseCdsInformation: attempting to parse " << strInfo);

    // As in documentation comment, expect strInfo of form ID|TIER|CCY(|DOCCLAUSE)
    vector<string> tokens;
    boost::split(tokens, strInfo, boost::is_any_of("|"));

    if (tokens.size() != 4 && tokens.size() != 3) {
        TLOG("String " << strInfo << " not of form ID|TIER|CCY(|DOCCLAUSE) so parsing failed");
        return false;
    }

    CdsTier cdsTier;
    if (!tryParse<CdsTier>(tokens[1], cdsTier, &parseCdsTier)) {
        return false;
    }

    Currency ccy;
    if (!tryParseCurrency(tokens[2], ccy)) {
        return false;
    }

    boost::optional<CdsDocClause> cdsDocClause;
    if (tokens.size() == 4) {
        CdsDocClause tmp;
        if (!tryParse<CdsDocClause>(tokens[3], tmp, &parseCdsDocClause)) {
            return false;
        }
        cdsDocClause = tmp;
    }

    cdsInfo = CdsReferenceInformation(tokens[0], cdsTier, ccy, cdsDocClause);

    return true;
}

CreditDefaultSwapData::CreditDefaultSwapData()
    : settlesAccrual_(true), protectionPaymentTime_(QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault),
      upfrontFee_(Null<Real>()), rebatesAccrual_(true), recoveryRate_(Null<Real>()), cashSettlementDays_(3) {}

CreditDefaultSwapData::CreditDefaultSwapData(const string& issuerId, const string& creditCurveId, const LegData& leg,
    const bool settlesAccrual, const PPT protectionPaymentTime,
    const Date& protectionStart, const Date& upfrontDate, const Real upfrontFee, Real recoveryRate,
    const string& referenceObligation, const Date& tradeDate, const string& cashSettlementDays,
    const bool rebatesAccrual)
    : issuerId_(issuerId), creditCurveId_(creditCurveId), leg_(leg), settlesAccrual_(settlesAccrual),
      protectionPaymentTime_(protectionPaymentTime), protectionStart_(protectionStart), upfrontDate_(upfrontDate),
      upfrontFee_(upfrontFee), rebatesAccrual_(rebatesAccrual), recoveryRate_(recoveryRate),
      referenceObligation_(referenceObligation),
      tradeDate_(tradeDate), strCashSettlementDays_(cashSettlementDays),
      cashSettlementDays_(strCashSettlementDays_.empty() ? 3 : parseInteger(strCashSettlementDays_)) {}

CreditDefaultSwapData::CreditDefaultSwapData(
    const string& issuerId, const CdsReferenceInformation& referenceInformation, const LegData& leg,
    bool settlesAccrual, const PPT protectionPaymentTime,
    const Date& protectionStart, const Date& upfrontDate, Real upfrontFee, Real recoveryRate,
    const std::string& referenceObligation, const Date& tradeDate, const string& cashSettlementDays,
    const bool rebatesAccrual)
    : issuerId_(issuerId), leg_(leg), settlesAccrual_(settlesAccrual), protectionPaymentTime_(protectionPaymentTime),
      protectionStart_(protectionStart), upfrontDate_(upfrontDate), upfrontFee_(upfrontFee), rebatesAccrual_(rebatesAccrual),
      recoveryRate_(recoveryRate), referenceObligation_(referenceObligation),
      tradeDate_(tradeDate), strCashSettlementDays_(cashSettlementDays),
      cashSettlementDays_(strCashSettlementDays_.empty() ? 3 : parseInteger(strCashSettlementDays_)),
      referenceInformation_(referenceInformation) {}

void CreditDefaultSwapData::fromXML(XMLNode* node) {

    check(node);

    issuerId_ = XMLUtils::getChildValue(node, "IssuerId", false);

    // May get an explicit CreditCurveId node. If so, we use it. Otherwise, we must have ReferenceInformation node.
    // XMLNode* tmp = XMLUtils::getChildNode(node, "CreditCurveId");
    if (auto tmp = XMLUtils::getChildNode(node, "CreditCurveId")) {
        creditCurveId_ = XMLUtils::getNodeValue(tmp);
        CdsReferenceInformation ref;
        if (tryParseCdsInformation(creditCurveId_, ref))
            referenceInformation_ = ref;
    } else {
        tmp = XMLUtils::getChildNode(node, "ReferenceInformation");
        QL_REQUIRE(tmp, "Need either a CreditCurveId or ReferenceInformation node in CreditDefaultSwapData");
        CdsReferenceInformation ref;
        ref.fromXML(tmp);
        referenceInformation_ = ref;
        creditCurveId_ = ref.id();
    }

    settlesAccrual_ = XMLUtils::getChildValueAsBool(node, "SettlesAccrual", false);
    rebatesAccrual_ = XMLUtils::getChildValueAsBool(node, "RebatesAccrual", false);
    
    protectionPaymentTime_ = PPT::atDefault;

    // for backwards compatibility only
    if (auto c = XMLUtils::getChildNode(node, "PaysAtDefaultTime"))
        if (!parseBool(XMLUtils::getNodeValue(c)))
            protectionPaymentTime_ = PPT::atPeriodEnd;

    // new node overrides deprecated one, if both should be given
    if (auto c = XMLUtils::getChildNode(node, "ProtectionPaymentTime")) {
        if (XMLUtils::getNodeValue(c) == "atDefault")
            protectionPaymentTime_ = PPT::atDefault;
        else if (XMLUtils::getNodeValue(c) == "atPeriodEnd")
            protectionPaymentTime_ = PPT::atPeriodEnd;
        else if (XMLUtils::getNodeValue(c) == "atMaturity")
            protectionPaymentTime_ = PPT::atMaturity;
        else {
            QL_FAIL("protection payment time '" << XMLUtils::getNodeValue(c) << 
                "' not known, expected atDefault, atPeriodEnd, atMaturity");
        }
    }

    protectionStart_ = Date();
    if (auto tmp = XMLUtils::getChildNode(node, "ProtectionStart"))
        protectionStart_ = parseDate(XMLUtils::getNodeValue(tmp));

    upfrontDate_ = Date();
    if (auto tmp = XMLUtils::getChildNode(node, "UpfrontDate"))
        upfrontDate_ = parseDate(XMLUtils::getNodeValue(tmp));

    // zero if empty or missing
    upfrontFee_ = Null<Real>();
    string strUpfrontFee = XMLUtils::getChildValue(node, "UpfrontFee", false);
    if (!strUpfrontFee.empty()) {
        upfrontFee_ = parseReal(strUpfrontFee);
    }

    if (upfrontDate_ == Date()) {
        QL_REQUIRE(close_enough(upfrontFee_, 0.0) || upfrontFee_ == Null<Real>(),
                   "fromXML(): UpfronFee (" << upfrontFee_ << ") must be empty or zero if no upfront date is given");
        upfrontFee_ = Null<Real>();
    }

    // Recovery rate is Null<Real>() on a standard CDS i.e. if "FixedRecoveryRate" field is not populated.
    recoveryRate_ = Null<Real>();
    string strRecoveryRate = XMLUtils::getChildValue(node, "FixedRecoveryRate", false);
    if (!strRecoveryRate.empty()) {
        recoveryRate_ = parseReal(strRecoveryRate);
    }

    tradeDate_ = Date();
    if (auto n = XMLUtils::getChildNode(node, "TradeDate"))
        tradeDate_ = parseDate(XMLUtils::getNodeValue(n));

    strCashSettlementDays_ = XMLUtils::getChildValue(node, "CashSettlementDays", false);
    cashSettlementDays_ = strCashSettlementDays_.empty() ? 3 : parseInteger(strCashSettlementDays_);

    leg_.fromXML(XMLUtils::getChildNode(node, "LegData"));
}

XMLNode* CreditDefaultSwapData::toXML(XMLDocument& doc) const {

    XMLNode* node = alloc(doc);

    XMLUtils::addChild(doc, node, "IssuerId", issuerId_);

    // We either have reference information or an explicit credit curve ID
    if (referenceInformation_) {
        XMLUtils::appendNode(node, referenceInformation_->toXML(doc));
    } else {
        XMLUtils::addChild(doc, node, "CreditCurveId", creditCurveId_);
    }

    XMLUtils::addChild(doc, node, "SettlesAccrual", settlesAccrual_);
    if (!rebatesAccrual_)
        XMLUtils::addChild(doc, node, "RebatesAccrual", rebatesAccrual_);
    if (protectionPaymentTime_ == PPT::atDefault) {
        XMLUtils::addChild(doc, node, "ProtectionPaymentTime", "atDefault");
    } else if (protectionPaymentTime_ == PPT::atPeriodEnd) {
        XMLUtils::addChild(doc, node, "ProtectionPaymentTime", "atPeriodEnd");
    } else if (protectionPaymentTime_ == PPT::atMaturity) {
        XMLUtils::addChild(doc, node, "ProtectionPaymentTime", "atMaturity");
    } else {
        QL_FAIL("toXML(): unexpected ProtectionPaymentTime");
    }

    if (protectionStart_ != Date()) {
        XMLUtils::addChild(doc, node, "ProtectionStart", to_string(protectionStart_));
    }

    if (upfrontDate_ != Date()) {
        XMLUtils::addChild(doc, node, "UpfrontDate", to_string(upfrontDate_));
    }

    if (upfrontFee_ != Null<Real>())
        XMLUtils::addChild(doc, node, "UpfrontFee", upfrontFee_);

    if (recoveryRate_ != Null<Real>())
        XMLUtils::addChild(doc, node, "FixedRecoveryRate", recoveryRate_);

    if (tradeDate_ != Date()) {
        XMLUtils::addChild(doc, node, "TradeDate", to_string(tradeDate_));
    }

    if (!strCashSettlementDays_.empty()) {
        XMLUtils::addChild(doc, node, "CashSettlementDays", strCashSettlementDays_);
    }

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

void CreditDefaultSwapData::check(XMLNode* node) const {
    XMLUtils::checkNode(node, "CreditDefaultSwapData");
}

XMLNode* CreditDefaultSwapData::alloc(XMLDocument& doc) const {
    return doc.allocNode("CreditDefaultSwapData");
}

} // namespace data
} // namespace ore
