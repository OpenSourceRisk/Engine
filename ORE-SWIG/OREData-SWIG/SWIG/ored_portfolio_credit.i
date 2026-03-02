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
using ore::data::CreditDefaultSwapData;
using ORECreditDefaultSwap = ore::data::CreditDefaultSwap;
using ore::data::BasketConstituent;
using ore::data::BasketData;
using ore::data::IndexCreditDefaultSwapData;
using ore::data::SyntheticCDO;
using namespace std;
%}

%extend CreditDefaultSwap {
    enum ProtectionPaymentTime {
        atDefault,
        atPeriodEnd,
        atMaturity
    };
}

%shared_ptr(CreditDefaultSwapData)
class CreditDefaultSwapData : public XMLSerializable {
public:
    using PPT = QuantLib::CreditDefaultSwap::ProtectionPaymentTime;
    CreditDefaultSwapData(const string& issuerId, const string& creditCurveId, const LegData& leg,
                          const bool settlesAccrual = true,
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
%template(BasketConstituentVector) vector<ext::shared_ptr<BasketConstituent>>;

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
    SyntheticCDO(const Envelope& env, const LegData& leg, const string& qualifier, const BasketData& basketData,
                 double attachmentPoint, double detachmentPoint, const bool settlesAccrual = true,
                 const QuantExt::CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime =
                     QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault,
                 const string& protectionStart = string(), const string& upfrontDate = string(),
                 const Real upfrontFee = Null<Real>(), const bool rebatesAccrual = true,
                 Real recoveryRate = Null<Real>());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
