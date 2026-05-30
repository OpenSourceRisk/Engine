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

#ifndef qle_termstructures_ext2_i
#define qle_termstructures_ext2_i

%include common.i
%include date.i
%include types.i
%include termstructures.i
%include volatilities.i
%include defaultprobability.i
%include interpolation.i
%include <std_map.i>
%include qle_termstructures.i
%include qle_indexes.i
%include qle_futureexpirycalculator.i

%{
#include <qle/termstructures/dynamicstype.hpp>
#include <qle/termstructures/fxsmilesection.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/termstructures/commoditybasispricetermstructure.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>

// Alias the nested template enum so that SWIG-generated code can reference it
using SurvivalProbabilityCurveExtrapolation =
    QuantExt::SurvivalProbabilityCurve<QuantLib::Linear>::Extrapolation;
%}

// ================================================================
// Step 1: Support Types
// ================================================================

// --- Dynamics enums (dynamicstype.hpp) ---
namespace QuantExt {
    enum Stickyness { StickyStrike, StickyLogMoneyness, StickyAbsoluteMoneyness };
    enum ReactionToTimeDecay { ConstantVariance, ForwardForwardVariance };
    enum YieldCurveRollDown { ConstantDiscounts, ForwardForward };
}

// --- FxSmileSection (fxsmilesection.hpp) ---
%shared_ptr(QuantExt::FxSmileSection)
namespace QuantExt {
class FxSmileSection {
  public:
    FxSmileSection(QuantLib::Real spot, QuantLib::Real rd, QuantLib::Real rf, QuantLib::Time t);
    virtual ~FxSmileSection();
    virtual QuantLib::Volatility volatility(QuantLib::Real strike) const = 0;
    QuantLib::DiscountFactor domesticDiscount() const;
    QuantLib::DiscountFactor foreignDiscount() const;
};
}

// --- InterpolatedSmileSection (blackvolsurfacedelta.hpp) ---
%shared_ptr(QuantExt::InterpolatedSmileSection)
namespace QuantExt {
class InterpolatedSmileSection : public FxSmileSection {
  public:
    enum class InterpolationMethod { Linear, NaturalCubic, FinancialCubic, CubicSpline };
    InterpolatedSmileSection(QuantLib::Real spot, QuantLib::Real rd, QuantLib::Real rf, QuantLib::Time t,
                             const std::vector<QuantLib::Real>& strikes,
                             const std::vector<QuantLib::Volatility>& vols,
                             InterpolationMethod method,
                             bool flatExtrapolation = false);
    QuantLib::Volatility volatility(QuantLib::Real strike) const override;
    const std::vector<QuantLib::Real>& strikes() const;
    const std::vector<QuantLib::Volatility>& volatilities() const;
};
}

// --- ConstantSmileSection (blackvolsurfacedelta.hpp) ---
%shared_ptr(QuantExt::ConstantSmileSection)
namespace QuantExt {
class ConstantSmileSection : public FxSmileSection {
  public:
    ConstantSmileSection(QuantLib::Volatility vol);
    QuantLib::Volatility volatility(QuantLib::Real strike) const override;
};
}

// --- BootstrapHelper<OptionletVolatilityStructure> instantiation ---
%shared_ptr(BootstrapHelper<OptionletVolatilityStructure>)
%template(OptionletVolatilityHelper) BootstrapHelper<OptionletVolatilityStructure>;

#if defined(SWIGCSHARP)
SWIG_STD_VECTOR_ENHANCED( ext::shared_ptr<BootstrapHelper<OptionletVolatilityStructure> > )
#endif
namespace std {
    %template(OptionletVolatilityHelperVector)
        vector<ext::shared_ptr<BootstrapHelper<OptionletVolatilityStructure> > >;
}

// --- CapFloorHelper (capfloorhelper.hpp) ---
%shared_ptr(QuantExt::CapFloorHelper)
namespace QuantExt {
class CapFloorHelper : public BootstrapHelper<OptionletVolatilityStructure> {
  public:
    enum Type { Cap, Floor, Automatic };
    enum QuoteType { Premium, Volatility };
    CapFloorHelper(Type type,
                   const QuantLib::Period& tenor,
                   QuantLib::Rate strike,
                   const QuantLib::Handle<QuantLib::Quote>& quote,
                   const ext::shared_ptr<IborIndex>& iborIndex,
                   const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve,
                   bool moving = true,
                   const QuantLib::Date& effectiveDate = QuantLib::Date(),
                   QuoteType quoteType = Premium,
                   VolatilityType quoteVolatilityType = Normal,
                   QuantLib::Real quoteDisplacement = 0.0,
                   bool endOfMonth = false,
                   bool firstCapletExcluded = true);
    ext::shared_ptr<QuantLib::CapFloor> capFloor() const;
    QuantLib::Real impliedQuote() const;
};
}

// --- CommodityBasisPriceTermStructure abstract stub ---
%shared_ptr(QuantExt::CommodityBasisPriceTermStructure)
%nodefaultctor QuantExt::CommodityBasisPriceTermStructure;
namespace QuantExt {
class CommodityBasisPriceTermStructure : public PriceTermStructure {
  public:
    bool addBasis() const;
    bool averagingBaseCashflow() const;
    bool priceAsHistoricalFixing() const;
    QuantLib::Size monthOffset() const;
};
}

// --- SurvivalProbabilityCurve::Extrapolation (standalone alias for nested enum) ---
enum class SurvivalProbabilityCurveExtrapolation { flatFwd, flatZero };

// --- std::map<Date, Handle<Quote>> conversion for CommodityBasisPriceCurve ---
// We avoid %template(DateQuoteHandleMap) because swig::traits<Date> and
// swig::traits<Handle<Quote>> are already specialized. Instead, provide a
// Python-side helper via %typemap or use an %extend constructor that takes vectors.
%{
#include <map>
%}
%inline %{
typedef std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> > DateQuoteHandleMap;
%}

// Provide input typemap to convert a Python list of (Date, QuoteHandle) tuples to std::map
%typemap(in) const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> >& (std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> > temp) {
    if (!PyList_Check($input) && !PyDict_Check($input)) {
        SWIG_exception_fail(SWIG_TypeError, "Expected a list of (Date, QuoteHandle) tuples or a dict");
    }
    if (PyDict_Check($input)) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next($input, &pos, &key, &value)) {
            void* argp1 = 0;
            void* argp2 = 0;
            int res1 = SWIG_ConvertPtr(key, &argp1, $descriptor(QuantLib::Date*), 0);
            if (!SWIG_IsOK(res1)) {
                SWIG_exception_fail(SWIG_ArgError(res1), "dict key must be a Date");
            }
            int res2 = SWIG_ConvertPtr(value, &argp2, $descriptor(QuantLib::Handle<QuantLib::Quote>*), 0);
            if (!SWIG_IsOK(res2)) {
                SWIG_exception_fail(SWIG_ArgError(res2), "dict value must be a QuoteHandle");
            }
            temp[*reinterpret_cast<QuantLib::Date*>(argp1)] = *reinterpret_cast<QuantLib::Handle<QuantLib::Quote>*>(argp2);
        }
    } else {
        Py_ssize_t n = PyList_Size($input);
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject* item = PyList_GetItem($input, i);
            if (!PyTuple_Check(item) || PyTuple_Size(item) != 2) {
                SWIG_exception_fail(SWIG_TypeError, "Expected (Date, QuoteHandle) tuple");
            }
            void* argp1 = 0;
            void* argp2 = 0;
            int res1 = SWIG_ConvertPtr(PyTuple_GetItem(item, 0), &argp1, $descriptor(QuantLib::Date*), 0);
            int res2 = SWIG_ConvertPtr(PyTuple_GetItem(item, 1), &argp2, $descriptor(QuantLib::Handle<QuantLib::Quote>*), 0);
            if (!SWIG_IsOK(res1) || !SWIG_IsOK(res2)) {
                SWIG_exception_fail(SWIG_TypeError, "Expected (Date, QuoteHandle) tuple");
            }
            temp[*reinterpret_cast<QuantLib::Date*>(argp1)] = *reinterpret_cast<QuantLib::Handle<QuantLib::Quote>*>(argp2);
        }
    }
    $1 = &temp;
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_MAP) const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> >& {
    $1 = (PyList_Check($input) || PyDict_Check($input)) ? 1 : 0;
}

// ================================================================
// Step 2: Standalone Classes
// ================================================================

// --- DiscountRatioModifiedCurve (discountratiomodifiedcurve.hpp) ---
%shared_ptr(QuantExt::DiscountRatioModifiedCurve)
namespace QuantExt {
class DiscountRatioModifiedCurve : public YieldTermStructure {
  public:
    DiscountRatioModifiedCurve(const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurve,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& numCurve,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& denCurve);
    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurve() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure>& numeratorCurve() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure>& denominatorCurve() const;
    QuantLib::DayCounter dayCounter() const;
    QuantLib::Calendar calendar() const;
    QuantLib::Natural settlementDays() const;
    const QuantLib::Date& referenceDate() const;
    QuantLib::Date maxDate() const;
};
}

// --- SurvivalProbabilityCurve<T> (survivalprobabilitycurve.hpp) ---
// Macro to stamp out interpolator-specific specializations.
// QuantLib SWIG already has SurvivalProbabilityCurve (QuantLib::InterpolatedSurvivalProbabilityCurve<Linear>)
// which takes raw Probability values. The QuantExt version takes Handle<Quote> (observable quotes).

%define export_QleSurvivalProbabilityCurve(Name, Interpolator)

%{
typedef QuantExt::SurvivalProbabilityCurve<Interpolator> Name;
%}

%warnfilter(509) Name;

%shared_ptr(Name)
class Name : public DefaultProbabilityTermStructure {
  public:
    Name(const std::vector<QuantLib::Date>& dates,
         const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
         const QuantLib::DayCounter& dayCounter,
         const QuantLib::Calendar& calendar = QuantLib::Calendar(),
         const std::vector<QuantLib::Handle<QuantLib::Quote> >& jumps =
             std::vector<QuantLib::Handle<QuantLib::Quote> >(),
         const std::vector<QuantLib::Date>& jumpDates = std::vector<QuantLib::Date>());
    QuantLib::Date maxDate() const;
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<QuantLib::Real>& data() const;
    const std::vector<QuantLib::Probability>& survivalProbabilities() const;
    const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes() const;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real> > nodes() const;
};

%extend Name {
    Name(const std::vector<QuantLib::Date>& dates,
         const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
         const QuantLib::DayCounter& dayCounter,
         const QuantLib::Calendar& calendar,
         const std::vector<QuantLib::Handle<QuantLib::Quote> >& jumps,
         const std::vector<QuantLib::Date>& jumpDates,
         SurvivalProbabilityCurveExtrapolation extrapolation) {
        return new Name(dates, quotes, dayCounter, calendar, jumps, jumpDates, Interpolator(),
            static_cast<QuantExt::SurvivalProbabilityCurve<Interpolator>::Extrapolation>(
                static_cast<int>(extrapolation)));
    }
}

%enddef

export_QleSurvivalProbabilityCurve(SurvivalProbabilityCurveLinear, Linear);
export_QleSurvivalProbabilityCurve(SurvivalProbabilityCurveLogLinear, LogLinear);

// ================================================================
// Step 3: Volatility Surfaces (Ticket 2)
// ================================================================

// --- DynamicBlackVolTermStructure<tag::surface> ---
%{
#include <qle/termstructures/dynamicblackvoltermstructure.hpp>
using DynamicBlackVolTermStructureSurface = QuantExt::DynamicBlackVolTermStructure<QuantExt::tag::surface>;
%}

%shared_ptr(DynamicBlackVolTermStructureSurface)
class DynamicBlackVolTermStructureSurface : public BlackVolTermStructure {
  public:
    DynamicBlackVolTermStructureSurface(
        const QuantLib::Handle<QuantLib::BlackVolTermStructure>& source,
        QuantLib::Natural settlementDays,
        const QuantLib::Calendar& calendar,
        QuantExt::ReactionToTimeDecay decayMode = QuantExt::ConstantVariance,
        QuantExt::Stickyness stickyness = QuantExt::StickyLogMoneyness,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& riskfree = QuantLib::Handle<QuantLib::YieldTermStructure>(),
        const QuantLib::Handle<QuantLib::YieldTermStructure>& dividend = QuantLib::Handle<QuantLib::YieldTermStructure>(),
        const QuantLib::Handle<QuantLib::Quote>& spot = QuantLib::Handle<QuantLib::Quote>(),
        const std::vector<QuantLib::Real> initialForwardGrid = std::vector<QuantLib::Real>());
    QuantLib::Real minStrike() const;
    QuantLib::Real maxStrike() const;
    QuantLib::Date maxDate() const;
};

// --- SpreadedBlackVolatilitySurfaceMoneyness family ---
%{
#include <qle/termstructures/spreadedblackvolatilitysurfacemoneyness.hpp>
%}

%shared_ptr(QuantExt::SpreadedBlackVolatilitySurfaceMoneyness)
%nodefaultctor QuantExt::SpreadedBlackVolatilitySurfaceMoneyness;
namespace QuantExt {
class SpreadedBlackVolatilitySurfaceMoneyness : public BlackVolTermStructure {
  public:
    const std::vector<QuantLib::Real>& moneyness() const;
    QuantLib::Date maxDate() const;
    const QuantLib::Date& referenceDate() const;
    QuantLib::Real minStrike() const;
    QuantLib::Real maxStrike() const;
};
}

// Macro to stamp out each concrete subclass (all share the base constructor via using declaration)
%define export_SpreadedMoneynessSubclass(Name)
%shared_ptr(QuantExt::Name)
namespace QuantExt {
class Name : public SpreadedBlackVolatilitySurfaceMoneyness {
  public:
    Name(const QuantLib::Handle<QuantLib::BlackVolTermStructure>& referenceVol,
         const QuantLib::Handle<QuantLib::Quote>& movingSpot,
         const std::vector<QuantLib::Time>& times,
         const std::vector<QuantLib::Real>& moneyness,
         const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote> > >& volSpreads,
         const QuantLib::Handle<QuantLib::Quote>& stickySpot,
         const QuantLib::Handle<QuantLib::YieldTermStructure>& stickyDividendTs,
         const QuantLib::Handle<QuantLib::YieldTermStructure>& stickyRiskFreeTs,
         const QuantLib::Handle<QuantLib::YieldTermStructure>& movingDividendTs,
         const QuantLib::Handle<QuantLib::YieldTermStructure>& movingRiskFreeTs,
         bool stickyStrike);
};
}
%enddef

export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceMoneynessSpot)
export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceMoneynessForward)
export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceLogMoneynessSpot)
export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceLogMoneynessForward)
export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceMoneynessSpotAbsolute)
export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceMoneynessForwardAbsolute)
export_SpreadedMoneynessSubclass(SpreadedBlackVolatilitySurfaceStdDevs)

// --- BlackVarianceSurfaceSparse<Linear, Linear> ---
%{
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
using BlackVarianceSurfaceSparseLinear = QuantExt::BlackVarianceSurfaceSparse<QuantLib::Linear, QuantLib::Linear>;
%}

%shared_ptr(BlackVarianceSurfaceSparseLinear)
class BlackVarianceSurfaceSparseLinear : public BlackVolTermStructure {
  public:
    BlackVarianceSurfaceSparseLinear(
        const QuantLib::Date& referenceDate,
        const QuantLib::Calendar& cal,
        const std::vector<QuantLib::Date>& dates,
        const std::vector<QuantLib::Real>& strikes,
        const std::vector<QuantLib::Volatility>& volatilities,
        const QuantLib::DayCounter& dayCounter,
        bool lowerStrikeConstExtrap = true,
        bool upperStrikeConstExtrap = true,
        QuantLib::BlackVolTimeExtrapolation::Type timeExtrapolationType
            = QuantLib::BlackVolTimeExtrapolation::FlatVolatility,
        QuantLib::VolatilityType volType = QuantLib::ShiftedLognormal,
        QuantLib::Real shift = 0.0);
    QuantLib::Date maxDate() const;
    QuantLib::Real minStrike() const;
    QuantLib::Real maxStrike() const;
};

// --- BlackVolatilitySurfaceDelta ---
// Already wrapped in QuantLib SWIG volatilities.i. Extend with inspectors.
%{
#include <ql/termstructures/volatility/equityfx/blackvolsurfacedelta.hpp>
%}
%extend BlackVolatilitySurfaceDelta {
    const std::vector<Date>& dates() const {
        return self->dates();
    }
    ext::shared_ptr<SmileSection> blackVolSmile(Time t) const {
        return self->blackVolSmile(t);
    }
}

// ================================================================
// Step 4: Commodity Basis Curve (Ticket 2)
// ================================================================

// --- CommodityBasisPriceCurve<T> ---
%{
#include <qle/termstructures/commoditybasispricecurve.hpp>
%}

%define export_CommodityBasisPriceCurve(Name, Interpolator)

%{
typedef QuantExt::CommodityBasisPriceCurve<Interpolator> Name;
%}

%shared_ptr(Name)
class Name : public QuantExt::CommodityBasisPriceTermStructure {
  public:
    Name(const QuantLib::Date& referenceDate,
         const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> >& basisData,
         const QuantLib::ext::shared_ptr<QuantExt::FutureExpiryCalculator>& basisFec,
         const QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>& baseIndex,
         const QuantLib::ext::shared_ptr<QuantExt::FutureExpiryCalculator>& baseFec,
         bool addBasis = true,
         QuantLib::Size monthOffset = 0,
         bool priceAsHistFixing = true);
    QuantLib::Date maxDate() const;
    QuantLib::Time maxTime() const;
    QuantLib::Time minTime() const;
    std::vector<QuantLib::Date> pillarDates() const;
    const QuantLib::Currency& currency() const;
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Real>& prices() const;
};

%enddef

export_CommodityBasisPriceCurve(CommodityBasisPriceCurveLinear, Linear);
export_CommodityBasisPriceCurve(CommodityBasisPriceCurveLogLinear, LogLinear);

// ================================================================
// Step 5: Optionlet Bootstrapping (Ticket 2)
// ================================================================

// --- OISCapFloorHelper ---
%{
#include <qle/termstructures/oiscapfloorhelper.hpp>
%}

%shared_ptr(QuantExt::OISCapFloorHelper)
namespace QuantExt {
class OISCapFloorHelper : public BootstrapHelper<OptionletVolatilityStructure> {
  public:
    OISCapFloorHelper(CapFloorHelper::Type type,
                      const Period& tenor,
                      const Period& rateComputationPeriod,
                      Rate strike,
                      const Handle<Quote>& quote,
                      const ext::shared_ptr<OvernightIndex>& index,
                      const Handle<YieldTermStructure>& discountingCurve,
                      bool moving = true,
                      const Date& effectiveDate = Date(),
                      CapFloorHelper::QuoteType quoteType = CapFloorHelper::Premium,
                      VolatilityType quoteVolatilityType = Normal,
                      Real quoteDisplacement = 0.0,
                      bool useEffectiveVolatility = false);
    Leg capFloor() const;
    Real impliedQuote() const;
    Real atmStrike() const;
};
}

// --- InterpolatedOptionletCurve<Linear> (base class stub) ---
%{
#include <qle/termstructures/optionletcurve.hpp>
using InterpolatedOptionletCurveLinear = QuantExt::InterpolatedOptionletCurve<QuantLib::Linear>;
%}

%shared_ptr(InterpolatedOptionletCurveLinear)
%nodefaultctor InterpolatedOptionletCurveLinear;
class InterpolatedOptionletCurveLinear : public OptionletVolatilityStructure {
  public:
    QuantLib::Date maxDate() const;
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<QuantLib::Real>& volatilities() const;
    const std::vector<QuantLib::Real>& data() const;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real> > nodes() const;
    QuantLib::VolatilityType volatilityType() const;
    QuantLib::Real displacement() const;
};

// --- PiecewiseOptionletCurve<Linear, IterativeBootstrap> ---
%{
#include <qle/termstructures/piecewiseoptionletcurve.hpp>
using PiecewiseOptionletCurveLinear =
    QuantExt::PiecewiseOptionletCurve<QuantLib::Linear, QuantExt::IterativeBootstrap>;
%}

%shared_ptr(PiecewiseOptionletCurveLinear)
class PiecewiseOptionletCurveLinear : public InterpolatedOptionletCurveLinear {
  public:
    PiecewiseOptionletCurveLinear(
        const QuantLib::Date& referenceDate,
        const std::vector<QuantLib::ext::shared_ptr<BootstrapHelper<OptionletVolatilityStructure> > >& instruments,
        const QuantLib::Calendar& calendar,
        QuantLib::BusinessDayConvention bdc,
        const QuantLib::DayCounter& dayCounter,
        QuantLib::VolatilityType volatilityType = QuantLib::Normal,
        QuantLib::Real displacement = 0.0,
        bool flatFirstPeriod = true,
        bool useEffectiveVolatility = false);
    QuantLib::Date maxDate() const;
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<QuantLib::Real>& volatilities() const;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real> > nodes() const;
};

#endif
