/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_credit_i
#define ored_portfolio_credit_i

%{
using ore::data::CdsTier;
using ore::data::CdsDocClause;
using ore::data::CdsReferenceInformation;
using ore::data::CreditDefaultSwapData;
using ORECreditDefaultSwap = ore::data::CreditDefaultSwap;
using ore::data::BasketConstituent;
using ore::data::BasketData;
using ore::data::IndexCreditDefaultSwapData;
using ore::data::SyntheticCDO;
using AuctionSettlementInformation = ore::data::CreditDefaultSwapOption::AuctionSettlementInformation;
%}

%extend CreditDefaultSwap {
    enum ProtectionPaymentTime {
        atDefault,
        atPeriodEnd,
        atMaturity
    };
}

// ore/OREData/ored/portfolio/creditdefaultswapdata.hpp

enum class CdsTier { SNRFOR, SUBLT2, SNRLAC, SECDOM, JRSUBUT2, PREFT1, LIEN1, LIEN2, LIEN3 };

enum class CdsDocClause { CR, MM, MR, XR, CR14, MM14, MR14, XR14 };

%shared_ptr(CdsReferenceInformation)
class CdsReferenceInformation : public XMLSerializable {
public:
    CdsReferenceInformation();
    CdsReferenceInformation(const std::string& referenceEntityId, CdsTier tier,
                            const QuantLib::Currency& currency,
                            QuantLib::ext::optional<CdsDocClause> docClause = QuantLib::ext::nullopt);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    const std::string& referenceEntityId() const;
    CdsTier tier() const;
    const QuantLib::Currency& currency() const;
    bool hasDocClause() const;
    CdsDocClause docClause() const;
    const std::string& id() const;
};

%shared_ptr(CreditDefaultSwapData)
class CreditDefaultSwapData : public XMLSerializable {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    CreditDefaultSwapData(const std::string& issuerId, const std::string& creditCurveId, const LegData& leg,
                          const bool settlesAccrual = true,
                          const PPT protectionPaymentTime = PPT::atDefault,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
                          const bool rebatesAccrual = true);
    CreditDefaultSwapData(const std::string& issuerId, const CdsReferenceInformation& referenceInformation,
                          const LegData& leg, const bool settlesAccrual = true,
                          const PPT protectionPaymentTime = PPT::atDefault,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
                          const bool rebatesAccrual = true);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ORECreditDefaultSwap)
class ORECreditDefaultSwap : public Trade {
public:
    ORECreditDefaultSwap(const Envelope& env, const CreditDefaultSwapData& swap);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(BasketConstituent)
class BasketConstituent : public XMLSerializable {
public:
    BasketConstituent(const std::string& issuerName, const std::string& creditCurveId, QuantLib::Real notional,
                      const std::string& currency, const std::string& qualifier,
                      QuantLib::Real priorNotional = QuantLib::Null<QuantLib::Real>(),
                      QuantLib::Real recovery = QuantLib::Null<QuantLib::Real>(),
                      const QuantLib::Date& auctionDate = QuantLib::Date(),
                      const QuantLib::Date& auctionSettlementDate = QuantLib::Date(),
                      const QuantLib::Date& defaultDate = QuantLib::Date(),
                      const QuantLib::Date& eventDeterminationDate = QuantLib::Date());
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%template(BasketConstituentVector) std::vector<ext::shared_ptr<BasketConstituent>>;

%shared_ptr(BasketData)
class BasketData : public XMLSerializable {
public:
    BasketData(const std::vector<BasketConstituent>& constituents);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend BasketData {
    BasketData(const std::vector<ext::shared_ptr<BasketConstituent>>& constituents) {
        return new BasketData(VECTOR_SWIG_TO_ORE(constituents));
    }
}

%shared_ptr(IndexCreditDefaultSwapData)
class IndexCreditDefaultSwapData : public CreditDefaultSwapData {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    IndexCreditDefaultSwapData(const std::string& creditCurveId,
        const BasketData& basket,
        const LegData& leg,
        const bool settlesAccrual = true,
        const PPT protectionPaymentTime = PPT::atDefault,
        const QuantLib::Date& protectionStart = QuantLib::Date(),
        const QuantLib::Date& upfrontDate = QuantLib::Date(),
        const QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::Date& tradeDate = QuantLib::Date(),
        const std::string& cashSettlementDays = "",
        const bool rebatesAccrual = true);
};

%shared_ptr(SyntheticCDO)
class SyntheticCDO : public Trade {
public:
    SyntheticCDO(const Envelope& env, const LegData& leg, const std::string& qualifier, const BasketData& basketData,
                 double attachmentPoint, double detachmentPoint, const bool settlesAccrual = true,
                 const QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                     QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                 const std::string& protectionStart = std::string(), const std::string& upfrontDate = std::string(),
                 const Real upfrontFee = Null<Real>(), const bool rebatesAccrual = true,
                 Real recoveryRate = Null<Real>());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// Additional credit trade types from ored_portfolio2.i

%{
using ore::data::CreditDefaultSwapOption;
using ore::data::CreditLinkedSwap;
using ore::data::IndexCreditDefaultSwap;
using ore::data::IndexCreditDefaultSwapOption;
%}

// ore/OREData/ored/portfolio/creditdefaultswapoption.hpp

// Expose AuctionSettlementInformation as a top-level class (inner class of CreditDefaultSwapOption)
%shared_ptr(AuctionSettlementInformation)
class AuctionSettlementInformation : public XMLSerializable {
public:
    AuctionSettlementInformation();
    AuctionSettlementInformation(const QuantLib::Date& auctionSettlementDate,
                                 QuantLib::Real auctionFinalPrice);
    const QuantLib::Date& auctionSettlementDate() const;
    QuantLib::Real auctionFinalPrice() const;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(CreditDefaultSwapOption)
class CreditDefaultSwapOption : public Trade {
public:
    CreditDefaultSwapOption();
    CreditDefaultSwapOption(const Envelope& env, const OptionData& option, const CreditDefaultSwapData& swap,
                            QuantLib::Real strike = QuantLib::Null<QuantLib::Real>(),
                            const std::string& strikeType = "Spread",
                            bool knockOut = true, const std::string& term = "");
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/creditlinkedswap.hpp

%shared_ptr(CreditLinkedSwap)
class CreditLinkedSwap : public Trade {
public:
    CreditLinkedSwap();
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CreditLinkedSwap {
    CreditLinkedSwap(const std::string& creditCurveId, const bool settlesAccrual,
                     const QuantLib::Real fixedRecoveryRate,
                     QuantExt::CreditDefaultSwap::ProtectionPaymentTime defaultPaymentTime,
                     const std::vector<ext::shared_ptr<LegData>>& independentPayments,
                     const std::vector<ext::shared_ptr<LegData>>& contingentPayments,
                     const std::vector<ext::shared_ptr<LegData>>& defaultPayments,
                     const std::vector<ext::shared_ptr<LegData>>& recoveryPayments) {
        return new CreditLinkedSwap(creditCurveId, settlesAccrual, fixedRecoveryRate, defaultPaymentTime,
            VECTOR_SWIG_TO_ORE(independentPayments), VECTOR_SWIG_TO_ORE(contingentPayments),
            VECTOR_SWIG_TO_ORE(defaultPayments), VECTOR_SWIG_TO_ORE(recoveryPayments));
    }
}

// ore/OREData/ored/portfolio/indexcreditdefaultswap.hpp

%shared_ptr(IndexCreditDefaultSwap)
class IndexCreditDefaultSwap : public Trade {
public:
    IndexCreditDefaultSwap();
    IndexCreditDefaultSwap(const Envelope& env, const IndexCreditDefaultSwapData& swap, const BasketData& basket);
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/indexcreditdefaultswapoption.hpp

%shared_ptr(IndexCreditDefaultSwapOption)
class IndexCreditDefaultSwapOption : public Trade {
public:
    IndexCreditDefaultSwapOption();
    IndexCreditDefaultSwapOption(const Envelope& env, const IndexCreditDefaultSwapData& swap,
                                  const OptionData& option, QuantLib::Real strike,
                                  const std::string& indexTerm = "",
                                  const std::string& strikeType = "Spread",
                                  const QuantLib::Date& tradeDate = QuantLib::Date(),
                                  const QuantLib::Date& fepStartDate = QuantLib::Date());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
