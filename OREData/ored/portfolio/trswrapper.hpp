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

/*! \file ored/portfolio/trswrapper.hpp
    \brief generic wrapper for trs (bond, convertible bond, equity, ...)
*/

#pragma once

#include <ored/portfolio/trs.hpp>

#include <ored/portfolio/trade.hpp>

#include <qle/indexes/fxindex.hpp>

namespace ore {
namespace data {

//! TRS Instrument Wrapper
class TRSWrapper : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;

    /* To compute the return,

       - the underlyingIndex fixing at the last relevant past valuation date and
       - the underlying->NPV()

       is used (for each underlying if several are given). The index fixing of each underyling is multiplied by the
       underlyingMultiplier, while it is assumed that the underlying->NPV() already contains this scaling. Note that the
       underlyingIndex is only queried for past fixings < today.

       The initial price is also multiplied by the underlyingMultiplier, if the number of underlyings is one. If there
       is more than one underlying, the initial price must be given as an absolute "dollar" amount.

       If includeUnderlyingCashflowsInReturn = true, the cashflows in the underlying legs between the last relevant
       valuation date and today multiplied with the underlyingMultiplier are assumed to correctly represent the
       flows of the underlying. If the underlyingIndex is an EquityIndex or CompositeIndex, dividends stored in this
       index are also taken into account as flows.

       The fx indices are used to convert the asset (return) currency to the funding currency. If no conversion is
       required, the respective index should be a nullptr. The fx indices can be given in any direction, i.e. this
       wrapper will figure out whether to apply the fixing or the inverse fixing.
    */
    TRSWrapper(const std::vector<QuantLib::ext::shared_ptr<ore::data::Trade>>& underlying,
               const std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>>& underlyingIndex,
               const std::vector<QuantLib::Real> underlyingMultiplier, const bool includeUnderlyingCashflowsInReturn,
               const QuantLib::Real initialPrice, const QuantLib::Currency& initialPriceCurrency,
               const std::vector<QuantLib::Currency>& assetCurrency, const QuantLib::Currency& returnCurrency,
               const std::vector<QuantLib::Date>& valuationSchedule, const std::vector<QuantLib::Date>& paymentSchedule,
               const std::vector<QuantLib::Leg>& fundingLegs,
               const std::vector<TRS::FundingData::NotionalType>& fundingNotionalTypes,
               const QuantLib::Currency& fundingCurrency, const QuantLib::Size fundingResetGracePeriod,
               const bool paysAsset, const bool paysFunding, const QuantLib::Leg& additionalCashflowLeg,
               const bool additionalCashflowLegPayer, const QuantLib::Currency& additionalCashflowCurrency,
               const std::vector<QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndexAsset,
               const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndexReturn,
               const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndexAdditionalCashflows,
               const std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& addFxindices);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;
    //@}

private:
    std::vector<QuantLib::ext::shared_ptr<ore::data::Trade>> underlying_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>> underlyingIndex_;
    std::vector<QuantLib::Real> underlyingMultiplier_;
    bool includeUnderlyingCashflowsInReturn_;
    QuantLib::Real initialPrice_;
    const QuantLib::Currency initialPriceCurrency_;
    const std::vector<QuantLib::Currency> assetCurrency_;
    const QuantLib::Currency returnCurrency_;
    std::vector<QuantLib::Date> valuationSchedule_, paymentSchedule_;
    std::vector<QuantLib::Leg> fundingLegs_;
    std::vector<TRS::FundingData::NotionalType> fundingNotionalTypes_;
    const QuantLib::Currency fundingCurrency_;
    const QuantLib::Size fundingResetGracePeriod_;
    bool paysAsset_, paysFunding_;
    QuantLib::Leg additionalCashflowLeg_;
    bool additionalCashflowLegPayer_;
    const QuantLib::Currency additionalCashflowCurrency_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexAsset_;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndexReturn_, fxIndexAdditionalCashflows_;
    std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> addFxIndices_;

    Date lastDate_;
};

class TRSWrapper::arguments : public virtual QuantLib::PricingEngine::arguments {
public:
    // direct copy from the instrument
    std::vector<QuantLib::ext::shared_ptr<ore::data::Trade>> underlying_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>> underlyingIndex_;
    std::vector<QuantLib::Real> underlyingMultiplier_;
    bool includeUnderlyingCashflowsInReturn_;
    QuantLib::Real initialPrice_;
    QuantLib::Currency initialPriceCurrency_;
    std::vector<QuantLib::Currency> assetCurrency_;
    QuantLib::Currency returnCurrency_;
    std::vector<QuantLib::Date> valuationSchedule_, paymentSchedule_;
    std::vector<QuantLib::Leg> fundingLegs_;
    std::vector<TRS::FundingData::NotionalType> fundingNotionalTypes_;
    QuantLib::Currency fundingCurrency_;
    QuantLib::Size fundingResetGracePeriod_;
    bool useFixedFundingLegNotional_, paysAsset_, paysFunding_;
    QuantLib::Leg additionalCashflowLeg_;
    bool additionalCashflowLegPayer_;
    QuantLib::Currency additionalCashflowCurrency_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexAsset_;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndexReturn_, fxIndexAdditionalCashflows_;
    std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> addFxIndices_;

    void validate() const override;
};

class TRSWrapper::results : public Instrument::results {
public:
    void reset() override {}
};

class TRSWrapper::engine : public QuantLib::GenericEngine<TRSWrapper::arguments, TRSWrapper::results> {};

class TRSWrapperAccrualEngine : public TRSWrapper::engine {
public:
    void calculate() const override;

private:
    /* Computes underlying value, fx conversion for each underlying and the start date of the nth current
       valuation period. Notice there might be more than one "current" valuation period, if a payment lag
       is present and nth refers to the nth such period in order the associated valuation periods are
       given. Consider e.g. the situation
       v0 < v1 < today < p0 < p1
       where [v0,v1] and [v1,v2] are two valuation periods and p0 and p1 are the associated payment dates

       The endDate will be set to the valuation end date of the valuation period if that is <= today, i.e.
       the period return is already determined, but not yet paid. Otherwise endDate is set to null.

       For nth = 0 this functions always returns true.
       For nth > 0 the function returns true if there is a nth current period to consider */
    bool computeStartValue(std::vector<QuantLib::Real>& underlyingStartValue,
                           std::vector<QuantLib::Real>& fxConversionFactor, QuantLib::Date& startDate,
                           QuantLib::Date& endDate, bool& usingInitialPrice, const Size nth) const;
    // return conversion rate from source to target on date, today's fixing projection is enforced
    QuantLib::Real getFxConversionRate(const QuantLib::Date& date, const QuantLib::Currency& source,
                                       const QuantLib::Currency& target, const bool enforceProjection) const;

    // return underlying #i fixing on date < today
    Real getUnderlyingFixing(const Size i, const QuantLib::Date& date, const bool enforceProjection) const;

    // additional inspectors
    QuantLib::Real currentNotional() const;
};

} // namespace data
} // namespace ore
