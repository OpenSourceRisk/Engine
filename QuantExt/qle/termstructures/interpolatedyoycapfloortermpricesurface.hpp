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

/*! \file qle/termstructures/interpolatedyoycapfloortermpricesurface.hpp
    \brief Interpolated YoY Inflation Cap floor term price surface -
           extends QuantLib InterpolatedYoYCapFloorTermPriceSurface to allow
           choice of termstructure directly from YoY swap quotes or from
           atm swap quotes stripped from cap/floor price surface
    \ingroup termstructures
*/

#ifndef quantext_interpolated_yoy_capfloor_term_pricesurface_hpp
#define quantext_interpolated_yoy_capfloor_term_pricesurface_hpp

#include <ql/experimental/inflation/yoycapfloortermpricesurface.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Interpolated YoY Inflation Cap floor term price surface
/*! \ingroup termstructures */
template <class Interpolator2D, class Interpolator1D>
class InterpolatedYoYCapFloorTermPriceSurface : public YoYCapFloorTermPriceSurface {
public:
    InterpolatedYoYCapFloorTermPriceSurface(Natural fixingDays,
                                            const Period& yyLag, // observation lag
                                            const QuantLib::ext::shared_ptr<YoYInflationIndex>& yii, Rate baseRate,
                                            const Handle<YieldTermStructure>& nominal, const DayCounter& dc,
                                            const Calendar& cal, const BusinessDayConvention& bdc,
                                            const std::vector<Rate>& cStrikes, const std::vector<Rate>& fStrikes,
                                            const std::vector<Period>& cfMaturities, const Matrix& cPrice,
                                            const Matrix& fPrice,
                                            const Interpolator2D& interpolator2d = Interpolator2D(),
                                            const Interpolator1D& interpolator1d = Interpolator1D());

    //! inflation term structure interface
    //@{
    virtual Date maxDate() const override { return yoy_->maxDate(); }
    virtual Date baseDate() const override { return yoy_->baseDate(); }
    //@}
    virtual Natural fixingDays() const override { return fixingDays_; }

    //! \name YoYCapFloorTermPriceSurface interface
    //@{
    virtual std::pair<std::vector<Time>, std::vector<Rate> > atmYoYSwapTimeRates() const override {
        return atmYoYSwapTimeRates_;
    }
    virtual std::pair<std::vector<Date>, std::vector<Rate> > atmYoYSwapDateRates() const override {
        return atmYoYSwapDateRates_;
    }
    virtual QuantLib::ext::shared_ptr<YoYInflationTermStructure> YoYTS() const override {
        return yoyIndex_->yoyInflationTermStructure().empty() ? yoy_
                                                              : yoyIndex_->yoyInflationTermStructure().currentLink();
    }

    virtual Rate price(const Date& d, const Rate k) const override;
    virtual Real floorPrice(const Date& d, const Rate k) const override;
    virtual Real capPrice(const Date& d, const Rate k) const override;
    virtual Rate atmYoYSwapRate(const Date& d, bool extrapolate = true) const override {
        return atmYoYSwapRateCurve_(timeFromReference(d), extrapolate);
    }
    virtual Rate atmYoYRate(const Date& d, const Period& obsLag = Period(-1, Days), bool extrapolate = true) const override {
        // work in terms of maturity-of-instruments
        // so ask for rate with observation lag
        // Third parameter = force linear interpolation of yoy
        return yoy_->yoyRate(d, obsLag, false, extrapolate);
    }
    //@}

    //! \name LazyObject interface
    //@{
    void update() override;
    void performCalculations() const;
    //@}

    /* For stripping vols from a surface we need a more dense set of maturities than
       provided by market data i.e. yearly. This provides the option to update the
       vector of maturities - the interpoator handles the price */
    void setMaturities(std::vector<Period>& overrideMaturities) { cfMaturities_ = overrideMaturities; }

protected:
    // create instruments from quotes and bootstrap
    void calculateYoYTermStructure() const;

    // data for surfaces and curve
    mutable Matrix cPriceB_;
    mutable Matrix fPriceB_;
    mutable Interpolation2D capPrice_, floorPrice_;
    mutable Interpolator2D interpolator2d_;
    mutable Interpolation atmYoYSwapRateCurve_;
    mutable Interpolator1D interpolator1d_;
};

namespace detail {
struct CloseEnoughComparator {
    explicit CloseEnoughComparator(const Real v) : v_(v) {}
    bool operator()(const Real w) const { return QuantLib::close_enough(v_, w); }
    Real v_;
};
} // namespace detail

// template definitions
template <class Interpolator2D, class Interpolator1D>
InterpolatedYoYCapFloorTermPriceSurface<Interpolator2D, Interpolator1D>::InterpolatedYoYCapFloorTermPriceSurface(
    Natural fixingDays, const Period& yyLag, const QuantLib::ext::shared_ptr<YoYInflationIndex>& yii, Rate baseRate,
    const Handle<YieldTermStructure>& nominal, const DayCounter& dc, const Calendar& cal,
    const BusinessDayConvention& bdc, const std::vector<Rate>& cStrikes, const std::vector<Rate>& fStrikes,
    const std::vector<Period>& cfMaturities, const Matrix& cPrice, const Matrix& fPrice,
    const Interpolator2D& interpolator2d, const Interpolator1D& interpolator1d)
    : YoYCapFloorTermPriceSurface(fixingDays, yyLag, yii, baseRate, nominal, dc, cal, bdc, cStrikes, fStrikes,
                                  cfMaturities, cPrice, fPrice),
      interpolator2d_(interpolator2d), interpolator1d_(interpolator1d) {
    performCalculations();
}

template <class I2D, class I1D> void InterpolatedYoYCapFloorTermPriceSurface<I2D, I1D>::update() { notifyObservers(); }

template <class I2D, class I1D> void InterpolatedYoYCapFloorTermPriceSurface<I2D, I1D>::performCalculations() const {

    cfMaturityTimes_.clear();
    for (Size i = 0; i < cfMaturities_.size(); i++) {
        cfMaturityTimes_.push_back(timeFromReference(yoyOptionDateFromTenor(cfMaturities_[i])));
    }

    Interpolation2D capPriceTemp, floorPriceTemp;
    capPriceTemp = interpolator2d_.interpolate(cfMaturityTimes_.begin(), cfMaturityTimes_.end(), cStrikes_.begin(),
                                               cStrikes_.end(), cPrice_);
    capPriceTemp.enableExtrapolation();

    floorPriceTemp = interpolator2d_.interpolate(cfMaturityTimes_.begin(), cfMaturityTimes_.end(), fStrikes_.begin(),
                                                 fStrikes_.end(), fPrice_);
    floorPriceTemp.enableExtrapolation();

    // check if we have a valid yoyTS
    if (yoyIndex_->yoyInflationTermStructure().empty()) {
        // create a yoyinflation term structure from put/call parity
        // find the first overlapping strike
        std::vector<Real> overlappingStrikes;
        for (Size i = 0; i < fStrikes_.size(); i++) {
            for (Size j = 0; j < cStrikes_.size(); j++) {
                if (fStrikes_[i] == cStrikes_[j]) {
                    overlappingStrikes.push_back(fStrikes_[i]);
                }
            }
        }
        QL_REQUIRE(overlappingStrikes.size(), "No overlapping strikes between caps and floors for "
                                                  << "yoycapfloortermpricesurface " << yoyIndex_->name());

        // calculate the yoy termstructure from the first overlapping strike
        // TODO: extend to use all overlapping strike

        // We can get the 1Y fair swap rate from the zero Inflation curve
        // If the YoY curve is unavailable, a YoY Index built from a ZeroInflationIndex
        QuantLib::ext::shared_ptr<YoYInflationIndexWrapper> yiiWrapper =
            QuantLib::ext::dynamic_pointer_cast<YoYInflationIndexWrapper>(yoyIndex_);
        QuantLib::ext::shared_ptr<ZeroInflationTermStructure> zeroTs =
            yiiWrapper->zeroIndex()->zeroInflationTermStructure().currentLink();
        Real fairSwap1Y = zeroTs->zeroRate(yoyOptionDateFromTenor(Period(1, Years)));

        Real k = Null<Real>();
        if (fairSwap1Y < overlappingStrikes.back()) {
            for (Size i = 0; i < overlappingStrikes.size(); i++) {
                if (overlappingStrikes[i] > fairSwap1Y) {
                    k = overlappingStrikes[i];
                    break;
                }
            }
        } else {
            k = overlappingStrikes.back();
        }

        for (Size i = 0; i < cfMaturities_.size(); i++) {
            Time t = cfMaturityTimes_[i];
            // determine the sum of discount factors
            Size numYears = (Size)(t + 0.5);
            Real fairSwap;
            if (numYears == 1) {
                fairSwap = fairSwap1Y;
            } else {
                Real sumDiscount = 0.0;
                for (Size j = 0; j < numYears; ++j)
                    sumDiscount += nominalTS_->discount(j + 1.0);

                Real capPrice = capPriceTemp(t, k);
                Real floorPrice = floorPriceTemp(t, k);

                fairSwap = ((capPrice - floorPrice) / 10000 + k * sumDiscount) / sumDiscount;
            }

            atmYoYSwapDateRates_.first.push_back(referenceDate() + cfMaturities_[i]);
            atmYoYSwapTimeRates_.first.push_back(t);
            atmYoYSwapTimeRates_.second.push_back(fairSwap);
            atmYoYSwapDateRates_.second.push_back(fairSwap);
        }

        atmYoYSwapRateCurve_ = interpolator1d_.interpolate(
            atmYoYSwapTimeRates_.first.begin(), atmYoYSwapTimeRates_.first.end(), atmYoYSwapTimeRates_.second.begin());

        calculateYoYTermStructure();

    } else {
        yoy_ = yoyIndex_->yoyInflationTermStructure().currentLink();
    }

    cPriceB_ = Matrix(cfStrikes_.size(), cfMaturities_.size(), Null<Real>());
    fPriceB_ = Matrix(cfStrikes_.size(), cfMaturities_.size(), Null<Real>());

    for (Size j = 0; j < cfMaturities_.size(); ++j) {
        Period mat = cfMaturities_[j];

        Time t = cfMaturityTimes_[j];
        Size numYears = (Size)(t + 0.5);
        Real sumDiscount = 0.0;
        for (Size k = 0; k < numYears; ++k)
            sumDiscount += nominalTS_->discount(k + 1.0);

        Real S = yoy_->yoyRate(yoyOptionDateFromTenor(mat));
        for (Size i = 0; i < cfStrikes_.size(); ++i) {
            Real K = cfStrikes_[i];
            // Real K = std::pow(1.0 + K_quote, mat.length());
            Size indF = std::find_if(fStrikes_.begin(), fStrikes_.end(), detail::CloseEnoughComparator(cfStrikes_[i])) -
                        fStrikes_.begin();
            Size indC = std::find_if(cStrikes_.begin(), cStrikes_.end(), detail::CloseEnoughComparator(cfStrikes_[i])) -
                        cStrikes_.begin();
            bool isFloorStrike = indF < fStrikes_.size();
            bool isCapStrike = indC < cStrikes_.size();
            if (isFloorStrike) {
                fPriceB_[i][j] = fPrice_[indF][j];
                if (!isCapStrike) {
                    cPriceB_[i][j] = fPrice_[indF][j] + (S - K) * 10000 * sumDiscount;
                }
            }
            if (isCapStrike) {
                cPriceB_[i][j] = cPrice_[indC][j];
                if (!isFloorStrike) {
                    fPriceB_[i][j] = cPrice_[indC][j] - (S - K) * 10000 * sumDiscount;
                }
            }
        }
    }

    // check that all cells are filled
    for (Size i = 0; i < cPriceB_.rows(); ++i) {
        for (Size j = 0; j < cPriceB_.columns(); ++j) {
            QL_REQUIRE(cPriceB_[i][j] != Null<Real>(), "InterpolatedCPICapFloorTermPriceSurface: did not "
                                                       "fill call price matrix at ("
                                                           << i << "," << j << "), this is unexpected");
            QL_REQUIRE(fPriceB_[i][j] != Null<Real>(), "InterpolatedCPICapFloorTermPriceSurface: did not "
                                                       "fill floor price matrix at ("
                                                           << i << "," << j << "), this is unexpected");
        }
    }

    capPrice_ = interpolator2d_.interpolate(cfMaturityTimes_.begin(), cfMaturityTimes_.end(), cfStrikes_.begin(),
                                            cfStrikes_.end(), cPriceB_);
    capPrice_.enableExtrapolation();

    floorPrice_ = interpolator2d_.interpolate(cfMaturityTimes_.begin(), cfMaturityTimes_.end(), cfStrikes_.begin(),
                                              cfStrikes_.end(), fPriceB_);
    floorPrice_.enableExtrapolation();
}

template <class I2D, class I1D>
Rate InterpolatedYoYCapFloorTermPriceSurface<I2D, I1D>::price(const Date& d, const Rate k) const {
    Rate atm = atmYoYSwapRate(d);
    return k > atm ? capPrice(d, k) : floorPrice(d, k);
}

template <class I2D, class I1D>
Rate InterpolatedYoYCapFloorTermPriceSurface<I2D, I1D>::capPrice(const Date& d, const Rate k) const {
    Time t = timeFromReference(d);
    return std::max(0.0, capPrice_(t, k));
}

template <class I2D, class I1D>
Rate InterpolatedYoYCapFloorTermPriceSurface<I2D, I1D>::floorPrice(const Date& d, const Rate k) const {
    Time t = timeFromReference(d);
    return std::max(0.0, floorPrice_(t, k));
}

template <class I2D, class I1D>
void InterpolatedYoYCapFloorTermPriceSurface<I2D, I1D>::calculateYoYTermStructure() const {

    // which yoy-swap points to use in building the yoy-fwd curve?
    // for now pick every year
    Size nYears = (Size)(0.5 + timeFromReference(referenceDate() + cfMaturities_.back()));

    Handle<YieldTermStructure> nominalH(nominalTS_);
    std::vector<QuantLib::ext::shared_ptr<BootstrapHelper<YoYInflationTermStructure> > > YYhelpers;
    for (Size i = 1; i <= nYears; i++) {
        Date maturity = nominalTS_->referenceDate() + Period(i, Years);
        Handle<Quote> quote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(atmYoYSwapRate(maturity)))); //!
        QuantLib::ext::shared_ptr<BootstrapHelper<YoYInflationTermStructure> > anInstrument(new YearOnYearInflationSwapHelper(
            quote, observationLag(), maturity, calendar(), bdc_, dayCounter(), yoyIndex(), nominalH));
        YYhelpers.push_back(anInstrument);
    }

    // usually this base rate is known
    // however for the data to be self-consistent
    // we pick this as the end of the curve
    Rate baseYoYRate = atmYoYSwapRate(referenceDate()); //!

    QuantLib::ext::shared_ptr<PiecewiseYoYInflationCurve<I1D>> pYITS(new PiecewiseYoYInflationCurve<I1D>(
        nominalTS_->referenceDate(), calendar(), dayCounter(), observationLag(), yoyIndex()->frequency(),
        yoyIndex()->interpolated(), baseYoYRate, YYhelpers));
    pYITS->recalculate();
    yoy_ = pYITS; // store

    // check that helpers are repriced
    const Real eps = 1e-5;
    for (Size i = 0; i < YYhelpers.size(); i++) {
        Rate original = atmYoYSwapRate(yoyOptionDateFromTenor(Period(i + 1, Years)));
        QL_REQUIRE(fabs(YYhelpers[i]->impliedQuote() - original) < eps,
                   "could not reprice helper " << i << ", data " << original << ", implied quote "
                                               << YYhelpers[i]->impliedQuote());
    }
}

} // namespace QuantExt

#endif
