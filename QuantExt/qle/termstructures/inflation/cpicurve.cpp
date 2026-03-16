
#include <iostream>
#include <ql/math/comparison.hpp>
#include <qle/termstructures/inflation/cpicurve.hpp>

namespace QuantExt {

using QuantLib::Calendar;
using QuantLib::close_enough;
using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::Frequency;
using QuantLib::Natural;
using QuantLib::Null;
using QuantLib::Period;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Seasonality;
using QuantLib::Time;
using QuantLib::ZeroInflationTermStructure;

CPICurve::CPICurve(Date baseDate, Real baseCPI, const Period& observationLag, Frequency frequency,
                   const DayCounter& dayCounter, const QuantLib::ext::shared_ptr<Seasonality>& seasonality)
    : ZeroInflationTermStructure(baseDate, observationLag, frequency, dayCounter, seasonality), baseCPI_(baseCPI) {
    check();
}

CPICurve::CPICurve(const Date& referenceDate, Date baseDate, Real baseCPI, const Period& observationLag,
                   Frequency frequency, const DayCounter& dayCounter,
                   const QuantLib::ext::shared_ptr<Seasonality>& seasonality)
    : ZeroInflationTermStructure(referenceDate, baseDate, observationLag, frequency, dayCounter, seasonality),
      baseCPI_(baseCPI) {
    check();
}

CPICurve::CPICurve(Natural settlementDays, const Calendar& calendar, Date baseDate, Real baseCPI,
                   const Period& observationLag, Frequency frequency, const DayCounter& dayCounter,
                   const QuantLib::ext::shared_ptr<Seasonality>& seasonality)
    : ZeroInflationTermStructure(settlementDays, calendar, baseDate, observationLag, frequency, dayCounter,
                                 seasonality),
      baseCPI_(baseCPI) {
    check();
}

Real CPICurve::CPI(const Date& d, bool extrapolate) const {
    std::pair<Date, Date> dd = inflationPeriod(d, frequency());
    InflationTermStructure::checkRange(dd.first, extrapolate);
    Time t = timeFromReference(dd.first);
    Real cpi = forwardCPIImpl(t);
    if (hasSeasonality()) {
        cpi *= seasonality()->seasonalityFactor(d) / seasonality()->seasonalityFactor(baseDate());
    }
    return cpi;
}

Rate CPICurve::zeroRateImpl(Time t) const {
    auto tb = timeFromReference(baseDate());
    if (t <= tb || close_enough(t, tb)) {
        return Null<Real>();
    }
    Rate cpi = forwardCPIImpl(t);
    Rate baseCPI = baseCPI_;
    return std::pow(cpi / baseCPI, 1.0 / (t - tb)) - 1.0;
}

void CPICurve::check() const {
    QL_REQUIRE(baseCPI_ >= 0 && !close_enough(baseCPI_, 0.0), "Base CPI must be greater than 0");
}

} // namespace QuantExt
