/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/trs.hpp
    \brief trs
*/

#pragma once

#include <ored/portfolio/simmcreditqualifiermapping.hpp>

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>

#include <boost/optional.hpp>

namespace ore {
namespace data {

/*! TRS trade class */
class TRS : public Trade {
public:
    class ReturnData : public XMLSerializable {
    public:
        ReturnData() : payer_(false), initialPrice_(Null<Real>()), payUnderlyingCashFlowsImmediately_(false) {}
        ReturnData(const bool payer, const std::string& currency, const ScheduleData& scheduleData,
                   const std::string& observationLag, const std::string& observationConvention,
                   const std::string& observationCalendar, const std::string& paymentLag,
                   const std::string& paymentConvention, const std::string& paymentCalendar,
                   const std::vector<std::string>& paymentDates, const Real initialPrice,
                   const std::string& initialPriceCurrency, const std::vector<std::string>& fxTerms,
                   const boost::optional<bool> payUnderlyingCashFlowsImmediately)
            : payer_(payer), currency_(currency), scheduleData_(scheduleData), observationLag_(observationLag),
              observationCalendar_(observationCalendar), paymentLag_(paymentLag), paymentConvention_(paymentConvention),
              paymentCalendar_(paymentCalendar), paymentDates_(paymentDates), initialPrice_(initialPrice),
              initialPriceCurrency_(initialPriceCurrency), fxTerms_(fxTerms),
              payUnderlyingCashFlowsImmediately_(payUnderlyingCashFlowsImmediately) {}

        bool payer() const { return payer_; }
        const std::string& currency() const { return currency_; }
        const ScheduleData& scheduleData() const { return scheduleData_; }
        const std::string& observationLag() const { return observationLag_; }
        const std::string& observationConvention() const { return observationConvention_; }
        const std::string& observationCalendar() const { return observationCalendar_; }
        const std::string& paymentLag() const { return paymentLag_; }
        const std::string& paymentConvention() const { return paymentConvention_; }
        const std::string& paymentCalendar() const { return paymentCalendar_; }
        const std::vector<std::string>& paymentDates() const { return paymentDates_; }
        Real initialPrice() const { return initialPrice_; }
        const std::string& initialPriceCurrency() const { return initialPriceCurrency_; }
        const std::vector<std::string>& fxTerms() const { return fxTerms_; }
        boost::optional<bool> payUnderlyingCashFlowsImmediately() const { return payUnderlyingCashFlowsImmediately_; }

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    private:
        bool payer_;
        std::string currency_;
        ScheduleData scheduleData_;
        std::string observationLag_, observationConvention_, observationCalendar_, paymentLag_, paymentConvention_,
            paymentCalendar_;
        std::vector<std::string> paymentDates_;
        Real initialPrice_;
        std::string initialPriceCurrency_;
        std::vector<std::string> fxTerms_; // FX index strings
	boost::optional<bool> payUnderlyingCashFlowsImmediately_;
    };

    class FundingData : public XMLSerializable {
    public:
        enum class NotionalType { PeriodReset, DailyReset, Fixed };
        FundingData() {}
        explicit FundingData(const std::vector<LegData>& legData, const std::vector<NotionalType>& notionalType = {},
                             const Size fundingResetGracePeriod = 0)
            : legData_(legData), notionalType_(notionalType), fundingResetGracePeriod_(fundingResetGracePeriod) {}

        const std::vector<LegData>& legData() const { return legData_; }
        std::vector<LegData>& legData() { return legData_; }
        const std::vector<NotionalType>& notionalType() const { return notionalType_; }
        std::vector<NotionalType>& notionalType() { return notionalType_; }
        QuantLib::Size fundingResetGracePeriod() const { return fundingResetGracePeriod_; }
        QuantLib::Size& fundingResetGracePeriod() { return fundingResetGracePeriod_; }

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    private:
        std::vector<LegData> legData_;
        std::vector<NotionalType> notionalType_;
        QuantLib::Size fundingResetGracePeriod_ = 0;
    };

    class AdditionalCashflowData : public XMLSerializable {
    public:
        AdditionalCashflowData() {}
        AdditionalCashflowData(const LegData& legData) : legData_(legData) {}

        const LegData& legData() const { return legData_; }
        LegData& legData() { return legData_; }

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    private:
        LegData legData_;
    };

    TRS() : Trade("TotalReturnSwap") {}

    TRS(const Envelope& env, const std::vector<QuantLib::ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const ReturnData& returnData,
        const FundingData& fundingData, const AdditionalCashflowData& additionalCashflowData)
        : Trade("TotalReturnSwap", env), underlying_(underlying), underlyingDerivativeId_(underlyingDerivativeId),
          returnData_(returnData), fundingData_(fundingData), additionalCashflowData_(additionalCashflowData) {
        QL_REQUIRE(underlying_.size() == underlyingDerivativeId_.size(),
                   "TRS: underlying size (" << underlying_.size() << ") must match underlying derivative id size ("
                                            << underlyingDerivativeId_.size() << ")");
    }

    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! Inspectors
    //@{
    const std::vector<QuantLib::ext::shared_ptr<Trade>>& underlying() const { return underlying_; }
    const ReturnData& returnData() const { return returnData_; }
    const FundingData& fundingData() const { return fundingData_; }
    const AdditionalCashflowData& additionalCashflowData() const { return additionalCashflowData_; }
    const std::string& creditRiskCurrency() const { return creditRiskCurrency_; }
    const std::map<std::string, SimmCreditQualifierMapping>& creditQualifierMapping() const {
        return creditQualifierMapping_;
    }
    //@}

    //! Interface
    //@{
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;
    QuantLib::Real notional() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

protected:
    QuantLib::ext::shared_ptr<QuantExt::FxIndex>
    getFxIndex(const QuantLib::ext::shared_ptr<Market> market, const std::string& configuration, const std::string& domestic,
               const std::string& foreign, std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndices,
               std::set<std::string>& missingFxIndexPairs) const;

    mutable std::vector<QuantLib::ext::shared_ptr<Trade>> underlying_;
    // empty if underlying is not from a Derivative subnode of UnderlyingData
    mutable std::vector<std::string> underlyingDerivativeId_;
    ReturnData returnData_;
    FundingData fundingData_;
    AdditionalCashflowData additionalCashflowData_;

    std::string creditRiskCurrency_;
    std::map<std::string, SimmCreditQualifierMapping> creditQualifierMapping_;
};

TRS::FundingData::NotionalType parseTrsFundingNotionalType(const std::string& s);
std::ostream& operator<<(std::ostream& os, const TRS::FundingData::NotionalType t);

/*! just an Alias */
class CFD : public TRS {
public:
    CFD() : TRS() { tradeType_ = "ContractForDifference"; }
    CFD(const Envelope& env, const std::vector<QuantLib::ext::shared_ptr<Trade>>& underlying,
        const std::vector<std::string>& underlyingDerivativeId, const ReturnData& returnData,
        const FundingData& fundingData, const AdditionalCashflowData& additionalCashflowData)
        : TRS(env, underlying, underlyingDerivativeId, returnData, fundingData, additionalCashflowData) {
        tradeType_ = "ContractForDifference";
    }
};

} // namespace data
} // namespace ore
