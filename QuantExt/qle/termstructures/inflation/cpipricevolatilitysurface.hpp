/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#pragma once

#include <iostream>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/termstructures/inflation/constantcpivolatility.hpp>
#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>
#include <qle/termstructures/strippedcpivolatilitystructure.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

struct CPIPriceVolatilitySurfaceDefaultValues {
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
  If type is CapFloor: In case of overlap, use floor quotes up to the ATM strike, cap quotes for strikes beyond
  ATM
 */

template <class InterpolatorStrike, class InterpolatorTime>
class CPIPriceVolatilitySurface : public QuantExt::CPIVolatilitySurface, public QuantLib::LazyObject {
public:
    CPIPriceVolatilitySurface(
        PriceQuotePreference type, const QuantLib::Period& observationLag,
        const QuantLib::Calendar& cal, // calendar in index may not be useful
        const QuantLib::BusinessDayConvention& bdc, const QuantLib::DayCounter& dc,
        const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index, 
        QuantLib::Handle<QuantLib::YieldTermStructure> yts,
        const std::vector<QuantLib::Rate>& cStrikes, const std::vector<QuantLib::Rate>& fStrikes,
        const std::vector<QuantLib::Period>& cfMaturities, const QuantLib::Matrix& cPrice,
        const QuantLib::Matrix& fPrice, const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
        const bool quotedInstrumentsAreInterpolated = false,
        const QuantLib::Date& capFloorStartDate = QuantLib::Date(), 
        bool ignoreMissingPrices = false, // if true, it allows prices to be Null and work as long there is one
        bool lowerStrikeConstExtrap = true, bool upperStrikeConstExtrap = true,
        const QuantLib::VolatilityType& volType = QuantLib::ShiftedLognormal, const double displacement = 0.0,
        const QuantLib::Real& upperVolBound = CPIPriceVolatilitySurfaceDefaultValues::upperVolBound,
        const QuantLib::Real& lowerVolBound = CPIPriceVolatilitySurfaceDefaultValues::lowerVolBound,
        const QuantLib::Real& solverTolerance = CPIPriceVolatilitySurfaceDefaultValues::solverTolerance);


    QL_DEPRECATED CPIPriceVolatilitySurface(
        PriceQuotePreference type,
        const QuantLib::Period& observationLag,
        const QuantLib::Calendar& cal, // calendar in index may not be useful
        const QuantLib::BusinessDayConvention& bdc, 
        const QuantLib::DayCounter& dc,
        const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index, 
        QuantLib::Handle<QuantLib::YieldTermStructure> yts,
        const std::vector<QuantLib::Rate>& cStrikes, const std::vector<QuantLib::Rate>& fStrikes,
        const std::vector<QuantLib::Period>& cfMaturities, const QuantLib::Matrix& cPrice,
        const QuantLib::Matrix& fPrice,
        const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
        const QuantLib::Date& capFloorStartDate = QuantLib::Date(),
        bool ignoreMissingPrices = false, // if true, it allows prices to be Null and work as long there is one
        bool lowerStrikeConstExtrap = true, bool upperStrikeConstExtrap = true,
        const QuantLib::VolatilityType& volType = QuantLib::ShiftedLognormal, const double displacement = 0.0,
        const QuantLib::Real& upperVolBound = CPIPriceVolatilitySurfaceDefaultValues::upperVolBound,
        const QuantLib::Real& lowerVolBound = CPIPriceVolatilitySurfaceDefaultValues::lowerVolBound,
        const QuantLib::Real& solverTolerance = CPIPriceVolatilitySurfaceDefaultValues::solverTolerance);

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    void update() override {
        CPIVolatilitySurface::update();
        QuantLib::LazyObject::update();
    }

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
    //! Returns the strikes
    const std::vector<QuantLib::Real>& strikes() { return strikes_; }

    //! Returns the tenors
    const std::vector<QuantLib::Period>& maturities() { return expiries_; }

    const QuantLib::Matrix& volData() const {
        calculate();
        return volData_;
    }

    const std::vector<std::vector<bool>>& missingValues() const { 
        calculate();
        return missingPrices_;
    }

    const std::vector<std::vector<bool>>& pricesFailedToConvert() const { 
        calculate();
        return failedPrices_;
    }

    //@}

protected:
    //! CPI fixing on the baseDate of the surface
    double baseCPI() const;

    double atmGrowth(QuantLib::Period& tenor) const;

    double atmGrowth(const QuantLib::Date& date) const;

    QuantLib::Real atmStrike(const QuantLib::Date& maturity,
                             const QuantLib::Period& obsLag = QuantLib::Period(-1, QuantLib::Days)) const override;

private:
    virtual void validateInputParameters() const;

    // unique union of capStrikes and floorStrikes
    virtual void initializeStrikes() const;

    //! Computes a cap price from a floor price using the put-call parity and vice-versa
    double priceFromPutCallParity(double price, bool isCapPrice, double atm, double strikeGrowth, double df) const;

    //! Returns floor price for strike level (average annual inflation) and maturity index
    double floorPrice(double strike, size_t tenorIdx, double atm, double strikeGrowth, double df) const;

    //! Returns cap price for strike level (average annual inflation) and maturity index
    double capPrice(double strike, size_t tenorIdx, double atm, double strikeGrowth, double df) const;

    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override;

    // prefer cap or floor quote
    bool chooseFloor(QuantLib::Real strike, QuantLib::Real atmRate) const;

    // imply the black / bachelier vol from the cap/floor price using brent solver
    double implyVol(double strike, const QuantLib::Date& maturity, double price, bool isFloor) const;

    PriceQuotePreference preference_;
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index_;
    QuantLib::Handle<QuantLib::YieldTermStructure> yts_;
    std::vector<double> capStrikes_;
    std::vector<double> floorStrikes_;

    QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine_;
    bool ignoreMissingPrices_;
    bool lowerStrikeConstExtrap_;
    bool upperStrikeConstExtrap_;
    QuantLib::Real upperVolBound_;
    QuantLib::Real lowerVolBound_;
    QuantLib::Real solverTolerance_;

    std::vector<QuantLib::Period> expiries_;
    mutable std::vector<QuantLib::Rate> strikes_;

    QuantLib::Matrix capPrices_;
    QuantLib::Matrix floorPrices_;
    mutable std::vector<QuantLib::Date> fixingDates_;
    mutable QuantLib::Matrix volData_;
    mutable std::vector<std::vector<bool>> missingPrices_;
    mutable std::vector<std::vector<bool>> failedPrices_;

    mutable QuantLib::ext::shared_ptr<QuantExt::OptionInterpolator2d<InterpolatorStrike, InterpolatorTime>> volSurface_;
};



template <class InterpolatorStrike, class InterpolatorTime>
CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::CPIPriceVolatilitySurface(
    PriceQuotePreference type, const QuantLib::Period& observationLag,
    const QuantLib::Calendar& cal, // calendar in index may not be useful
    const QuantLib::BusinessDayConvention& bdc, const QuantLib::DayCounter& dc,
    const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index, QuantLib::Handle<QuantLib::YieldTermStructure> yts,
    const std::vector<QuantLib::Rate>& cStrikes, const std::vector<QuantLib::Rate>& fStrikes,
    const std::vector<QuantLib::Period>& cfMaturities, const QuantLib::Matrix& cPrice, const QuantLib::Matrix& fPrice,
    const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine, const bool quotedInstrumentsAreInterpolated,
    const QuantLib::Date& capFloorStartDate,
    bool ignoreMissingPrices, // if true, it allows prices to be Null and work as long there is one
    bool lowerStrikeConstExtrap, bool upperStrikeConstExtrap, const QuantLib::VolatilityType& volType,
    const double displacement, const QuantLib::Real& upperVolBound, const QuantLib::Real& lowerVolBound,
    const QuantLib::Real& solverTolerance)
    : QuantExt::CPIVolatilitySurface(0, cal, bdc, dc, observationLag, index->frequency(), quotedInstrumentsAreInterpolated,
                                     capFloorStartDate, volType, displacement),
      preference_(type), index_(index), yts_(yts), capStrikes_(cStrikes), floorStrikes_(fStrikes), engine_(engine),
      ignoreMissingPrices_(ignoreMissingPrices), lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
      upperStrikeConstExtrap_(upperStrikeConstExtrap), upperVolBound_(upperVolBound), lowerVolBound_(lowerVolBound),
      solverTolerance_(solverTolerance), expiries_(cfMaturities), capPrices_(cPrice), floorPrices_(fPrice) {
    validateInputParameters();
    initializeStrikes();
    QL_REQUIRE(!yts.empty(), "DiscountCurve not provided");
    QL_REQUIRE(engine, "PricingEngine not provided");
    QL_REQUIRE(index, "PricingEngine not provided");
    registerWith(index_);
    registerWith(yts_);
}

QL_DEPRECATED_DISABLE_WARNING
template <class InterpolatorStrike, class InterpolatorTime>
CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::CPIPriceVolatilitySurface(
    PriceQuotePreference type, 
    const QuantLib::Period& observationLag,
    const QuantLib::Calendar& cal, // calendar in index may not be useful
    const QuantLib::BusinessDayConvention& bdc, 
    const QuantLib::DayCounter& dc,
    const QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> index, 
    QuantLib::Handle<QuantLib::YieldTermStructure> yts,
    const std::vector<QuantLib::Rate>& cStrikes, const std::vector<QuantLib::Rate>& fStrikes,
    const std::vector<QuantLib::Period>& cfMaturities, const QuantLib::Matrix& cPrice, const QuantLib::Matrix& fPrice,
    const QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
    const QuantLib::Date& capFloorStartDate,
    bool ignoreMissingPrices, // if true, it allows prices to be Null and work as long there is one
    bool lowerStrikeConstExtrap, bool upperStrikeConstExtrap, 
    const QuantLib::VolatilityType& volType, const double displacement, const QuantLib::Real& upperVolBound,
    const QuantLib::Real& lowerVolBound, const QuantLib::Real& solverTolerance)
    : QuantExt::CPIVolatilitySurface(0, cal, bdc, dc, observationLag, index->frequency(), index->interpolated(),
                                     capFloorStartDate, volType, displacement),
      preference_(type), index_(index), yts_(yts), capStrikes_(cStrikes), floorStrikes_(fStrikes), engine_(engine),
      ignoreMissingPrices_(ignoreMissingPrices), lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
      upperStrikeConstExtrap_(upperStrikeConstExtrap), upperVolBound_(upperVolBound), lowerVolBound_(lowerVolBound),
      solverTolerance_(solverTolerance), expiries_(cfMaturities), capPrices_(cPrice), floorPrices_(fPrice) {
    validateInputParameters();
    initializeStrikes();
    QL_REQUIRE(!yts.empty(), "DiscountCurve not provided");
    QL_REQUIRE(engine, "PricingEngine not provided");
    QL_REQUIRE(index, "PricingEngine not provided");
    registerWith(index_);
    registerWith(yts_);
}
QL_DEPRECATED_ENABLE_WARNING

template <class InterpolatorStrike, class InterpolatorTime>
void CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::performCalculations() const {
    volData_ = QuantLib::Matrix(strikes_.size(), expiries_.size(), QuantLib::Null<QuantLib::Real>());
    missingPrices_ = std::vector<std::vector<bool>>(strikes_.size(), std::vector<bool>(expiries_.size(), false));
    failedPrices_ = std::vector<std::vector<bool>>(strikes_.size(), std::vector<bool>(expiries_.size(), false));


    std::vector<QuantLib::Date> dates;
    std::vector<double> strikes;
    std::vector<double> vols;

    for (QuantLib::Size tenorIdx = 0; tenorIdx < expiries_.size(); tenorIdx++) {
        QuantLib::Date maturityDate = optionDateFromTenor(expiries_[tenorIdx]);
        QuantLib::Date fixingDate =
            ZeroInflation::fixingDate(maturityDate, observationLag(), frequency(), indexIsInterpolated());
        double atm = atmGrowth(maturityDate);
        double df = yts_->discount(fixingDate);
        double ttm =
            QuantLib::inflationYearFraction(frequency(), indexIsInterpolated(), dayCounter(), baseDate(), fixingDate);
        fixingDates_.push_back(fixingDate);
        double atmAvgRate = std::pow(atm, 1.0 / ttm) - 1.0;
        for (QuantLib::Size strikeIdx = 0; strikeIdx < strikes_.size(); strikeIdx++) {
            double strike = strikes_[strikeIdx];
            double strikeGrowth = std::pow(1.0 + strike, ttm);
            bool useFloor = chooseFloor(strike, atmAvgRate);
            double vol = QuantLib::Null<QuantLib::Real>();
            QuantLib::Real priceToMatch = useFloor ? floorPrice(strike, tenorIdx, atm, strikeGrowth, df)
                                                   : capPrice(strike, tenorIdx, atm, strikeGrowth, df);
            if (priceToMatch != QuantLib::Null<QuantLib::Real>()) {
                try {
                    vol = implyVol(strike, maturityDate, priceToMatch, useFloor);
                } catch (const std::exception&) {
                    // implied failed, we try to interpolate the failed values
                    vol = QuantLib::Null<QuantLib::Real>();
                    failedPrices_[strikeIdx][tenorIdx] = true;
                }
            } else {
                missingPrices_[strikeIdx][tenorIdx] = true;
                QL_REQUIRE(ignoreMissingPrices_, "Missing price for cpi capfloor vol for tenor "
                                                     << expiries_[tenorIdx] << " and strike " << strike);
            }
            if (vol != QuantLib::Null<QuantLib::Real>()) {
                dates.push_back(fixingDate);
                strikes.push_back(strike);
                vols.push_back(vol);
            }
        }
    }

    volSurface_ = QuantLib::ext::make_shared<QuantExt::OptionInterpolator2d<InterpolatorStrike, InterpolatorTime>>(
        referenceDate(), dayCounter(), dates, strikes, vols, lowerStrikeConstExtrap_, upperStrikeConstExtrap_,
        InterpolatorStrike(), InterpolatorTime(), baseDate());

    for (QuantLib::Size strikeIdx = 0; strikeIdx < strikes_.size(); strikeIdx++) {
        for (QuantLib::Size tenorIdx = 0; tenorIdx < expiries_.size(); tenorIdx++) {
            volData_[strikeIdx][tenorIdx] = volSurface_->getValue(fixingDates_[tenorIdx], strikes_[strikeIdx]);
        }
    }
}

template <class InterpolatorStrike, class InterpolatorTime>
bool CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::chooseFloor(QuantLib::Real strike,
                                                                                  QuantLib::Real atmRate) const {
    if (floorStrikes_.empty()) {
        return false;
    }

    if (capStrikes_.empty()) {
        return true;
    }

    if (preference_ == Floor) {
        if (strike <= floorStrikes_.back())
            return true;
        else
            return false;
    }

    if (preference_ == Cap) {
        if (strike < capStrikes_.front())
            return true;
        else
            return false;
    }

    // else: Use floors where we have floor quotes only, caps where we have cap quotes only,
    // and decide based on ATM where we have both cap and floor quotes

    // 1) strike < maxFloorStrike < minCapStrike: Floor!
    if (strike <= floorStrikes_.back() && strike < capStrikes_.front())
        return true;
    // 2) strike > maxFloorStrike and strike >= minCapStrike: Cap!
    else if (strike > floorStrikes_.back() && strike >= capStrikes_.front())
        return false;
    // 3) Overlap, maxFloorStrike > minCapStrike and strike in between: Depends on atmRate which surface we pick
    else if (strike <= floorStrikes_.back() && strike >= capStrikes_.front()) {
        // we have overlapping strikes, decide depending on atm level
        if (strike < atmRate)
            return true;
        else
            return false;
        // 4) Gap, maxFloorStrike < minCapStrike and strike in the gap: Depends on atmRate which surface we
        // extrapolate
    } else if (strike > floorStrikes_.back() && strike < capStrikes_.front()) {
        // there is a gap between floor end and caps begin, decide again depending on strike level
        if (strike < atmRate)
            return true;
        else
            return false;
    } else {
        QL_FAIL("case not covered in  StrippedCPIVolatilitySurface: strike="
                << strike << " maxFloorStrike=" << floorStrikes_.back() << " minCapStrike=" << capStrikes_.front()
                << " atm=" << atmRate);
        return false;
    }
}

template <class InterpolatorStrike, class InterpolatorTime>
QuantLib::Real CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::minStrike() const {
    return strikes_.front() - QL_EPSILON;
}

template <class InterpolatorStrike, class InterpolatorTime>
QuantLib::Real CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::maxStrike() const {
    return strikes_.back() + QL_EPSILON;
}

template <class InterpolatorStrike, class InterpolatorTime>
QuantLib::Date CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::maxDate() const {
    return optionDateFromTenor(expiries_.back());
}

template <class InterpolatorStrike, class InterpolatorTime>
QuantLib::Real
CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::volatilityImpl(QuantLib::Time length,
                                                                                QuantLib::Rate strike) const {
    calculate();
    return volSurface_->getValue(length, strike);
}

template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::baseCPI() const {
    return ZeroInflation::cpiFixing(index_, capFloorStartDate(), observationLag(), indexIsInterpolated());
}

template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::atmGrowth(QuantLib::Period& tenor) const {
    return atmGrowth(optionDateFromTenor(tenor));
}

template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::atmGrowth(const QuantLib::Date& date) const {
    return ZeroInflation::cpiFixing(index_, date, observationLag(), indexIsInterpolated()) / baseCPI();
}

template <class InterpolatorStrike, class InterpolatorTime>
void CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::validateInputParameters() const {
    QL_REQUIRE(!expiries_.empty(), "Need at least one tenor");
    QL_REQUIRE(!floorStrikes_.empty() || !capStrikes_.empty(), "cap and floor strikes can not be both empty");
    QL_REQUIRE(capPrices_.rows() == capStrikes_.size() && capPrices_.columns() == (capStrikes_.empty() ? 0 : expiries_.size()),
               "mismatch between cap price matrix dimension and number of strikes and tenors");
    QL_REQUIRE(floorPrices_.rows() == floorStrikes_.size() &&
                   floorPrices_.columns() == (floorStrikes_.empty() ? 0 : expiries_.size()),
               "mismatch between cap price matrix dimension and number of strikes and tenors");

    // Some basic arbitrage checks
    for (size_t tenorIdx = 0; tenorIdx < capPrices_.columns(); ++tenorIdx) {
        double prevPrice = std::numeric_limits<double>::max();
        for (size_t strikeIdx = 0; strikeIdx < capPrices_.rows(); ++strikeIdx) {
            double currentPrice = capPrices_[strikeIdx][tenorIdx];
            QL_REQUIRE(ignoreMissingPrices_ || currentPrice != QuantLib::Null<QuantLib::Real>(),
                       "Input prices can not be null");
            QL_REQUIRE(!QuantLib::close_enough(0.0, currentPrice) && currentPrice > 0.0, "No zero cap prices allowd");
            if (currentPrice != QuantLib::Null<Real>()) {
                QL_REQUIRE(currentPrice <= prevPrice, "Non decreasing cap prices");
                prevPrice = currentPrice;
            }
        }
    }

    for (size_t tenorIdx = 0; tenorIdx < floorPrices_.columns(); ++tenorIdx) {
        double prevPrice = QL_EPSILON;
        for (size_t strikeIdx = 0; strikeIdx < floorPrices_.rows(); ++strikeIdx) {
            double currentPrice = floorPrices_[strikeIdx][tenorIdx];
            QL_REQUIRE(ignoreMissingPrices_ || currentPrice != QuantLib::Null<QuantLib::Real>(),
                       "Input prices can not be null");
            QL_REQUIRE(!QuantLib::close_enough(0.0, currentPrice) && currentPrice > 0.0, "No zero cap prices allowd");
            if (currentPrice != QuantLib::Null<Real>()) {
                QL_REQUIRE(currentPrice >= prevPrice, "Non increasing floor prices");
                prevPrice = currentPrice;
            }
        }
    }
}

template <class InterpolatorStrike, class InterpolatorTime>
void CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::initializeStrikes() const {
    strikes_.clear();
    std::set<double> uniqueStrikes(floorStrikes_.begin(), floorStrikes_.end());
    uniqueStrikes.insert(capStrikes_.begin(), capStrikes_.end());
    std::unique_copy(uniqueStrikes.begin(), uniqueStrikes.end(), std::back_inserter(strikes_),
                     [](double a, double b) { return QuantLib::close_enough(a, b); });
}

//! Computes a cap price from a floor price using the put-call parity and vice-versa
template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::priceFromPutCallParity(
    double price, bool isCapPrice, double atm, double strikeGrowth, double df) const {
    if (isCapPrice) {
        return price + atm - strikeGrowth * df;
    } else {
        return price + strikeGrowth * df - atm;
    }
}

//! Returns floor price for strike level (average annual inflation) and maturity index
template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::floorPrice(double strike, size_t tenorIdx,
                                                                                   double atm, double strikeGrowth,
                                                                                   double df) const {
    auto close_enough = [&strike](const double& x) { return QuantLib::close_enough(strike, x); };
    auto itCap = std::find_if(capStrikes_.begin(), capStrikes_.end(), close_enough);
    auto itFloor = std::find_if(floorStrikes_.begin(), floorStrikes_.end(), close_enough);
    if (itFloor != floorStrikes_.end()) {
        size_t floorPriceIdx = std::distance(floorStrikes_.begin(), itFloor);
        return floorPrices_[floorPriceIdx][tenorIdx];
    } else if (itCap != capStrikes_.end()) {
        size_t capPriceIdx = std::distance(capStrikes_.begin(), itCap);
        double capPrice = capPrices_[capPriceIdx][tenorIdx];
        return priceFromPutCallParity(capPrice, true, atm, strikeGrowth, df);
    } else {
        return QuantLib::Null<double>();
    }
}

//! Returns cap price for strike level (average annual inflation) and maturity index
template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::capPrice(double strike, size_t tenorIdx,
                                                                                 double atm, double strikeGrowth,
                                                                                 double df) const {
    auto close_enough = [&strike](const double& x) { return QuantLib::close_enough(strike, x); };
    auto itCap = std::find_if(capStrikes_.begin(), capStrikes_.end(), close_enough);
    auto itFloor = std::find_if(floorStrikes_.begin(), floorStrikes_.end(), close_enough);
    if (itCap != capStrikes_.end()) {
        size_t capPriceIdx = std::distance(capStrikes_.begin(), itCap);
        return capPrices_[capPriceIdx][tenorIdx];
    } else if (itFloor != floorStrikes_.end()) {
        size_t floorPriceIdx = std::distance(floorStrikes_.begin(), itFloor);
        double floorPrice = floorPrices_[floorPriceIdx][tenorIdx];
        return priceFromPutCallParity(floorPrice, false, atm, strikeGrowth, df);
    } else {
        return QuantLib::Null<double>();
    }
}

template <class InterpolatorStrike, class InterpolatorTime>
double CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::implyVol(double strike,
                                                                                 const QuantLib::Date& maturity,
                                                                                 double price, bool isFloor) const {
    QuantLib::Date startDate = capFloorStartDate();
    QuantLib::Calendar cal = calendar();
    auto bdc = businessDayConvention();
    auto dc = dayCounter();
    auto index = index_;
    auto freq = frequency();
    auto obsLag = observationLag();

    QuantLib::CPICapFloor capFloor(isFloor ? QuantLib::Option::Put : QuantLib::Option::Call,
                                   1.0, // unit nominal, because the price surface returns unit nominal prices
                                   capFloorStartDate(), baseCPI(), maturity, calendar(), businessDayConvention(),
                                   calendar(), businessDayConvention(), strike,
                                   index_, observationLag(),
                                   indexIsInterpolated() ? QuantLib::CPI::Linear : QuantLib::CPI::Flat);

    QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine = engine_;

    bool interpolated = indexIsInterpolated();
    capFloor.setPricingEngine(engine);

    auto targetFunction = [&engine, &cal, &dc, &bdc, &startDate, &obsLag, &freq, &price,
                           &capFloor, &interpolated](const double& guess) {
        QuantLib::ext::shared_ptr<QuantExt::ConstantCPIVolatility> vol = QuantLib::ext::make_shared<QuantExt::ConstantCPIVolatility>(
            guess, 0, cal, bdc, dc, obsLag, freq, interpolated, startDate);

        engine->setVolatility(QuantLib::Handle<QuantLib::CPIVolatilitySurface>(vol));

        QuantLib::Real npv = capFloor.NPV();
        return price - npv;
    };
    QuantLib::Brent solver;
    QuantLib::Real guess = (upperVolBound_ + lowerVolBound_) / 2.0;
    return solver.solve(targetFunction, solverTolerance_, guess, lowerVolBound_, upperVolBound_);
}

template <class InterpolatorStrike, class InterpolatorTime>
QuantLib::Real
CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::atmStrike(const QuantLib::Date& maturity,
                                                                           const QuantLib::Period& obsLag) const {
    QuantLib::Period lag = obsLag == -1 * QuantLib::Days ? observationLag() : obsLag;
    QuantLib::Date fixingDate = ZeroInflation::fixingDate(maturity, lag, frequency(), indexIsInterpolated());
    double forwardCPI = ZeroInflation::cpiFixing(index_, maturity, lag, indexIsInterpolated());
    double atm = forwardCPI / baseCPI();
    double ttm =
        QuantLib::inflationYearFraction(frequency(), indexIsInterpolated(), dayCounter(), baseDate(), fixingDate);
    return std::pow(atm, 1.0 / ttm) - 1.0;
}
} // namespace QuantExt
