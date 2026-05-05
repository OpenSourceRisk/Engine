
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

namespace QuantExt {
enum class ShiftScheme { Forward, Backward, Central };
enum class ShiftType { Absolute, Relative };

ShiftScheme parseShiftScheme(const std::string& s);
ShiftType parseShiftType(const std::string& s);
}

%shared_ptr(QuantExt::PriceTermStructure)
namespace QuantExt {
class PriceTermStructure : public TermStructure {
  private:
    PriceTermStructure();
  public:
    QuantLib::Real price(QuantLib::Time t, bool extrapolate = false) const;
    QuantLib::Real price(const QuantLib::Date& d, bool extrapolate = false) const;
};
}

%{
typedef QuantExt::PriceTermStructure PriceTermStructure;
%}

typedef QuantExt::PriceTermStructure PriceTermStructure;

%template(PriceTermStructureHandle) Handle<QuantExt::PriceTermStructure>;
%template(RelinkablePriceTermStructureHandle) RelinkableHandle<QuantExt::PriceTermStructure>;

%define export_Interpolated_Price_Curve(Name, Interpolator)

%{
typedef QuantExt::InterpolatedPriceCurve<Interpolator> Name;
%}

%warnfilter(509) Name;

%shared_ptr(Name)
class Name : public QuantExt::PriceTermStructure {
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


%rename(QLESwaptionVolCube2) QuantExt::SwaptionVolCube2;
%shared_ptr(QuantExt::SwaptionVolCube2)
namespace QuantExt {
class SwaptionVolCube2 : public QuantLib::SwaptionVolatilityCube {
  public:
    SwaptionVolCube2(const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& atmVolStructure,
                     const std::vector<QuantLib::Period>& optionTenors,
                     const std::vector<QuantLib::Period>& swapTenors,
                     const std::vector<QuantLib::Spread>& strikeSpreads,
                     const std::vector<std::vector<QuantLib::Handle<QuantLib::Quote>>>& volSpreads,
                     const ext::shared_ptr<SwapIndex>& swapIndexBase,
                     const ext::shared_ptr<SwapIndex>& shortSwapIndexBase,
                        bool vegaWeightedSmileFit,
                        bool flatExtrapolation,
                        bool volsAreSpreads = true);
            QuantLib::DayCounter dayCounter() const;
};
}


%shared_ptr(QuantExt::FxBlackVannaVolgaVolatilitySurface)
namespace QuantExt {
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
}



%shared_ptr(QuantExt::BlackVarianceSurfaceMoneynessSpot)
namespace QuantExt {
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
}

%shared_ptr(QuantExt::BlackVarianceSurfaceMoneynessForward)
namespace QuantExt {
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
}


%shared_ptr(QuantExt::SwaptionVolCubeWithATM)
namespace QuantExt {
class SwaptionVolCubeWithATM : public SwaptionVolatilityStructure {
  public:
    SwaptionVolCubeWithATM(const ext::shared_ptr<QuantLib::SwaptionVolatilityCube>& cube);
  QuantLib::DayCounter dayCounter() const;
};
}

%inline %{
QuantExt::SwaptionVolCubeWithATM* qleMakeSwaptionVolCubeWithATM(const QuantExt::SwaptionVolCube2& cube) {
  return new QuantExt::SwaptionVolCubeWithATM(QuantLib::ext::make_shared<QuantExt::SwaptionVolCube2>(cube));
}
%}

%pythoncode %{
def _swaption_vol_cube_with_atm_init(self, *args):
  _ore_module = globals().get("_ORE", globals().get("_OREP"))
  if len(args) == 1:
    try:
      _ore_module.SwaptionVolCubeWithATM_swiginit(self, _ore_module.qleMakeSwaptionVolCubeWithATM(args[0]))
      return
    except TypeError:
      pass
  _ore_module.SwaptionVolCubeWithATM_swiginit(self, _ore_module.new_SwaptionVolCubeWithATM(*args))

SwaptionVolCubeWithATM.__init__ = _swaption_vol_cube_with_atm_init
%}

%shared_ptr(QuantExt::SwaptionVolatilityConstantSpread)
namespace QuantExt {
class SwaptionVolatilityConstantSpread : public SwaptionVolatilityStructure {
  public:
    SwaptionVolatilityConstantSpread(const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& atm,
                                     const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& cube);
    const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& atmVol();
    const QuantLib::Handle<QuantLib::SwaptionVolatilityStructure>& cube();
};
}

namespace QuantExt {
Real convertSwaptionVolatility(const Date& asof, const Period& optionTenor, const Period& swapTenor,
                               const ext::shared_ptr<SwapIndex>& swapIndexBase,
                               const ext::shared_ptr<SwapIndex>& shortSwapIndexBase, const DayCounter volDayCounter,
                               const Real strikeSpread, const Real inputVol, const QuantLib::VolatilityType inputType,
                               const Real inputShift, const QuantLib::VolatilityType outputType,
                               const Real outputShift);
}

%shared_ptr(QuantExt::CreditCurve)
namespace QuantExt {
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
    }
    %template(CreditCurveHandle) Handle<QuantExt::CreditCurve>;
    %template(RelinkableCreditCurveHandle) RelinkableHandle<QuantExt::CreditCurve>;

    %shared_ptr(QuantExt::CreditVolCurve)
    namespace QuantExt {
    class CreditVolCurve : public VolatilityTermStructure {
    private:
        CreditVolCurve();
    public:
        enum class Type { Price, Spread };
        CreditVolCurve(QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                       const std::vector<QuantLib::Period>& terms,
                 const std::vector<QuantLib::Handle<QuantExt::CreditCurve>>& termCurves, const Type& type);
        CreditVolCurve(const QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                       QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                       const std::vector<QuantLib::Period>& terms,
                 const std::vector<QuantLib::Handle<QuantExt::CreditCurve>>& termCurves, const Type& type);
        CreditVolCurve(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
                       QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
                       const std::vector<QuantLib::Period>& terms,
                 const std::vector<QuantLib::Handle<QuantExt::CreditCurve>>& termCurves, const Type& type);

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
      }

      %shared_ptr(QuantExt::CreditVolCurveWrapper)
      namespace QuantExt {
      class CreditVolCurveWrapper : public QuantExt::CreditVolCurve {
    public:
        CreditVolCurveWrapper(const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol);
        QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                                  const QuantLib::Real strike, const Type& targetType) const override;
        const QuantLib::Date& referenceDate() const override;
};
      }
      %template(CreditVolCurveHandle) Handle<QuantExt::CreditVolCurve>;
      %template(RelinkableVolCreditCurveHandle) RelinkableHandle<QuantExt::CreditVolCurve>;

%nodefaultctor QuantExt::ParametricVolatility;
namespace QuantExt {
class ParametricVolatility {
  public:
    enum class MarketQuoteType { Price, NormalVolatility, ShiftedLognormalVolatility };
    enum class ParameterCalibration { Fixed, Calibrated, Implied };
};
}

#if defined(SWIGPYTHON)
// Add nested-class-style access so Python code can write
//   ParametricVolatility.MarketQuoteType.ShiftedLognormalVolatility
// in addition to the SWIG default
//   ParametricVolatility.MarketQuoteType_ShiftedLognormalVolatility
%pythoncode %{
class _MarketQuoteTypeAccessor:
    Price                     = ParametricVolatility.MarketQuoteType_Price
    NormalVolatility          = ParametricVolatility.MarketQuoteType_NormalVolatility
    ShiftedLognormalVolatility = ParametricVolatility.MarketQuoteType_ShiftedLognormalVolatility
ParametricVolatility.MarketQuoteType = _MarketQuoteTypeAccessor

class _ParameterCalibrationAccessor:
    Fixed      = ParametricVolatility.ParameterCalibration_Fixed
    Calibrated = ParametricVolatility.ParameterCalibration_Calibrated
    Implied    = ParametricVolatility.ParameterCalibration_Implied
ParametricVolatility.ParameterCalibration = _ParameterCalibrationAccessor
%}
#endif

%template(OptionTypeVector)       std::vector<QuantLib::Option::Type>;
%template(OptionTypeVectorVector) std::vector<std::vector<QuantLib::Option::Type>>;

%shared_ptr(QuantExt::SviParametricVolatility)
%nodefaultctor QuantExt::SviParametricVolatility;
namespace QuantExt {
class SviParametricVolatility {
  public:
    enum class ModelVariant {
        Gatheral2004SviRaw = 0,
        Gatheral2004SviNatural = 1,
        Gatheral2004SviJw = 2,
        Gatheral2012SsviHeston = 3,
        Gatheral2012SsviPowerLaw = 4,
        HendriksMartini2017EssviFirstPowerLaw = 5,
        HendriksMartini2017EssviSecondPowerLaw = 6,
        CorbettaEtAl2019Essvi = 7,
        Mingone2022EssviGJ = 8,
        Mingone2022EssviMM = 9
    };
    QuantLib::Real globalVolRmseShiftedLognormal() const;
    QuantLib::Matrix volRmseShiftedLognormal() const;
};
}

%shared_ptr(QuantExt::CreditVolCurveSvi)
namespace QuantExt {
class CreditVolCurveSvi : public CreditVolCurve {
  public:
    CreditVolCurveSvi(
        const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal,
        QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dc,
        const std::vector<QuantLib::Period>& terms,
        const std::vector<QuantLib::Handle<CreditCurve>>& termCurves,
        const CreditVolCurve::Type& type,
        const std::vector<QuantLib::Date>& exerciseDates,
        const std::vector<QuantLib::Period>& underlyingTerms,
        const std::vector<std::vector<QuantLib::Real>>& strikes,
        const std::vector<std::vector<QuantLib::Real>>& vols,
        SviParametricVolatility::ModelVariant modelVariant,
        ParametricVolatility::MarketQuoteType inputMarketQuoteType =
            ParametricVolatility::MarketQuoteType::NormalVolatility,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve =
            QuantLib::Handle<QuantLib::YieldTermStructure>());
    QuantLib::Real volatility(const QuantLib::Date& exerciseDate, const QuantLib::Real underlyingLength,
                              const QuantLib::Real strike,
                              const CreditVolCurve::Type& targetType) const override;
};
  }

#if defined(SWIGPYTHON)
// Typemap: convert Python dict -> C++ std::map<pair<Real,Real>, vector<pair<Real,ParameterCalibration>>>
// Enables passing {} (empty) or {(lo,hi): [(init, calib), ...]} directly from Python.
%typemap(in) const std::map<std::pair<QuantLib::Real, QuantLib::Real>,
    std::vector<std::pair<QuantLib::Real, ParametricVolatility::ParameterCalibration>>>&
    (std::map<std::pair<QuantLib::Real, QuantLib::Real>,
     std::vector<std::pair<QuantLib::Real, ParametricVolatility::ParameterCalibration>>> _mp_temp) {
    if (!PyDict_Check($input)) {
        PyErr_SetString(PyExc_TypeError, "modelParameters must be a dict");
        SWIG_fail;
    }
    PyObject *_mp_key, *_mp_val;
    Py_ssize_t _mp_pos = 0;
    while (PyDict_Next($input, &_mp_pos, &_mp_key, &_mp_val)) {
        if (!PyTuple_Check(_mp_key) || PyTuple_GET_SIZE(_mp_key) != 2) {
            PyErr_SetString(PyExc_TypeError, "modelParameters key must be a (lo, hi) 2-tuple");
            SWIG_fail;
        }
        double _lo = PyFloat_AsDouble(PyTuple_GET_ITEM(_mp_key, 0));
        double _hi = PyFloat_AsDouble(PyTuple_GET_ITEM(_mp_key, 1));
        if (PyErr_Occurred()) SWIG_fail;
        if (!PySequence_Check(_mp_val)) {
            PyErr_SetString(PyExc_TypeError, "modelParameters value must be a sequence");
            SWIG_fail;
        }
        std::vector<std::pair<QuantLib::Real, ParametricVolatility::ParameterCalibration>> _params;
        Py_ssize_t _n = PySequence_Size(_mp_val);
        for (Py_ssize_t _i = 0; _i < _n; ++_i) {
            PyObject* _item = PySequence_GetItem(_mp_val, _i);
            if (!PyTuple_Check(_item) || PyTuple_GET_SIZE(_item) != 2) {
                Py_DECREF(_item);
                PyErr_SetString(PyExc_TypeError, "each entry must be a (initial, calibration) 2-tuple");
                SWIG_fail;
            }
            double _init = PyFloat_AsDouble(PyTuple_GET_ITEM(_item, 0));
            long   _cal  = PyLong_AsLong(PyTuple_GET_ITEM(_item, 1));
            Py_DECREF(_item);
            if (PyErr_Occurred()) SWIG_fail;
            _params.emplace_back(_init,
                static_cast<ParametricVolatility::ParameterCalibration>(_cal));
        }
        _mp_temp[{_lo, _hi}] = std::move(_params);
    }
    $1 = &_mp_temp;
}
%typemap(typecheck, precedence=4095) const std::map<std::pair<QuantLib::Real, QuantLib::Real>,
    std::vector<std::pair<QuantLib::Real, ParametricVolatility::ParameterCalibration>>>& {
    $1 = PyDict_Check($input) ? 1 : 0;
}
#endif

%shared_ptr(QuantExt::BlackVolatilitySurfaceSvi)
namespace QuantExt {
class BlackVolatilitySurfaceSvi : public BlackVolTermStructure {
  public:
    BlackVolatilitySurfaceSvi(
        const QuantLib::Date& referenceDate,
        const std::vector<QuantLib::Date>& dates,
        const std::vector<std::vector<QuantLib::Real>>& strikes,
        const std::vector<std::vector<QuantLib::Real>>& quotes,
        const QuantLib::DayCounter& dayCounter,
        const QuantLib::Calendar& calendar,
        const QuantLib::Handle<QuantLib::Quote>& spot,
        QuantLib::Size spotDays,
        const QuantLib::Calendar& spotCalendar,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& domesticTS,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& foreignTS,
        SviParametricVolatility::ModelVariant modelVariant,
        ParametricVolatility::MarketQuoteType inputMarketQuoteType =
            ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        const std::vector<std::vector<QuantLib::Option::Type>>& optionTypes = {},
        const std::map<std::pair<QuantLib::Real, QuantLib::Real>,
            std::vector<std::pair<QuantLib::Real, ParametricVolatility::ParameterCalibration>>>& modelParameters = {},
        QuantLib::Size maxCalibrationAttempts = 10,
        QuantLib::Real exitEarlyErrorThreshold = 0.005,
        QuantLib::Real maxAcceptableError = 0.05);
    QuantLib::Date maxDate() const;
    QuantLib::Rate minStrike() const;
    QuantLib::Rate maxStrike() const;
    %extend {
        // Return the underlying SviParametricVolatility (triggers calibration on first call).
      ext::shared_ptr<QuantExt::SviParametricVolatility> sviParametricVolatility() const {
        return QuantLib::ext::dynamic_pointer_cast<QuantExt::SviParametricVolatility>(
                self->parametricVolatility());
        }
    }
};
  }

  %shared_ptr(QuantExt::BlackVolatilityWithATM)
  namespace QuantExt {
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
}

  %shared_ptr(QuantExt::EquityAnnouncedDividendCurve)
  namespace QuantExt {
  class EquityAnnouncedDividendCurve : public QuantLib::TermStructure {

public:
    EquityAnnouncedDividendCurve(const QuantLib::Date& referenceDate, const std::set<QuantExt::Dividend>& dividends,
                                const Handle<YieldTermStructure>& discountCurve,
                                const QuantLib::Calendar& cal = QuantLib::Calendar(),
                                const QuantLib::DayCounter& dc = QuantLib::DayCounter());

    Date maxDate() const override;
    QuantLib::Real discountedFutureDividends(Time t);
};
  }

#endif
