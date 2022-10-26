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
#include <ql/experimental/inflation/cpicapfloortermpricesurface.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/termstructures/inflation/constantcpivolatility.hpp>
#include <qle/termstructures/inflation/cpivolatilitystructure.hpp>
#include <qle/utilities/inflation.hpp>

namespace QuantExt {

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
  If type is CapFloor: In case of overlap, use floor quotes up to the ATM strike, cap quotes for strikes beyond
  ATM
 */

template <class InterpolatorStrike, class InterpolatorTime>
class CPIPriceVolatilitySurface : public QuantExt::CPIVolatilitySurface, LazyObject {
public:
    CPIPriceVolatilitySurface(
        PriceQuotePreference type, const Period& observationLag,
        const Calendar& cal, // calendar in index may not be useful
        const BusinessDayConvention& bdc, const DayCounter& dc,
        const boost::shared_ptr<QuantLib::ZeroInflationIndex> index, Handle<YieldTermStructure> yts,
        const std::vector<Rate>& cStrikes, const std::vector<Rate>& fStrikes, const std::vector<Period>& cfMaturities,
        const Matrix& cPrice, const Matrix& fPrice, const boost::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
        bool ignoreMissingPrices = false, // if true, it allows prices to be Null and work as long there is one
        bool flatExtrapolateMinStrike = true, bool flatExtrapolateMaxStrike = true,
        const QuantLib::Date& capFloorStartDate = Date(),
        const QuantLib::VolatilityType& volType = QuantLib::ShiftedLognormal, const double displacement = 0.0,
        const QuantLib::Real& upperVolBound = StrippedCPIVolSurfaceDefaultValues::upperVolBound,
        const QuantLib::Real& lowerVolBound = StrippedCPIVolSurfaceDefaultValues::lowerVolBound,
        const QuantLib::Real& solverTolerance = StrippedCPIVolSurfaceDefaultValues::solverTolerance);

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
    const std::vector<QuantLib::Period>& maturities() { return expiries_; }

    double baseCPI() const {
        return ZeroInflation::cpiFixing(index_, capFloorStartDate(), observationLag(), indexIsInterpolated());
    }

    double atmGrowth(Period& tenor) {
        return atmGrowth(optionDateFromTenor(tenor));
    }

    double atmGrowth(const Date& date) {
        return ZeroInflation::cpiFixing(index_, date, observationLag(), indexIsInterpolated()) / baseCPI();
    }

    const QuantLib::Matrix& volData() {
        calculate();
        return volData_;
    }

    //@}

private:
    void validateInputParameters(const Matrix& capQuotes, const Matrix& floorQuotes) {
        QL_REQUIRE(!expiries_.empty(), "Need at least one tenor");
        QL_REQUIRE(!floorStrikes_.empty() && !capStrikes_.empty(), "cap and floor strikes can not be both empty");
        QL_REQUIRE(capQuotes.rows() == capStrikes_.size() && capQuotes.columns() == expiries_.size(),
                   "mismatch between cap price matrix dimension and number of strikes and tenors");
        QL_REQUIRE(floorQuotes.rows() == floorStrikes_.size() && floorQuotes.columns() == expiries_.size(),
                   "mismatch between cap price matrix dimension and number of strikes and tenors");
        // TODO: some basic arbitrage checks
    }

    void initializeStrikes() const {
        strikes_.clear();
        std::set<double> uniqueStrikes(floorStrikes_.begin(), floorStrikes_.end());
        uniqueStrikes.insert(capStrikes_.begin(), capStrikes_.end());
        std::unique_copy(uniqueStrikes.begin(), uniqueStrikes.end(), std::back_inserter(strikes_),
                         [](double a, double b) { return QuantLib::close_enough(a, b); });
    }

    void initializeTimes() const {
        fixingTimes_.clear();
        fixingDates_.clear();
        maturityDates_.clear();
        for (QuantLib::Size i = 0; i < expiries_.size(); i++) {
            QuantLib::Date d = optionMaturityFromTenor(expiries_[i]);
            maturityDates_.push_back(d);
            fixingDates_.push_back(ZeroInflation::fixingDate(d, observationLag(), frequency(), indexIsInterpolated()));
            fixingTimes_.push_back(fixingTime(d));
        }
    }

    void initializePriceMatrix(const Matrix& capQuotes, const Matrix& floorQuotes) const {
        for (int tenorIdx = 0; tenorIdx < expiries_.size(); ++tenorIdx) {
            auto ttm = inflationYearFraction(frequency(), indexIsInterpolated(), dayCounter(), baseDate(),
                                             fixingDates_[tenorIdx]);
            double df = yts_->discount(fixingTimes_[tenorIdx]);
            for (int strikeIdx = 0; strikeIdx < strikes_.size(); ++strikeIdx) {
                double strike = strikes_[strikeIdx];
                double strikeGrowth = std::pow(1.0 + strike, ttm);
                auto itCap = std::find(capStrikes_.begin(), capStrikes_.end(), strike);
                auto itFloor = std::find(floorStrikes_.begin(), floorStrikes_.end(), strike);
                if (itCap != capStrikes_.end()) {
                    capPrices_[strikeIdx][tenorIdx] = capQuotes[strikeIdx][tenorIdx];
                } else if (itFloor != floorStrikes_.end() && floorQuotes[strikeIdx][tenorIdx] != Null<Real>()) {
                    // Put-Call parity
                    capPrices_[strikeIdx][tenorIdx] =
                        floorQuotes[strikeIdx][tenorIdx] + strikeGrowth * df - atmLevels_[tenorIdx];
                } else {
                    capPrices_[strikeIdx][tenorIdx] = Null<Real>();
                }
                if (itFloor != floorStrikes_.end()) {
                    floorPrices_[strikeIdx][tenorIdx] = floorQuotes[strikeIdx][tenorIdx];
                } else if (itCap != capStrikes_.end() && capQuotes[strikeIdx][tenorIdx] != Null<Real>()) {
                    // Put-Call parity
                    floorPrices_[strikeIdx][tenorIdx] =
                        capQuotes[strikeIdx][tenorIdx] + atmLevels_[tenorIdx] - strikeGrowth * df;
                } else {
                    floorPrices_[strikeIdx][tenorIdx] = Null<Real>();
                }
            }
        }
    }

    double price(size_t strikeIdx, size_t tenorIdx, bool floorPrice) const {
        if (floorPrice) {
            return floorPrices_[strikeIdx][tenorIdx];
        } else {
            return capPrices_[strikeIdx][tenorIdx];
        }
    }

    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const override {
        calculate();
        volSurface_->getValue(length, strike);
    }

    bool chooseFloor(QuantLib::Real strike, QuantLib::Real atmRate) const;

    Real implyVol(double strike, const Date& maturity, double price) const;

    class ObjectiveFunction {
    public:
        // Pass price to match, CPICapFloor engine, and all data for creating a quoted CPICapFloor
        ObjectiveFunction(QuantLib::Real priceToMatch, bool useFloor, QuantLib::Real strike, QuantLib::Date startDate,
                          QuantLib::Date maturityDate, QuantLib::Real baseCPI, const CPIVolatilitySurface* surface,
                          const boost::shared_ptr<QuantLib::ZeroInflationIndex>& zeroIndex,
                          const boost::shared_ptr<QuantExt::CPICapFloorEngine>& engine);
        // Update the engine's volatility, return difference between capfloor NPV and priceToMatch
        QuantLib::Real operator()(QuantLib::Volatility guess) const;

    private:
        QuantLib::Real priceToMatch_;
        bool useFloor_;
        QuantLib::Real strike_;
        QuantLib::Date startDate_, maturityDate_;
        QuantLib::Real baseCPI_;
        const CPIVolatilitySurface* surface_;
        boost::shared_ptr<QuantLib::ZeroInflationIndex> index_;
        boost::shared_ptr<QuantExt::CPICapFloorEngine> engine_;
        QuantLib::Date lastAvailableFixingDate_;
        QuantLib::CPICapFloor cpiCapFloor_;
    };

    PriceQuotePreference preference_;
    boost::shared_ptr<QuantLib::ZeroInflationIndex> index_;
    Handle<YieldTermStructure> yts_;
    std::vector<double> capStrikes_;
    std::vector<double> floorStrikes_;

    boost::shared_ptr<QuantExt::CPICapFloorEngine> engine_;
    bool ignoreMissingPrices_;
    QuantLib::Real upperVolBound_;
    QuantLib::Real lowerVolBound_;
    QuantLib::Real solverTolerance_;
    Interpolator2D interpolator2d_;

    std::vector<QuantLib::Period> expiries_;
    mutable std::vector<QuantLib::Rate> strikes_;
    mutable Matrix capPrices_;
    mutable Matrix floorPrices_;

    mutable std::vector<Date> maturityDates_;
    mutable std::vector<Date> fixingDates_;
    mutable std::vector<QuantLib::Time> fixingTimes_;

    mutable QuantLib::Matrix volData_;
    mutable boost::shared_ptr<OptionInterpolator2d<InterpolatorStrike, InterpolatorTime>> volSurface_;
};

template <class InterpolatorStrike, class InterpolatorTime>
CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::CPIPriceVolatilitySurface(
    PriceQuotePreference type, const Period& observationLag,
    const Calendar& cal, // calendar in index may not be useful
    const BusinessDayConvention& bdc, const DayCounter& dc, const boost::shared_ptr<QuantLib::ZeroInflationIndex> index,
    Handle<YieldTermStructure> yts, const std::vector<Rate>& cStrikes, const std::vector<Rate>& fStrikes,
    const std::vector<Period>& cfMaturities, const Matrix& cPrice, const Matrix& fPrice,
    const boost::shared_ptr<QuantExt::CPICapFloorEngine>& engine,
    bool ignoreMissingPrices, // if true, it allows prices to be Null and work as long there is one
    bool flatExtrapolateMinStrike, bool flatExtrapolateMaxStrike, const QuantLib::Date& capFloorStartDate,
    const QuantLib::VolatilityType& volType, const double displacement, const QuantLib::Real& upperVolBound,
    const QuantLib::Real& lowerVolBound, const QuantLib::Real& solverTolerance)
    : QuantExt::CPIVolatilitySurface(0, cal, bdc, dc, observationLag, index->frequency(), index->interpolated(),
                                     capFloorStartDate, volType, displacement),
      preference_(type), index_(index), yts_(yts), capStrikes_(cStrikes), floorStrikes_(fStrikes), engine_(engine),
      ignoreMissingPrices_(ignoreMissingPrices), upperVolBound_(upperVolBound), lowerVolBound_(lowerVolBound),
      solverTolerance_(solverTolerance), expiries_(cfMaturities), capPrices_(cPrice), floorPrices_(fPrice) {
    validateInputParameters();
    initializeTimes();
    initializeStrikes();
    QL_REQUIRE(!yts.empty(), "DiscountCurve not provided");
    QL_REQUIRE(engine, "PricingEngine not provided");
    QL_REQUIRE(index, "PricingEngine not provided");
    registerWith(index_);
    registerWith(yts_);
}

template <class InterpolatorStrike, class InterpolatorTime>
void CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::performCalculations() const {
    volData_ = QuantLib::Matrix(strikes_.size(), expiries_.size(), QuantLib::Null<QuantLib::Real>());
    initializeTimes();
    initializePriceMatrix();
    vector<Date> fixingDates;
    vector<double> strikes;
    vector<double> vols;

    for (QuantLib::Size tenorIdx = 0; tenorIdx < expiries_.size(); tenorIdx++) {
        Date maturityDate = maturityDates_[tenorIdx];
        Date fixingDate = fixingDates_[tenorIdx];
        double atm = atmGrowth(maturityDate);
        for (QuantLib::Size strikeIdx = 0; strikeIdx < strikes_.size(); strikeIdx++) {
            double strike = strikes_[strikeIdx];
            bool useFloor = chooseFloor(strike, atm);
            double vol = Null<Real>();
            QuantLib::Real priceToMatch = price(strikeIdx, tenorIdx, useFloor);
            if (priceToMatch != Null<Real>()) {
                try {
                    vol = implyVol(strike, maturityDate, priceToMatch, useFloor);
                } catch (const std::exception& e) {
                    QL_FAIL("Can not imply cpi capfloor vol for tenor " << expiries_[tenorIdx] << " and strike "
                                                                        << strike << " from price " << priceToMatch
                                                                        << ", got " << e.what());
                }
            } else {
                QL_REQUIRE(ignoreMissingPrices_,
                           "Missing price for cpi capfloor vol for tenor " << expiries_[tenorIdx] << " and strike " << strike); 
            }
            if (vol != Null<Real>()) {
                fixingDates.push_back(fixingDate);
                strikes.push_back(strike);
                vols.push_back(vol);
            }
        }
    }

    volSurface_ = boost::make_shared<OptionInterpolator2d<InterpolatorStrike, InterpolatorTime>>();

    for (QuantLib::Size strikeIdx = 0; strikeIdx < strikes_.size(); strikeIdx++) {
        for (QuantLib::Size tenorIdx = 0; tenorIdx < expiries_.size(); tenorIdx++) {
            volData_[strikeIdx][tenorIdx] = volSurface_->getValue(fixingTimes[tenorIdx], strikes_[strikeIdx]);
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
QuantLib::Real CPIPriceVolatilitySurface<InterpolatorStrike,
                               InterpolatorTime>::minStrike() const {
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
    return volSurface_->getValue(length, strike);
}

template <class InterpolatorStrike, class InterpolatorTime>
CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::ObjectiveFunction::ObjectiveFunction(
    QuantLib::Real priceToMatch, bool useFloor, QuantLib::Real strike, QuantLib::Date startDate,
    QuantLib::Date maturityDate, QuantLib::Real baseCPI, const CPIVolatilitySurface* surface,
    const boost::shared_ptr<QuantLib::ZeroInflationIndex>& zeroIndex,
    const boost::shared_ptr<QuantExt::CPICapFloorEngine>& engine)
    : priceToMatch_(priceToMatch), useFloor_(useFloor), strike_(strike), startDate_(startDate),
      maturityDate_(maturityDate), baseCPI_(baseCPI), surface_(surface), index_(zeroIndex), engine_(engine),
      cpiCapFloor_(QuantLib::CPICapFloor(
          useFloor_ ? QuantLib::Option::Put : QuantLib::Option::Call,
          1.0, // unit nominal, because the price surface returns unit nominal prices
          startDate_, baseCPI_, maturityDate_, surface->calendar(), surface->businessDayConvention(),
          surface->calendar(), surface->businessDayConvention(), strike_,
          QuantLib::Handle<QuantLib::ZeroInflationIndex>(index_), surface->observationLag(), QuantLib::CPI::AsIndex)) {

    // FIXME: observation interpolation (last argument) uses default setting here CPI::AsIndex

    cpiCapFloor_.setPricingEngine(engine_);
}

template <class InterpolatorStrike, class InterpolatorTime>
QuantLib::Real CPIPriceVolatilitySurface<InterpolatorStrike, InterpolatorTime>::ObjectiveFunction::operator()(
    QuantLib::Volatility guess) const {
    boost::shared_ptr<QuantExt::ConstantCPIVolatility> vol = boost::make_shared<QuantExt::ConstantCPIVolatility>(
        guess, surface_->settlementDays(), surface_->calendar(), surface_->businessDayConvention(),
        surface_->dayCounter(), surface_->observationLag(), surface_->frequency(), index_->interpolated(), startDate_);

    engine_->setVolatility(QuantLib::Handle<QuantLib::CPIVolatilitySurface>(vol));

    QuantLib::Real npv = cpiCapFloor_.NPV();
    return priceToMatch_ - npv;
}

} // namespace QuantExt
