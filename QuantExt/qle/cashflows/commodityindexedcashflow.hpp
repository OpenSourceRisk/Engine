/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/commodityindexedcashflow.hpp
    \brief Cash flow dependent on a single commodity spot price or future's settlement price
 */

#ifndef quantext_commodity_indexed_cash_flow_hpp
#define quantext_commodity_indexed_cash_flow_hpp

#include <boost/optional.hpp>
#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Cash flow dependent on a single commodity spot price or futures settlement price on a given pricing date
class CommodityIndexedCashFlow : public CommodityCashFlow {

public:
    enum class PaymentTiming { InAdvance, InArrears, RelativeToExpiry };

    //! Constructor taking an explicit \p pricingDate and \p paymentDate
    CommodityIndexedCashFlow(QuantLib::Real quantity, const QuantLib::Date& pricingDate,
                             const QuantLib::Date& paymentDate, const ext::shared_ptr<CommodityIndex>& index,
                             QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0, bool useFuturePrice = false,
                             const Date& contractDate = Date(),
                             const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                             QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>(),
                             const ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    /*! Constructor taking a period \p startDate, \p endDate and some conventions. The pricing date and payment date
        are derived from the start date and end date using the conventions.
    */
    CommodityIndexedCashFlow(QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
                             const ext::shared_ptr<CommodityIndex>& index, QuantLib::Natural paymentLag,
                             const QuantLib::Calendar& paymentCalendar,
                             QuantLib::BusinessDayConvention paymentConvention, QuantLib::Natural pricingLag,
                             const QuantLib::Calendar& pricingLagCalendar, QuantLib::Real spread = 0.0,
                             QuantLib::Real gearing = 1.0, PaymentTiming paymentTiming = PaymentTiming::InArrears,
                             bool isInArrears = true, bool useFuturePrice = false, bool useFutureExpiryDate = true,
                             QuantLib::Natural futureMonthOffset = 0,
                             const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                             const QuantLib::Date& paymentDateOverride = Date(),
                             const QuantLib::Date& pricingDateOverride = Date(),
                             QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>(),
                             const ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    //! \name Inspectors
    //@{
    
    const QuantLib::Date& pricingDate() const { return pricingDate_; }
    bool useFutureExpiryDate() const { return useFutureExpiryDate_; }
    QuantLib::Natural futureMonthOffset() const { return futureMonthOffset_; }
    QuantLib::Real periodQuantity() const override { return periodQuantity_; }
    QuantLib::Natural dailyExpiryOffset() const { return dailyExpiryOffset_; }

    //@}
    //! \name CommodityCashFlow interface
    //@{
    const std::map<QuantLib::Date, ext::shared_ptr<CommodityIndex>>& indices() const override { return indices_; }
    
  
    QuantLib::Date lastPricingDate() const override { return pricingDate(); }
    //@}
    
    //! \name Event interface
    //@{
    QuantLib::Date date() const override { return paymentDate_; }
    //@}

    //! \name CashFlow interface
    //@{
    QuantLib::Real amount() const override;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}

    //! Allow the full calculation period quantity to be updated.
    void setPeriodQuantity(QuantLib::Real periodQuantity);

private:
    void performCalculations() const override;

    QuantLib::Date pricingDate_;
    QuantLib::Date paymentDate_;
    bool useFutureExpiryDate_;
    QuantLib::Natural futureMonthOffset_;
    QuantLib::Real periodQuantity_;
    QuantLib::Natural dailyExpiryOffset_;
    std::map<QuantLib::Date, ext::shared_ptr<CommodityIndex>> indices_;

    //! Shared initialisation
    void init(const ext::shared_ptr<FutureExpiryCalculator>& calc,
              const QuantLib::Date& contractDate = QuantLib::Date(),
              const PaymentTiming paymentTiming = PaymentTiming::InArrears,
              const QuantLib::Date& startDate = QuantLib::Date(), const QuantLib::Date& endDate = QuantLib::Date(),
              const QuantLib::Natural paymentLag = 0,
              const QuantLib::BusinessDayConvention paymentConvention = QuantLib::Unadjusted,
              const QuantLib::Calendar& paymentCalendar = QuantLib::NullCalendar());
};

//! Helper class building a sequence of commodity indexed cashflows
class CommodityIndexedLeg {

public:
    CommodityIndexedLeg(const QuantLib::Schedule& schedule, const ext::shared_ptr<CommodityIndex>& index);
    CommodityIndexedLeg& withQuantities(QuantLib::Real quantity);
    CommodityIndexedLeg& withQuantities(const std::vector<QuantLib::Real>& quantities);
    CommodityIndexedLeg& withPaymentLag(QuantLib::Natural paymentLag);
    CommodityIndexedLeg& withPaymentCalendar(const QuantLib::Calendar& paymentCalendar);
    CommodityIndexedLeg& withPaymentConvention(QuantLib::BusinessDayConvention paymentConvention);
    CommodityIndexedLeg& withPricingLag(QuantLib::Natural pricingLag);
    CommodityIndexedLeg& withPricingLagCalendar(const QuantLib::Calendar& pricingLagCalendar);
    CommodityIndexedLeg& withSpreads(QuantLib::Real spread);
    CommodityIndexedLeg& withSpreads(const std::vector<QuantLib::Real>& spreads);
    CommodityIndexedLeg& withGearings(QuantLib::Real gearing);
    CommodityIndexedLeg& withGearings(const std::vector<QuantLib::Real>& gearings);
    CommodityIndexedLeg& paymentTiming(CommodityIndexedCashFlow::PaymentTiming paymentTiming);
    CommodityIndexedLeg& inArrears(bool flag = true);
    CommodityIndexedLeg& useFuturePrice(bool flag = false);
    CommodityIndexedLeg& useFutureExpiryDate(bool flag = true);
    CommodityIndexedLeg& withFutureMonthOffset(QuantLib::Natural futureMonthOffset);
    CommodityIndexedLeg& withFutureExpiryCalculator(const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr);
    CommodityIndexedLeg& payAtMaturity(bool flag = false);
    CommodityIndexedLeg& withPricingDates(const std::vector<QuantLib::Date>& pricingDates);
    CommodityIndexedLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);
    CommodityIndexedLeg& withDailyExpiryOffset(QuantLib::Natural dailyExpiryOffset);
    CommodityIndexedLeg& withFxIndex(const ext::shared_ptr<FxIndex>& fxIndex);

    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<CommodityIndex> index_;
    std::vector<QuantLib::Real> quantities_;
    QuantLib::Natural paymentLag_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Natural pricingLag_;
    QuantLib::Calendar pricingLagCalendar_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<QuantLib::Real> gearings_;
    CommodityIndexedCashFlow::PaymentTiming paymentTiming_;
    bool inArrears_;
    bool useFuturePrice_;
    bool useFutureExpiryDate_;
    QuantLib::Natural futureMonthOffset_;
    ext::shared_ptr<FutureExpiryCalculator> calc_;
    bool payAtMaturity_;
    std::vector<QuantLib::Date> pricingDates_;
    std::vector<QuantLib::Date> paymentDates_;
    QuantLib::Natural dailyExpiryOffset_;
    ext::shared_ptr<FxIndex> fxIndex_;
};

} // namespace QuantExt

#endif
