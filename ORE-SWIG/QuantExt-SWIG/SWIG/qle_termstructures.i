
/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_termstructures_i
#define qle_termstructures_i

%include termstructures.i
%include volatilities.i
%include indexes.i
%include currencies.i
%include observer.i

%{
using QuantExt::PriceTermStructure;
using QuantExt::InterpolatedPriceCurve;
using QuantExt::FxBlackVannaVolgaVolatilitySurface;
using QuantExt::BlackVarianceSurfaceMoneynessSpot;
using QuantExt::BlackVarianceSurfaceMoneynessForward;
using QuantExt::EquityAnnouncedDividendCurve;
using QLESwaptionVolCube2 = QuantExt::SwaptionVolCube2;
using QuantExt::SwaptionVolCubeWithATM;
using QuantLib::SwaptionVolatilityCube;
using QuantExt::SwaptionVolatilityConstantSpread;
using QuantExt::BlackVolatilityWithATM;
using QuantExt::CreditCurve;
using QuantExt::CreditVolCurve;
using QuantExt::CreditVolCurveWrapper;
using QuantLib::Observer;
using QuantLib::VolatilityTermStructure;
using QuantExt::convertSwaptionVolatility;
using QuantExt::ShiftScheme;
using QuantExt::ShiftType;
using QuantExt::parseShiftScheme;
using QuantExt::parseShiftType;
%}

enum class ShiftScheme { Forward, Backward, Central };
enum class ShiftType { Absolute, Relative };

ShiftScheme parseShiftScheme(const std::string& s);
ShiftType parseShiftType(const std::string& s);

%shared_ptr(PriceTermStructure)
class PriceTermStructure : public TermStructure {
  private:
    PriceTermStructure();
  public:
    QuantLib::Real price(QuantLib::Time t, bool extrapolate = false) const;
    QuantLib::Real price(const QuantLib::Date& d, bool extrapolate = false) const;
};

%template(PriceTermStructureHandle) Handle<PriceTermStructure>;
%template(RelinkablePriceTermStructureHandle) RelinkableHandle<PriceTermStructure>;

%define export_Interpolated_Price_Curve(Name, Interpolator)

%{
typedef InterpolatedPriceCurve<Interpolator> Name;
%}

%warnfilter(509) Name;

%shared_ptr(Name)
class Name : public PriceTermStructure {
  public:
    Name(const std::vector<QuantLib::Period>& tenors,
	 const std::vector<QuantLib::Real>& prices,
	 const QuantLib::DayCounter& dc,
	 const QuantLib::Currency& currency);
    Name(const std::vector<QuantLib::Period>& tenors,
	 const std::vector<QuantLib::Handle<QuantLib::Quote>>& quotes,
	 const QuantLib::DayCounter& dc,
	 const QuantLib::Currency& currency);
    Name(const QuantLib::Date& referenceDate,
	 const std::vector<QuantLib::Date>& dates,
	 const std::vector<QuantLib::Handle<QuantLib::Quote>>& quotes,
	 const QuantLib::DayCounter& dc,
	 const QuantLib::Currency& currency);
    Name(const QuantLib::Date& referenceDate,
	 const std::vector<QuantLib::Date>& dates,
	 const std::vector<QuantLib::Real>& prices,
	 const QuantLib::DayCounter& dc,
	 const QuantLib::Currency& currency);
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Real>& prices() const;
};

%enddef

export_Interpolated_Price_Curve(LinearInterpolatedPriceCurve, Linear);
export_Interpolated_Price_Curve(BackwardFlatInterpolatedPriceCurve, BackwardFlat);
export_Interpolated_Price_Curve(LogLinearInterpolatedPriceCurve, LogLinear);
export_Interpolated_Price_Curve(CubicInterpolatedPriceCurve, Cubic);
export_Interpolated_Price_Curve(SplineCubicInterpolatedPriceCurve, SplineCubic);
export_Interpolated_Price_Curve(MonotonicCubicInterpolatedPriceCurve, MonotonicCubic);


%shared_ptr(QLESwaptionVolCube2)
class QLESwaptionVolCube2 : public SwaptionVolatilityCube {
  public:
    QLESwaptionVolCube2(const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& atmVolStructure,
                        const std::vector<QuantLib::Period>& optionTenors,
                        const std::vector<QuantLib::Period>& swapTenors,
                        const std::vector<QuantLib::Spread>& strikeSpreads,
                        const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>>& volSpreads,
                        const ext::shared_ptr<SwapIndex>& swapIndexBase,
                        const ext::shared_ptr<SwapIndex>& shortSwapIndexBase,
                        bool vegaWeightedSmileFit,
                        bool flatExtrapolation,
                        bool volsAreSpreads = true);
};


%shared_ptr(FxBlackVannaVolgaVolatilitySurface)
class FxBlackVannaVolgaVolatilitySurface : public BlackVolTermStructure {
  public:
    FxBlackVannaVolgaVolatilitySurface(const QuantLib::Date& refDate,
                                       const std::vector<QuantLib::Date>& dates,
                                       const std::vector<QuantLib::Volatility>& atmVols,
                                       const std::vector<QuantLib::Volatility>& rr25d,
                                       const std::vector<QuantLib::Volatility>& bf25d,
                                       const QuantLib::DayCounter& dc,
                                       const QuantLib::Calendar& cal,
                                       const QuantLib::Handle<QuantLib::Quote>& fx,
                                       const QuantLib::Handle<QuantLib::YieldTermStructure>& dom,
                                       const QuantLib::Handle<QuantLib::YieldTermStructure>& fore);
};



%shared_ptr(BlackVarianceSurfaceMoneynessSpot)
class BlackVarianceSurfaceMoneynessSpot : public BlackVolTermStructure {
  public:
    BlackVarianceSurfaceMoneynessSpot(const QuantLib::Calendar& cal,
                                      const QuantLib::Handle<QuantLib::Quote>& spot,
                                      const std::vector<QuantLib::Time>& times,
                                      const std::vector<QuantLib::Real>& moneyness,
                                      const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>>& blackVolMatrix,
                                      const QuantLib::DayCounter& dayCounter,
                                      bool stickyStrike);
};

%shared_ptr(BlackVarianceSurfaceMoneynessForward)
class BlackVarianceSurfaceMoneynessForward : public BlackVolTermStructure {
  public:
    BlackVarianceSurfaceMoneynessForward(const QuantLib::Calendar& cal,
                                         const QuantLib::Handle<QuantLib::Quote>& spot,
                                         const std::vector<QuantLib::Time>& times,
                                         const std::vector<QuantLib::Real>& moneyness,
                                         const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>>& blackVolMatrix,
                                         const QuantLib::DayCounter& dayCounter,
                                         const QuantLib::Handle<QuantLib::YieldTermStructure>& forTS,
                                         const QuantLib::Handle<QuantLib::YieldTermStructure>& domTS,
                                         bool stickyStrike = false);
};


%shared_ptr(SwaptionVolCubeWithATM)
class SwaptionVolCubeWithATM : public SwaptionVolatilityStructure {
  public:
    SwaptionVolCubeWithATM(const ext::shared_ptr<SwaptionVolatilityCube>& cube);
};

%shared_ptr(SwaptionVolatilityConstantSpread)
class SwaptionVolatilityConstantSpread : public SwaptionVolatilityStructure {
  public:
    SwaptionVolatilityConstantSpread(const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& atm,
                                     const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& cube);
    const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& atmVol();
    const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& cube();
};

Real convertSwaptionVolatility(const Date& asof, const Period& optionTenor, const Period& swapTenor,
                               const boost::shared_ptr<SwapIndex>& swapIndexBase,
                               const boost::shared_ptr<SwapIndex>& shortSwapIndexBase, const DayCounter volDayCounter,
                               const Real strikeSpread, const Real inputVol, const QuantLib::VolatilityType inputType,
                               const Real inputShift, const QuantLib::VolatilityType outputType,
                               const Real outputShift);

%shared_ptr(CreditCurve)
class CreditCurve : public Observer {
    public:
        struct RefData {
            RefData() {}
            QuantLib::Date startDate = QuantLib::Null<QuantLib::Date>();
            QuantLib::Period indexTerm = 0 * QuantLib::Days;
            QuantLib::Period tenor = 3 * QuantLib::Months;
            QuantLib::Calendar calendar = QuantLib::WeekendsOnly();
            QuantLib::BusinessDayConvention convention = QuantLib::Following;
            QuantLib::BusinessDayConvention termConvention = QuantLib::Following;
            QuantLib::DateGeneration::Rule rule = QuantLib::DateGeneration::CDS2015;
            bool endOfMonth = false;
            QuantLib::Real runningSpread = QuantLib::Null<QuantLib::Real>();
            QuantLib::BusinessDayConvention payConvention = QuantLib::Following;
            QuantLib::DayCounter dayCounter = QuantLib::Actual360(false);
            QuantLib::DayCounter lastPeriodDayCounter = QuantLib::Actual360(true);
            QuantLib::Natural cashSettlementDays = 3;
        };
        CreditCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& rateCurve =
                             QuantLib::Handle<QuantLib::YieldTermStructure>(),
                         const QuantLib::Handle<QuantLib::Quote>& recovery = QuantLib::Handle<QuantLib::Quote>(),
                         const RefData& refData = RefData());

        const RefData& refData() const;
        const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve() const;
        const QuantLib::Handle<QuantLib::YieldTermStructure>& rateCurve() const;
        const QuantLib::Handle<QuantLib::Quote>& recovery() const;
};
%template(CreditCurveHandle) Handle<CreditCurve>; 
%template(RelinkableCreditCurveHandle) RelinkableHandle<CreditCurve>;

%shared_ptr(CreditVolCurve)
class CreditVolCurve : public VolatilityTermStructure {
    private:
        CreditVolCurve();
    public:
        enum class Type { Price, Spread };
        CreditVolCurve(QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                       const std::vector<QuantLib::Period>& terms,
                       const std::vector<QuantLib::Handle<CreditCurve>>& termCurves, const Type& type);
        CreditVolCurve(const QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                       QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                       const std::vector<QuantLib::Period>& terms,
                       const std::vector<QuantLib::Handle<CreditCurve>>& termCurves, const Type& type);
        CreditVolCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                       QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                       const std::vector<QuantLib::Period>& terms,
                       const std::vector<QuantLib::Handle<CreditCurve>>& termCurves, const Type& type);

        QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Period& underlyingTerm,
                                  const QuantLib::Real strike, const Type& targetType) const;
        QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                                      const QuantLib::Real strike, const Type& targetType) const = 0;
        QuantLib::Real volatility(const QuantLib::Real exerciseTime, const QuantLib::Real underlyingLength,
                                  const QuantLib::Real strike, const Type& targetType) const;
        const Type& type() const;

        QuantLib::Real atmStrike(const QuantLib::Date& expiry, const QuantLib::Period& term) const;
        QuantLib::Real atmStrike(const QuantLib::Date& expiry, const QuantLib::Real underlyingLength) const;

        QuantLib::Real minStrike() const override;
        QuantLib::Real maxStrike() const override;
        QuantLib::Date maxDate() const override;
};

%shared_ptr(CreditVolCurveWrapper)
class CreditVolCurveWrapper : public CreditVolCurve {
    public:
        CreditVolCurveWrapper(const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol);
        QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                                  const QuantLib::Real strike, const Type& targetType) const override;
        const QuantLib::Date& referenceDate() const override;
};
%template(CreditVolCurveHandle) Handle<CreditVolCurve>;
%template(RelinkableVolCreditCurveHandle) RelinkableHandle<CreditVolCurve>;

%shared_ptr(BlackVolatilityWithATM)
class BlackVolatilityWithATM : public BlackVolTermStructure {
  public:
    BlackVolatilityWithATM(const ext::shared_ptr<QuantLib::BlackVolTermStructure>& surface,
                           const QuantLib::Handle<QuantLib::Quote>& spot,
                           const QuantLib::Handle<QuantLib::YieldTermStructure>& yield1,
                           const QuantLib::Handle<QuantLib::YieldTermStructure>& yield2);
    QuantLib::DayCounter dayCounter() const;
    QuantLib::Date maxDate() const;
    QuantLib::Time maxTime() const;
    const QuantLib::Date& referenceDate() const;
    QuantLib::Calendar calendar() const;
    QuantLib::Natural settlementDays() const;
    QuantLib::Rate minStrike() const;
    QuantLib::Rate maxStrike() const;
};

%shared_ptr(EquityAnnouncedDividendCurve)
class EquityAnnouncedDividendCurve : public QuantLib::TermStructure {

public:
    EquityAnnouncedDividendCurve(const QuantLib::Date& referenceDate, const std::set<QuantExt::Dividend>& dividends,
                                const Handle<YieldTermStructure>& discountCurve,
                                const QuantLib::Calendar& cal = QuantLib::Calendar(),
                                const QuantLib::DayCounter& dc = QuantLib::DayCounter());

    Date maxDate() const override;
    QuantLib::Real discountedFutureDividends(Time t);
};

#endif
