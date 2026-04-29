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

#ifndef qle_cashflows_ext_i
#define qle_cashflows_ext_i

%include common.i
%include cashflows.i
%include qle_indexes.i

namespace QuantExt {
    class FutureExpiryCalculator;
} // namespace QuantExt

namespace QuantExt {

enum class EquityReturnType {
    Price,
    Total,
    Absolute,
    Dividend
};

} // namespace QuantExt

%shared_ptr(QuantExt::EquityCoupon)
namespace QuantExt {
class EquityCoupon : public Coupon {
};
} // namespace QuantExt

%shared_ptr(QuantExt::EquityCouponPricer)
namespace QuantExt {
class EquityCouponPricer {
  public:
    Rate swapletRate();
};
} // namespace QuantExt

%shared_ptr(QuantExt::CommodityCashFlow)
namespace QuantExt {
class CommodityCashFlow : public CashFlow {
  public:
    QuantLib::Real quantity() const;
    QuantLib::Real spread() const;
    QuantLib::Real gearing() const;
    bool useFuturePrice() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::CommodityIndexedCashFlow)
namespace QuantExt {
class CommodityIndexedCashFlow : public CommodityCashFlow {
  public:
    enum class PaymentTiming { InAdvance, InArrears, RelativeToExpiry };

    CommodityIndexedCashFlow(QuantLib::Real quantity,
                             const QuantLib::Date& pricingDate,
                             const QuantLib::Date& paymentDate,
                             const ext::shared_ptr<CommodityIndex>& index,
                             QuantLib::Real spread = 0.0,
                             QuantLib::Real gearing = 1.0,
                             bool useFuturePrice = false,
                             const Date& contractDate = Date(),
                             const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                             QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>(),
                             const ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    const QuantLib::Date& pricingDate() const;
    QuantLib::Date date() const override;
    QuantLib::Real amount() const override;
};
} // namespace QuantExt

// QuantExt::CommodityQuantityFrequency enum
namespace QuantExt {
enum class CommodityQuantityFrequency {
    PerCalculationPeriod,
    PerCalendarDay,
    PerPricingDay,
    PerHour,
    PerHourAndCalendarDay
};
} // namespace QuantExt

// QuantExt::CommodityIndexedAverageCashFlow
%shared_ptr(QuantExt::CommodityIndexedAverageCashFlow)
namespace QuantExt {
class CommodityIndexedAverageCashFlow : public CommodityCashFlow {
  public:
    enum class PaymentTiming { InAdvance, InArrears };

    // Constructor 1: explicit payment date
    CommodityIndexedAverageCashFlow(
        QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
        const QuantLib::Date& paymentDate, const ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(),
        QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0,
        bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0,
        QuantLib::Integer futureMonthOffset = 0,
        const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
        bool includeEndDate = true, bool excludeStartDate = true,
        bool useBusinessDays = true);

    // Constructor 2: deduced payment date
    CommodityIndexedAverageCashFlow(
        QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
        QuantLib::Natural paymentLag, QuantLib::Calendar paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention,
        const ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(),
        QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0,
        CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming
            = CommodityIndexedAverageCashFlow::PaymentTiming::InArrears,
        bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0,
        QuantLib::Integer futureMonthOffset = 0,
        const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
        bool includeEndDate = true, bool excludeStartDate = true,
        const QuantLib::Date& paymentDateOverride = Date(),
        bool useBusinessDays = true);

    // Accessors
    const QuantLib::Date& startDate() const;
    const QuantLib::Date& endDate() const;
    ext::shared_ptr<CommodityIndex> index() const;
    QuantLib::Natural deliveryDateRoll() const;
    QuantLib::Integer futureMonthOffset() const;
    bool useBusinessDays() const;
    QuantLib::Real periodQuantity() const;
    QuantLib::Date date() const;
    QuantLib::Real amount() const;
};
} // namespace QuantExt

// QuantExt::CommodityIndexedAverageLeg builder using helper-function-with-kwargs pattern
%{
Leg _CommodityIndexedAverageLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::CommodityIndex>& index,
    const std::vector<Real>& quantities,
    Natural paymentLag = 0,
    const Calendar& paymentCalendar = Calendar(),
    BusinessDayConvention paymentConvention = Following,
    const Calendar& pricingCalendar = Calendar(),
    const std::vector<Real>& spreads = {},
    const std::vector<Real>& gearings = {},
    QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming
        = QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming::InArrears,
    bool useFuturePrice = false,
    Natural deliveryDateRoll = 0,
    Integer futureMonthOffset = 0,
    const ext::shared_ptr<QuantExt::FutureExpiryCalculator>& calc = nullptr,
    bool includeEndDate = true,
    bool excludeStartDate = true,
    bool useBusinessDays = true,
    QuantExt::CommodityQuantityFrequency quantityFrequency
        = QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr)
{
    QuantExt::CommodityIndexedAverageLeg leg(schedule, index);
    if (quantities.size() == 1)
        leg.withQuantities(quantities[0]);
    else
        leg.withQuantities(quantities);
    leg.withPaymentLag(paymentLag)
       .withPaymentCalendar(paymentCalendar)
       .withPaymentConvention(paymentConvention)
       .withPricingCalendar(pricingCalendar)
       .paymentTiming(paymentTiming)
       .useFuturePrice(useFuturePrice)
       .withDeliveryDateRoll(deliveryDateRoll)
       .withFutureMonthOffset(futureMonthOffset)
       .withFutureExpiryCalculator(calc)
       .includeEndDate(includeEndDate)
       .excludeStartDate(excludeStartDate)
       .useBusinessDays(useBusinessDays)
       .withQuantityFrequency(quantityFrequency)
       .withFxIndex(fxIndex);
    if (spreads.size() == 1)
        leg.withSpreads(spreads[0]);
    else if (!spreads.empty())
        leg.withSpreads(spreads);
    if (gearings.size() == 1)
        leg.withGearings(gearings[0]);
    else if (!gearings.empty())
        leg.withGearings(gearings);
    return leg;
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _CommodityIndexedAverageLeg;
#endif
%rename(CommodityIndexedAverageLeg) _CommodityIndexedAverageLeg;
Leg _CommodityIndexedAverageLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::CommodityIndex>& index,
    const std::vector<Real>& quantities,
    Natural paymentLag = 0,
    const Calendar& paymentCalendar = Calendar(),
    BusinessDayConvention paymentConvention = Following,
    const Calendar& pricingCalendar = Calendar(),
    const std::vector<Real>& spreads = {},
    const std::vector<Real>& gearings = {},
    QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming
        = QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming::InArrears,
    bool useFuturePrice = false,
    Natural deliveryDateRoll = 0,
    Integer futureMonthOffset = 0,
    const ext::shared_ptr<QuantExt::FutureExpiryCalculator>& calc = nullptr,
    bool includeEndDate = true,
    bool excludeStartDate = true,
    bool useBusinessDays = true,
    QuantExt::CommodityQuantityFrequency quantityFrequency
        = QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr);

%shared_ptr(QuantExt::TRSCashFlow)
namespace QuantExt {
class TRSCashFlow : public CashFlow {
  public:
    TRSCashFlow(const Date& paymentDate,
                const Date& fixingStartDate,
                const Date& fixingEndDate,
                const Real notional,
                const QuantLib::ext::shared_ptr<Index>& Index,
                const Real initialPrice = Null<Real>(),
                const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr,
                const bool applyFXIndexFixingDays = false);

    Real amount() const override;
    Date date() const override;
    const Date& fixingStartDate() const;
    const Date& fixingEndDate() const;
};
} // namespace QuantExt

#endif
