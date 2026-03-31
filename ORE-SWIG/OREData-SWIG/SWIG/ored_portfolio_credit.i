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

%rename(ORECreditDefaultSwap) ore::data::CreditDefaultSwap;
%rename(AuctionSettlementInformation) ore::data::CreditDefaultSwapOption::AuctionSettlementInformation;

%shared_ptr(ore::data::CdsReferenceInformation)
%shared_ptr(ore::data::CreditDefaultSwapData)
%shared_ptr(ore::data::CreditDefaultSwap)
%shared_ptr(ore::data::BasketConstituent)
%shared_ptr(ore::data::BasketData)
%shared_ptr(ore::data::IndexCreditDefaultSwapData)
%shared_ptr(ore::data::SyntheticCDO)
%shared_ptr(ore::data::CreditDefaultSwapOption::AuctionSettlementInformation)
%shared_ptr(ore::data::CreditDefaultSwapOption)
%shared_ptr(ore::data::CreditLinkedSwap)
%shared_ptr(ore::data::IndexCreditDefaultSwap)
%shared_ptr(ore::data::IndexCreditDefaultSwapOption)
%template(BasketConstituentDataVector) std::vector<ore::data::BasketConstituent>;
%template(BasketConstituentVector) std::vector<QuantLib::ext::shared_ptr<ore::data::BasketConstituent>>;

%{
namespace {

QuantLib::CreditDefaultSwap::ProtectionPaymentTime parseQlProtectionPaymentTime(const std::string& value) {
    QL_REQUIRE(value == "atDefault" || value == "atPeriodEnd" || value == "atMaturity",
               "Unsupported QuantLib credit protection payment time '" << value << "'");
    if (value == "atPeriodEnd") {
        return QuantLib::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
    }
    if (value == "atMaturity") {
        return QuantLib::CreditDefaultSwap::ProtectionPaymentTime::atMaturity;
    }
    return QuantLib::CreditDefaultSwap::ProtectionPaymentTime::atDefault;
}

QuantExt::CreditDefaultSwap::ProtectionPaymentTime parseQleProtectionPaymentTime(const std::string& value) {
    QL_REQUIRE(value == "atDefault" || value == "atPeriodEnd" || value == "atMaturity",
               "Unsupported QuantExt credit protection payment time '" << value << "'");
    if (value == "atPeriodEnd") {
        return QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
    }
    if (value == "atMaturity") {
        return QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atMaturity;
    }
    return QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault;
}

} // namespace
%}

namespace ore {
namespace data {

// ore/OREData/ored/portfolio/creditdefaultswapdata.hpp

enum class CdsTier { SNRFOR, SUBLT2, SNRLAC, SECDOM, JRSUBUT2, PREFT1, LIEN1, LIEN2, LIEN3 };

enum class CdsDocClause { CR, MM, MR, XR, CR14, MM14, MR14, XR14 };

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

class CreditDefaultSwapData : public XMLSerializable {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    CreditDefaultSwapData(const std::string& issuerId, const std::string& creditCurveId, const ore::data::LegData& leg,
                          const bool settlesAccrual = true,
                          const PPT protectionPaymentTime = PPT::atDefault,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
                          const bool rebatesAccrual = true);
    CreditDefaultSwapData(const std::string& issuerId, const ore::data::CdsReferenceInformation& referenceInformation,
                          const ore::data::LegData& leg, const bool settlesAccrual = true,
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
%extend CreditDefaultSwapData {
    CreditDefaultSwapData(const std::string& issuerId, const std::string& creditCurveId, const ore::data::LegData& leg,
                          const bool settlesAccrual, const std::string& protectionPaymentTime,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
                          const bool rebatesAccrual = true) {
        return new ore::data::CreditDefaultSwapData(
            issuerId, creditCurveId, leg, settlesAccrual, parseQlProtectionPaymentTime(protectionPaymentTime),
            protectionStart, upfrontDate, upfrontFee, recoveryRate, referenceObligation, tradeDate,
            cashSettlementDays, rebatesAccrual);
    }

    CreditDefaultSwapData(const std::string& issuerId, const ore::data::CdsReferenceInformation& referenceInformation,
                          const ore::data::LegData& leg, const bool settlesAccrual,
                          const std::string& protectionPaymentTime,
                          const Date& protectionStart = Date(), const Date& upfrontDate = Date(),
                          const Real upfrontFee = Null<Real>(),
                          QuantLib::Real recoveryRate = QuantLib::Null<QuantLib::Real>(),
                          const std::string& referenceObligation = "",
                          const Date& tradeDate = Date(),
                          const std::string& cashSettlementDays = "",
                          const bool rebatesAccrual = true) {
        return new ore::data::CreditDefaultSwapData(
            issuerId, referenceInformation, leg, settlesAccrual, parseQlProtectionPaymentTime(protectionPaymentTime),
            protectionStart, upfrontDate, upfrontFee, recoveryRate, referenceObligation, tradeDate,
            cashSettlementDays, rebatesAccrual);
    }
}

class CreditDefaultSwap : public Trade {
public:
    CreditDefaultSwap(const ore::data::Envelope& env, const ore::data::CreditDefaultSwapData& swap);
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

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

class BasketData : public XMLSerializable {
public:
    BasketData(const std::vector<ore::data::BasketConstituent>& constituents);
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
};
%extend BasketData {
    BasketData(const std::vector<QuantLib::ext::shared_ptr<ore::data::BasketConstituent>>& constituents) {
        return new ore::data::BasketData(VECTOR_SWIG_TO_ORE(constituents));
    }
}

%pythoncode %{
def _ore_module():
    return globals().get("_ORE", globals().get("_OREP"))


def _basket_data_init(self, constituents):
    ore_module = _ore_module()
    if isinstance(constituents, (list, tuple)):
        vector = BasketConstituentDataVector()
        for constituent in constituents:
            vector.append(constituent)
        ore_module.BasketData_swiginit(self, ore_module.new_BasketData(vector))
        return
    ore_module.BasketData_swiginit(self, ore_module.new_BasketData(constituents))


BasketData.__init__ = _basket_data_init
%}

class IndexCreditDefaultSwapData : public CreditDefaultSwapData {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    IndexCreditDefaultSwapData(const std::string& creditCurveId,
        const ore::data::BasketData& basket,
        const ore::data::LegData& leg,
        const bool settlesAccrual = true,
        const PPT protectionPaymentTime = PPT::atDefault,
        const QuantLib::Date& protectionStart = QuantLib::Date(),
        const QuantLib::Date& upfrontDate = QuantLib::Date(),
        const QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::Date& tradeDate = QuantLib::Date(),
        const std::string& cashSettlementDays = "",
        const bool rebatesAccrual = true);
};
%extend IndexCreditDefaultSwapData {
    IndexCreditDefaultSwapData(const std::string& creditCurveId,
        const ore::data::BasketData& basket,
        const ore::data::LegData& leg,
        const bool settlesAccrual,
        const std::string& protectionPaymentTime,
        const QuantLib::Date& protectionStart = QuantLib::Date(),
        const QuantLib::Date& upfrontDate = QuantLib::Date(),
        const QuantLib::Real upfrontFee = QuantLib::Null<QuantLib::Real>(),
        const QuantLib::Date& tradeDate = QuantLib::Date(),
        const std::string& cashSettlementDays = "",
        const bool rebatesAccrual = true) {
        return new ore::data::IndexCreditDefaultSwapData(
            creditCurveId, basket, leg, settlesAccrual, parseQlProtectionPaymentTime(protectionPaymentTime),
            protectionStart, upfrontDate, upfrontFee, tradeDate, cashSettlementDays, rebatesAccrual);
    }
}

class SyntheticCDO : public Trade {
public:
    SyntheticCDO(const ore::data::Envelope& env, const ore::data::LegData& leg, const std::string& qualifier, const ore::data::BasketData& basketData,
                 double attachmentPoint, double detachmentPoint, const bool settlesAccrual = true,
                 const QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                     QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                 const std::string& protectionStart = std::string(), const std::string& upfrontDate = std::string(),
                 const Real upfrontFee = Null<Real>(), const bool rebatesAccrual = true,
                 Real recoveryRate = Null<Real>());
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend SyntheticCDO {
    SyntheticCDO(const ore::data::Envelope& env, const ore::data::LegData& leg, const std::string& qualifier,
                 const ore::data::BasketData& basketData, double attachmentPoint, double detachmentPoint,
                 const bool settlesAccrual, const std::string& protectionPaymentTime,
                 const std::string& protectionStart = std::string(), const std::string& upfrontDate = std::string(),
                 const Real upfrontFee = Null<Real>(), const bool rebatesAccrual = true,
                 Real recoveryRate = Null<Real>()) {
        return new ore::data::SyntheticCDO(
            env, leg, qualifier, basketData, attachmentPoint, detachmentPoint, settlesAccrual,
            parseQleProtectionPaymentTime(protectionPaymentTime), protectionStart, upfrontDate, upfrontFee,
            rebatesAccrual, recoveryRate);
    }
}

// Additional credit trade types from ored_portfolio2.i

// ore/OREData/ored/portfolio/creditdefaultswapoption.hpp

class CreditDefaultSwapOption : public Trade {
public:
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

    CreditDefaultSwapOption();
    CreditDefaultSwapOption(const ore::data::Envelope& env, const ore::data::OptionData& option, const ore::data::CreditDefaultSwapData& swap,
                            QuantLib::Real strike = QuantLib::Null<QuantLib::Real>(),
                            const std::string& strikeType = "Spread",
                            bool knockOut = true, const std::string& term = "");
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/creditlinkedswap.hpp

class CreditLinkedSwap : public Trade {
public:
    CreditLinkedSwap();
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};
%extend CreditLinkedSwap {
    CreditLinkedSwap(const std::string& creditCurveId, const bool settlesAccrual,
                     const QuantLib::Real fixedRecoveryRate,
                     QuantExt::CreditDefaultSwap::ProtectionPaymentTime defaultPaymentTime,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& independentPayments,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& contingentPayments,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& defaultPayments,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& recoveryPayments) {
        return new ore::data::CreditLinkedSwap(creditCurveId, settlesAccrual, fixedRecoveryRate, defaultPaymentTime,
            VECTOR_SWIG_TO_ORE(independentPayments), VECTOR_SWIG_TO_ORE(contingentPayments),
            VECTOR_SWIG_TO_ORE(defaultPayments), VECTOR_SWIG_TO_ORE(recoveryPayments));
    }

    CreditLinkedSwap(const std::string& creditCurveId, const bool settlesAccrual,
                     const QuantLib::Real fixedRecoveryRate,
                     const std::string& defaultPaymentTime,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& independentPayments,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& contingentPayments,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& defaultPayments,
                     const std::vector<ext::shared_ptr<ore::data::LegData>>& recoveryPayments) {
        return new ore::data::CreditLinkedSwap(
            creditCurveId, settlesAccrual, fixedRecoveryRate, parseQleProtectionPaymentTime(defaultPaymentTime),
            VECTOR_SWIG_TO_ORE(independentPayments), VECTOR_SWIG_TO_ORE(contingentPayments),
            VECTOR_SWIG_TO_ORE(defaultPayments), VECTOR_SWIG_TO_ORE(recoveryPayments));
    }
}

// ore/OREData/ored/portfolio/indexcreditdefaultswap.hpp

class IndexCreditDefaultSwap : public Trade {
public:
    IndexCreditDefaultSwap();
    IndexCreditDefaultSwap(const ore::data::Envelope& env, const ore::data::IndexCreditDefaultSwapData& swap, const ore::data::BasketData& basket);
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

// ore/OREData/ored/portfolio/indexcreditdefaultswapoption.hpp

class IndexCreditDefaultSwapOption : public Trade {
public:
    IndexCreditDefaultSwapOption();
    IndexCreditDefaultSwapOption(const ore::data::Envelope& env, const ore::data::IndexCreditDefaultSwapData& swap,
                                  const ore::data::OptionData& option, QuantLib::Real strike,
                                  const std::string& indexTerm = "",
                                  const std::string& strikeType = "Spread",
                                  const QuantLib::Date& tradeDate = QuantLib::Date(),
                                  const QuantLib::Date& fepStartDate = QuantLib::Date());
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%pythoncode %{
BasketData.__init__ = _basket_data_init
%}

} // namespace data
} // namespace ore

#endif
