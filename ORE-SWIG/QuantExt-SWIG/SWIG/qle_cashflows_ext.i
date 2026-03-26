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
