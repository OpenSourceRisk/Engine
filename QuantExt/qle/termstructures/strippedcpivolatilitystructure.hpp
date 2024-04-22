/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file strippedcpivolatilitystructure.hpp
    \brief zero inflation volatility structure implied from a cpi cap/floor price surface
 */

#ifndef quantext_stripped_cpi_volatility_structure_hpp
#define quantext_stripped_cpi_volatility_structure_hpp

#include <iostream>
#include <ql/experimental/inflation/cpicapfloortermpricesurface.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/termstructures/inflation/constantcpivolatility.hpp>
#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {
using namespace QuantLib;

enum PriceQuotePreference { Cap, Floor, CapFloor };

struct StrippedCPIVolSurfaceDefaultValues {
    static constexpr QuantLib::Real upperVolBound = 1.0;
    static constexpr QuantLib::Real lowerVolBound = 0.000001;
    static constexpr QuantLib::Real solverTolerance = 1.0e-12;
};

//! Stripped zero inflation volatility structure
/*!
  The surface provides implied CPI Black volatilities for the union of strikes that occur in the underlying
  cap and floor price surface.

  The type argument determines which kind of price quotes are used with priority when there is an overlap,
  i.e. strikes for which we have both cap and floor quotes:
  If type is Cap: Use cap quotes where available, floor quotes otherwise
  If type is Floor: Use floor quotes where available, cap quotes otherwise
  If type is CapFloor: In case of overlap, use floor quotes up to the ATM strike, cap quotes for strikes beyond ATM
 */




template <class Interpolator2D> class StrippedCPIVolatilitySurface : public QuantExt::CPIVolatilitySurface {
public:
    
     
    QL_DEPRECATED StrippedCPIVolatilitySurface(PriceQuotePreference type,
                                 const QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface>& priceSurface,
                                 const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& zeroIndex,
                                 const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
                                 const QuantLib::Date& capFloorStartDate = Date(),
                                 const QuantLib::Real& upperVolBound = StrippedCPIVolSurfaceDefaultValues::upperVolBound,
                                 const QuantLib::Real& lowerVolBound = StrippedCPIVolSurfaceDefaultValues::lowerVolBound,
                                 const QuantLib::Real& solverTolerance = StrippedCPIVolSurfaceDefaultValues::solverTolerance,
                                 const Interpolator2D& interpolator2d = Interpolator2D(),
                                 const QuantLib::VolatilityType& volType = QuantLib::ShiftedLognormal, 
                                 const double displacement = 0.0);

    StrippedCPIVolatilitySurface(
        PriceQuotePreference type, const QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface>& priceSurface,
        const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& zeroIndex,
        const bool quotedPricesUseInterpolatedCPIFixings,
        const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine, const QuantLib::Date& capFloorStartDate = Date(),
        const QuantLib::Real& upperVolBound = StrippedCPIVolSurfaceDefaultValues::upperVolBound,
        const QuantLib::Real& lowerVolBound = StrippedCPIVolSurfaceDefaultValues::lowerVolBound,
        const QuantLib::Real& solverTolerance = StrippedCPIVolSurfaceDefaultValues::solverTolerance,
        const Interpolator2D& interpolator2d = Interpolator2D(),
        const QuantLib::VolatilityType& volType = QuantLib::ShiftedLognormal, const double displacement = 0.0);

    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

    //! \name Limits
    //@{
    //! the minimum strike for which the term structure can return vols
    QuantLib::Real minStrike() const override;
    //! the maximum strike for which the term structure can return vols
    QuantLib::Real maxStrike() const override;
    //! maximum date for which the term structure can return vols
    QuantLib::Date maxDate() const override;
    //@}

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Real>& strikes() { return strikes_; }
    const std::vector<QuantLib::Period>& maturities() { return maturities_; }
    const QuantLib::Matrix& volData() { return volData_; }
    //@}
    QuantLib::Real atmStrike(const QuantLib::Date& maturity,
                             const QuantLib::Period& obsLag = QuantLib::Period(-1, QuantLib::Days)) const override;

private:
    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;

    bool chooseFloor(QuantLib::Real strike, QuantLib::Real atmRate) const;

    class ObjectiveFunction {
    public:
        // Pass price to match, CPICapFloor engine, and all data for creating a quoted CPICapFloor
        ObjectiveFunction(QuantLib::Real priceToMatch, bool useFloor, QuantLib::Real strike, QuantLib::Date startDate,
                          QuantLib::Date maturityDate, QuantLib::Real baseCPI,
                          QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface> priceSurface,
                          const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& zeroIndex,
                          const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
                          const CPI::InterpolationType interpolationType);
        // Update the engine's volatility, return difference between capfloor NPV and priceToMatch
        QuantLib::Real operator()(QuantLib::Volatility guess) const;

    private:
        QuantLib::Real priceToMatch_;
        bool useFloor_;
        QuantLib::Real strike_;
        QuantLib::Date startDate_, maturityDate_;
        QuantLib::Real baseCPI_;
        QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface> priceSurface_;
        QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index_;
        QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine_;
        CPI::InterpolationType interpolationType_;
        QuantLib::Date lastAvailableFixingDate_;
        QuantLib::CPICapFloor cpiCapFloor_;

    };

    PriceQuotePreference preference_;
    QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface> priceSurface_;
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index_;
    QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine_;
    QuantLib::Real upperVolBound_;
    QuantLib::Real lowerVolBound_;
    QuantLib::Real solverTolerance_;

    mutable std::vector<QuantLib::Rate> strikes_;
    mutable std::vector<QuantLib::Period> maturities_;
    mutable std::vector<QuantLib::Time> maturityTimes_;
    mutable QuantLib::Matrix volData_;
    Interpolator2D interpolator2d_;
    mutable QuantLib::Interpolation2D vols_;
    bool computeTimeToExpiryFromLastAvailableFixingDate_;
};

template <class Interpolator2D>
StrippedCPIVolatilitySurface<Interpolator2D>::StrippedCPIVolatilitySurface(
    PriceQuotePreference type, const QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface>& priceSurface,
    const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index, const bool quotedPriceUseInterpolatedCPIFixing,
    const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine, // const QuantLib::Real& baseCPI,
    const QuantLib::Date& capFloorStartDate, const QuantLib::Real& upperVolBound, const QuantLib::Real& lowerVolBound,
    const QuantLib::Real& solverTolerance,
    const Interpolator2D& interpolator2d, const QuantLib::VolatilityType& volType,
    const double displacement)
    : QuantExt::CPIVolatilitySurface(priceSurface->settlementDays(), priceSurface->calendar(),
                                     priceSurface->businessDayConvention(), priceSurface->dayCounter(),
                                     priceSurface->observationLag(), index->frequency(),
                                     quotedPriceUseInterpolatedCPIFixing,
                                     capFloorStartDate, volType, displacement),
      preference_(type), priceSurface_(priceSurface), index_(index), engine_(engine), upperVolBound_(upperVolBound),
      lowerVolBound_(lowerVolBound), solverTolerance_(solverTolerance), interpolator2d_(interpolator2d) {

    performCalculations();
}

QL_DEPRECATED_DISABLE_WARNING
template <class Interpolator2D>
StrippedCPIVolatilitySurface<Interpolator2D>::StrippedCPIVolatilitySurface(
    PriceQuotePreference type, const QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface>& priceSurface,
    const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& index,
    const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine, // const QuantLib::Real& baseCPI,
    const QuantLib::Date& capFloorStartDate, const QuantLib::Real& upperVolBound, const QuantLib::Real& lowerVolBound,
    const QuantLib::Real& solverTolerance, const Interpolator2D& interpolator2d,
    const QuantLib::VolatilityType& volType, const double displacement)
    : StrippedCPIVolatilitySurface<Interpolator2D>::StrippedCPIVolatilitySurface(
          type, priceSurface, index, index->interpolated(), engine, capFloorStartDate, upperVolBound, lowerVolBound,
          solverTolerance, interpolator2d, volType, displacement) {}
QL_DEPRECATED_ENABLE_WARNING

template <class Interpolator2D> void StrippedCPIVolatilitySurface<Interpolator2D>::performCalculations() const {
    strikes_ = priceSurface_->strikes();
    maturities_ = priceSurface_->maturities();
    volData_ = QuantLib::Matrix(strikes_.size(), maturities_.size(), QuantLib::Null<QuantLib::Real>());
    QuantLib::Brent solver;
    QuantLib::Real guess = (upperVolBound_ + lowerVolBound_) / 2.0;
    QuantLib::Date startDate = capFloorStartDate();
    QuantLib::Date underlyingBaseDate =
        ZeroInflation::fixingDate(startDate, observationLag(), frequency(), indexIsInterpolated());
    double baseCPI = ZeroInflation::cpiFixing(index_, startDate, observationLag(), indexIsInterpolated());
    for (QuantLib::Size i = 0; i < strikes_.size(); i++) {
        for (QuantLib::Size j = 0; j < maturities_.size(); j++) {

            QuantLib::Date maturityDate = optionDateFromTenor(maturities_[j]);
            QuantLib::Date fixDate =
                ZeroInflation::fixingDate(maturityDate, observationLag(), frequency(), indexIsInterpolated());
            Rate I1 = ZeroInflation::cpiFixing(index_, maturityDate, observationLag(), indexIsInterpolated());

            Time timeToMaturity = dayCounter().yearFraction(underlyingBaseDate, fixDate);
            QuantLib::Real atmRate = std::pow(I1 / baseCPI, 1 / timeToMaturity) - 1.0;

            bool useFloor = chooseFloor(strikes_[i], atmRate);

            // FIXME: Do we need an effective maturity here ?
            QuantLib::Real priceToMatch = useFloor ? priceSurface_->floorPrice(maturities_[j], strikes_[i])
                                                   : priceSurface_->capPrice(maturities_[j], strikes_[i]);
            try {
                auto interpolationType =
                    indexIsInterpolated() ? CPI::InterpolationType::Linear : CPI::InterpolationType::Flat;
                ObjectiveFunction func(priceToMatch, useFloor, strikes_[i], startDate, maturityDate, baseCPI,
                                       priceSurface_, index_, engine_, interpolationType);
                QuantLib::Real found = solver.solve(func, solverTolerance_, guess, lowerVolBound_, upperVolBound_);
                volData_[i][j] = found;
            } catch (std::exception& e) {
                QL_FAIL("failed to find implied vol for "
                        << (useFloor ? "Floor" : "Cap") << " with strike " << strikes_[i] << " and maturity "
                        << maturities_[j] << ", because: " << e.what() << " "
                        << QuantLib::io::iso_date(startDate + maturities_[j]) << " " << maturityDate);
            }
        }
    }

    maturityTimes_.clear();
    for (QuantLib::Size i = 0; i < maturities_.size(); i++) {
        QuantLib::Date d = optionDateFromTenor(maturities_[i]);
        maturityTimes_.push_back(fixingTime(d));
    }

    vols_ = interpolator2d_.interpolate(maturityTimes_.begin(), maturityTimes_.end(), strikes_.begin(), strikes_.end(),
                                        volData_);
    vols_.enableExtrapolation();
}

template <class Interpolator2D>
bool StrippedCPIVolatilitySurface<Interpolator2D>::chooseFloor(QuantLib::Real strike, QuantLib::Real atmRate) const {
    if (preference_ == Floor) {
        if (strike <= priceSurface_->floorStrikes().back())
            return true;
        else
            return false;
    }

    if (preference_ == Cap) {
        if (strike < priceSurface_->capStrikes().front())
            return true;
        else
            return false;
    }

    // else: Use floors where we have floor quotes only, caps where we have cap quotes only,
    // and decide based on ATM where we have both cap and floor quotes

    // 1) strike < maxFloorStrike < minCapStrike: Floor!
    if (strike <= priceSurface_->floorStrikes().back() && strike < priceSurface_->capStrikes().front())
        return true;
    // 2) strike > maxFloorStrike and strike >= minCapStrike: Cap!
    else if (strike > priceSurface_->floorStrikes().back() && strike >= priceSurface_->capStrikes().front())
        return false;
    // 3) Overlap, maxFloorStrike > minCapStrike and strike in between: Depends on atmRate which surface we pick
    else if (strike <= priceSurface_->floorStrikes().back() && strike >= priceSurface_->capStrikes().front()) {
        // we have overlapping strikes, decide depending on atm level
        if (strike < atmRate)
            return true;
        else
            return false;
        // 4) Gap, maxFloorStrike < minCapStrike and strike in the gap: Depends on atmRate which surface we
        // extrapolate
    } else if (strike > priceSurface_->floorStrikes().back() && strike < priceSurface_->capStrikes().front()) {
        // there is a gap between floor end and caps begin, decide again depending on strike level
        if (strike < atmRate)
            return true;
        else
            return false;
    } else {
        QL_FAIL("case not covered in  StrippedCPIVolatilitySurface: strike="
                << strike << " maxFloorStrike=" << priceSurface_->floorStrikes().back()
                << " minCapStrike=" << priceSurface_->capStrikes().front() << " atm=" << atmRate);
        return false;
    }
}

template <class Interpolator2D> QuantLib::Real StrippedCPIVolatilitySurface<Interpolator2D>::minStrike() const {
    return strikes_.front() - QL_EPSILON;
}

template <class Interpolator2D> QuantLib::Real StrippedCPIVolatilitySurface<Interpolator2D>::maxStrike() const {
    return strikes_.back() + QL_EPSILON;
}

template <class Interpolator2D> QuantLib::Date StrippedCPIVolatilitySurface<Interpolator2D>::maxDate() const {
    QuantLib::Date today = QuantLib::Settings::instance().evaluationDate();
    return today + maturities_.back();
}

template <class Interpolator2D>
QuantLib::Volatility StrippedCPIVolatilitySurface<Interpolator2D>::volatilityImpl(QuantLib::Time length,
                                                                                  QuantLib::Rate strike) const {
    return vols_(length, strike);
}

template <class Interpolator2D>
StrippedCPIVolatilitySurface<Interpolator2D>::ObjectiveFunction::ObjectiveFunction(
    QuantLib::Real priceToMatch, bool useFloor, QuantLib::Real strike, QuantLib::Date startDate,
    QuantLib::Date maturityDate, QuantLib::Real baseCPI,
    QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface> priceSurface,
    const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex>& zeroIndex,
    const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine, const CPI::InterpolationType interpolationType)
    : priceToMatch_(priceToMatch), useFloor_(useFloor), strike_(strike), startDate_(startDate),
      maturityDate_(maturityDate), baseCPI_(baseCPI), priceSurface_(priceSurface), index_(zeroIndex), engine_(engine),
      interpolationType_(interpolationType),
      cpiCapFloor_(QuantLib::CPICapFloor(useFloor_ ? QuantLib::Option::Put : QuantLib::Option::Call,
                                         1.0, // unit nominal, because the price surface returns unit nominal prices
                                         startDate_, baseCPI_, maturityDate_, priceSurface_->calendar(),
                                         priceSurface_->businessDayConvention(), priceSurface_->calendar(),
                                         priceSurface_->businessDayConvention(), strike_, index_,
                                         priceSurface_->observationLag(), interpolationType)) {

    // FIXME: observation interpolation (last argument) uses default setting here CPI::AsIndex

    cpiCapFloor_.setPricingEngine(engine_);
}

template <class Interpolator2D>
QuantLib::Real
StrippedCPIVolatilitySurface<Interpolator2D>::ObjectiveFunction::operator()(QuantLib::Volatility guess) const {
    
    QL_DEPRECATED_DISABLE_WARNING
    bool isInterpolated = interpolationType_ == CPI::InterpolationType::Linear ||
                          (interpolationType_ == CPI::InterpolationType::AsIndex && index_->interpolated());
    QL_DEPRECATED_ENABLE_WARNING

    QuantLib::ext::shared_ptr<QuantExt::ConstantCPIVolatility> vol = QuantLib::ext::make_shared<QuantExt::ConstantCPIVolatility>(
        guess, priceSurface_->settlementDays(), priceSurface_->calendar(), priceSurface_->businessDayConvention(),
        priceSurface_->dayCounter(), priceSurface_->observationLag(), priceSurface_->frequency(), isInterpolated,
        startDate_);

    engine_->setVolatility(QuantLib::Handle<QuantLib::CPIVolatilitySurface>(vol));

    QuantLib::Real npv = cpiCapFloor_.NPV();
    return priceToMatch_ - npv;
}

template <class Interpolator2D>
QuantLib::Real StrippedCPIVolatilitySurface<Interpolator2D>::atmStrike(const QuantLib::Date& maturity,
                                                                           const QuantLib::Period& obsLag) const {
    QuantLib::Period lag = obsLag == -1 * QuantLib::Days ? observationLag() : obsLag;
    QuantLib::Date fixingDate = ZeroInflation::fixingDate(maturity, lag, frequency(), indexIsInterpolated());
    double forwardCPI = ZeroInflation::cpiFixing(index_, maturity, lag, indexIsInterpolated());
    double baseCPI = ZeroInflation::cpiFixing(index_, capFloorStartDate(), observationLag(), indexIsInterpolated());
    double atm = forwardCPI / baseCPI;
    double ttm =
        QuantLib::inflationYearFraction(frequency(), indexIsInterpolated(), dayCounter(), baseDate(), fixingDate);
    return std::pow(atm, 1.0 / ttm) - 1.0;
}

} // namespace QuantExt

#endif
