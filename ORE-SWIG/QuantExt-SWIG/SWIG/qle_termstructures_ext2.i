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

// Note: std::map<Date, Handle<Quote>> template (DateQuoteHandleMap) deferred to Step 4
// to avoid duplicate swig::traits specializations.

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

#endif
