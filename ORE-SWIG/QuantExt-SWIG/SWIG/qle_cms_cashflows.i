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

#ifndef qle_cms_cashflows_i
#define qle_cms_cashflows_i

%include cashflows.i
%include volatilities.i
%include qle_termstructures_ext.i

%{
#include <qle/cashflows/durationadjustedcmscoupon.hpp>
#include <qle/cashflows/durationadjustedcmscoupontsrpricer.hpp>
#include <qle/cashflows/lognormalcmsspreadpricer.hpp>
#include <qle/models/annuitymapping.hpp>
#include <qle/models/linearannuitymapping.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
%}

// ---------------------------------------------------------------------------
// QuantExt::AnnuityMappingBuilder – abstract base for TSR pricer builders
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::AnnuityMappingBuilder)
%nodefaultctor QuantExt::AnnuityMappingBuilder;
namespace QuantExt {
class AnnuityMappingBuilder : public Observable, public Observer {
  public:
    // Pure virtual – no direct construction from Python. Use a concrete subclass.
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt::LinearAnnuityMappingBuilder – linear mapping f(S) = a*S + b
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::LinearAnnuityMappingBuilder)
namespace QuantExt {
class LinearAnnuityMappingBuilder : public AnnuityMappingBuilder {
  public:
    LinearAnnuityMappingBuilder(Real a, Real b);
    LinearAnnuityMappingBuilder(const Handle<Quote>& reversion);
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt::DurationAdjustedCmsCoupon – CMS coupon scaled by duration factor
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::DurationAdjustedCmsCoupon)
namespace QuantExt {
class DurationAdjustedCmsCoupon : public FloatingRateCoupon {
  public:
    DurationAdjustedCmsCoupon(
        const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        Natural fixingDays,
        const ext::shared_ptr<SwapIndex>& index,
        Size duration = 0,
        Real gearing = 1.0,
        Spread spread = 0.0,
        const Date& refPeriodStart = Date(),
        const Date& refPeriodEnd = Date(),
        const DayCounter& dayCounter = DayCounter(),
        bool isInArrears = false,
        const Date& exCouponDate = Date());

    const ext::shared_ptr<SwapIndex>& swapIndex() const;
    Size duration() const;
    Real durationAdjustment() const;
    Rate indexFixing() const;
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// DurationAdjustedCmsLeg builder (kwargs pattern)
// ---------------------------------------------------------------------------
%{
Leg _DurationAdjustedCmsLeg(
    const Schedule& schedule,
    const ext::shared_ptr<SwapIndex>& swapIndex,
    Size duration,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    BusinessDayConvention paymentConvention = Following,
    Natural paymentLag = 0,
    const Calendar& paymentCalendar = Calendar(),
    Natural fixingDays = Null<Natural>(),
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    bool inArrears = false,
    bool zeroPayments = false)
{
    QuantExt::DurationAdjustedCmsLeg leg(schedule, swapIndex, duration);
    if (notionals.size() == 1)
        leg.withNotionals(notionals[0]);
    else
        leg.withNotionals(notionals);
    leg.withPaymentDayCounter(paymentDayCounter)
       .withPaymentAdjustment(paymentConvention)
       .withPaymentLag(paymentLag)
       .withPaymentCalendar(paymentCalendar)
       .withFixingDays(fixingDays);
    if (gearings.size() == 1)
        leg.withGearings(gearings[0]);
    else if (!gearings.empty())
        leg.withGearings(gearings);
    if (spreads.size() == 1)
        leg.withSpreads(spreads[0]);
    else if (!spreads.empty())
        leg.withSpreads(spreads);
    if (caps.size() == 1)
        leg.withCaps(caps[0]);
    else if (!caps.empty())
        leg.withCaps(caps);
    if (floors.size() == 1)
        leg.withFloors(floors[0]);
    else if (!floors.empty())
        leg.withFloors(floors);
    leg.inArrears(inArrears).withZeroPayments(zeroPayments);
    return leg;
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _DurationAdjustedCmsLeg;
#endif
%rename(DurationAdjustedCmsLeg) _DurationAdjustedCmsLeg;
Leg _DurationAdjustedCmsLeg(
    const Schedule& schedule,
    const ext::shared_ptr<SwapIndex>& swapIndex,
    Size duration,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    BusinessDayConvention paymentConvention = Following,
    Natural paymentLag = 0,
    const Calendar& paymentCalendar = Calendar(),
    Natural fixingDays = Null<Natural>(),
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    bool inArrears = false,
    bool zeroPayments = false);

// ---------------------------------------------------------------------------
// QuantExt::DurationAdjustedCmsCouponTsrPricer – TSR pricer for duration CMS
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::DurationAdjustedCmsCouponTsrPricer)
namespace QuantExt {
class DurationAdjustedCmsCouponTsrPricer : public CmsCouponPricer {
  public:
    // The underlying C++ constructor has a 5th `Integrator` parameter that
    // defaults to an empty shared_ptr. SWIG exposes the 4-parameter form;
    // the default integrator (Gauss-Legendre 16-point) is used automatically.
    DurationAdjustedCmsCouponTsrPricer(
        const Handle<SwaptionVolatilityStructure>& swaptionVol,
        const ext::shared_ptr<AnnuityMappingBuilder>& annuityMappingBuilder,
        Real lowerIntegrationBound = -0.3,
        Real upperIntegrationBound = 0.3);
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt::CmsSpreadCouponPricer2 – abstract base with CorrelationTermStructure
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::CmsSpreadCouponPricer2)
%nodefaultctor QuantExt::CmsSpreadCouponPricer2;
namespace QuantExt {
class CmsSpreadCouponPricer2 : public CmsSpreadCouponPricer {
  public:
    Real correlation(Time t, Real strike = 1.0) const;
    void setCorrelationCurve(
        const Handle<QuantExt::CorrelationTermStructure>& correlation
            = Handle<QuantExt::CorrelationTermStructure>());
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// Typemap for ext::optional<VolatilityType> used by LognormalCmsSpreadPricer
// ---------------------------------------------------------------------------
#if defined(SWIGPYTHON)
%typemap(in) QuantLib::ext::optional<QuantLib::VolatilityType> %{
    if ($input == Py_None)
        $1 = QuantLib::ext::nullopt;
    else if (PyLong_Check($input))
        $1 = static_cast<QuantLib::VolatilityType>(PyLong_AsLong($input));
    else
        SWIG_exception(SWIG_TypeError, "int (VolatilityType enum) or None expected");
%}
%typecheck(SWIG_TYPECHECK_INTEGER) QuantLib::ext::optional<QuantLib::VolatilityType> %{
    $1 = (PyLong_Check($input) || $input == Py_None) ? 1 : 0;
%}
#endif

// ---------------------------------------------------------------------------
// QuantExt::LognormalCmsSpreadPricer – renamed to QLELognormalCmsSpreadPricer
// to avoid collision with QuantLib::LognormalCmsSpreadPricer in cashflows.i
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::LognormalCmsSpreadPricer)
%rename(QLELognormalCmsSpreadPricer) QuantExt::LognormalCmsSpreadPricer;
namespace QuantExt {
class LognormalCmsSpreadPricer : public CmsSpreadCouponPricer2 {
  public:
    LognormalCmsSpreadPricer(
        const ext::shared_ptr<CmsCouponPricer>& cmsPricer,
        const Handle<QuantExt::CorrelationTermStructure>& correlation,
        const Handle<YieldTermStructure>& couponDiscountCurve
            = Handle<YieldTermStructure>(),
        Size integrationPoints = 16,
        const QuantLib::ext::optional<QuantLib::VolatilityType> volatilityType
            = QuantLib::ext::nullopt,
        Real shift1 = Null<Real>(),
        Real shift2 = Null<Real>());
};
} // namespace QuantExt

#endif
