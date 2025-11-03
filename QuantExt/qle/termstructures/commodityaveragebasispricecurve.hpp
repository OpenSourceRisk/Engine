/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/commodityaveragebasispricecurve.hpp
    \brief A commodity price curve created from an averaged base curve and a collection of basis quotes.
    \ingroup termstructures
*/

#ifndef quantext_commodity_average_basis_price_curve_hpp
#define quantext_commodity_average_basis_price_curve_hpp

#include <ql/math/comparison.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/termstructures/commoditybasispricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Commodity average basis price curve.
/*! Class representing an outright commodity price curve created from a base price curve and a collection of basis
    quotes that are added to or subtracted from the base curve. This class is intended to be used only for commodity
    future basis price curves. The base curve is averaged over the period defined the basis quote.

    \ingroup termstructures
*/
template <class Interpolator>
class CommodityAverageBasisPriceCurve : public CommodityBasisPriceTermStructure,
                                        public QuantLib::LazyObject,
                                        protected QuantLib::InterpolatedCurve<Interpolator> {
public:
    //! \name Constructors
    //@{
    //! Curve constructed from dates and quotes
    CommodityAverageBasisPriceCurve(const QuantLib::Date& referenceDate,
                                    const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote>>& basisData,
                                    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec,
                                    const QuantLib::ext::shared_ptr<CommodityIndex>& index,
                                    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                    bool priceAsHistFixing = true, const Interpolator& interpolator = Interpolator());
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    QuantLib::Time maxTime() const override;
    //@}

    //! \name PriceTermStructure interface
    //@{
    QuantLib::Time minTime() const override;
    std::vector<QuantLib::Date> pillarDates() const override;
    const QuantLib::Currency& currency() const override { return baseIndex_->priceCurve()->currency(); }
    //@}

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Time>& times() const { return this->times_; }
    const std::vector<QuantLib::Real>& prices() const { return this->data_; }
    //@}

protected:
    //! \name PriceTermStructure implementation
    //@{
    QuantLib::Real priceImpl(QuantLib::Time t) const override;
    //@}

private:
    std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> > basisData_;

    std::vector<QuantLib::Date> dates_;

    /*! Interpolator used for interpolating the basis if needed. Basis interpolation uses the same interpolator as
        the curve itself. A second template parameter could be added for this in future if it needs to be relaxed.
    */
    mutable std::vector<Time> basisTimes_;
    mutable std::vector<Real> basisValues_;
    mutable Interpolation basisInterpolation_;

    //! The averaging cashflows will give the base curve prices.
    QuantLib::Leg baseLeg_;

    /*! Map where the key is the index of a time in the times_ vector and the value is the index of the
        cashflow in the baseLeg_ to associate with that time.
    */
    std::map<QuantLib::Size, QuantLib::Size> legIndexMap_;
};

template <class Interpolator>
CommodityAverageBasisPriceCurve<Interpolator>::CommodityAverageBasisPriceCurve(
    const QuantLib::Date& referenceDate, const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> >& basisData,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec, const QuantLib::ext::shared_ptr<CommodityIndex>& index,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis, bool priceAsHistFixing,
    const Interpolator& interpolator)
    : CommodityBasisPriceTermStructure(referenceDate, basisFec, index, baseFec, addBasis, 0, true, priceAsHistFixing),
      QuantLib::InterpolatedCurve<Interpolator>(interpolator), basisData_(basisData) {
    QL_REQUIRE(baseIndex_ != nullptr && !baseIndex_->priceCurve().empty(),
               "CommodityAverageBasisPriceCurve requires baseIndex with priceCurve");
    using QuantLib::Date;
    using QuantLib::Schedule;
    using QuantLib::io::iso_date;
    using QuantLib::io::ordinal;
    using std::distance;
    using std::find;
    using std::max;
    using std::sort;
    using std::vector;

    // Observe the quotes
    for (auto it = basisData_.cbegin(); it != basisData_.cend();) {
        if (it->first < referenceDate) {
            // If the basis quote's date is before the reference date, remove it.
            basisData_.erase(it++);
        } else {
            // Otherwise, process the basis quote
            dates_.push_back(it->first);
            basisTimes_.push_back(timeFromReference(it->first));
            if (addBasis_)
                basisValues_.push_back(it->second->value());
            else
                basisValues_.push_back(-it->second->value());
            registerWith(it->second);
            ++it;
        }
    }

    // Set up the interpolation to be used on the basis
    basisInterpolation_ = interpolator.interpolate(basisTimes_.begin(), basisTimes_.end(), basisValues_.begin());

    // Initialise this curve's times with the basis pillars. We will add more pillars below.
    this->times_ = basisTimes_;

    // Get the first basis contract expiry date strictly prior to the curve reference date.
    Date start = basisFec_->priorExpiry(false, referenceDate);

    // Get the first basis contract expiry date on or after the max date. Here, max date is defined as the maximum of
    // 1) last pillar date of base price curve and 2) basis curve data.
    Date maxDate = max(baseIndex_->priceCurve()->maxDate(), basisData_.rbegin()->first);
    Date end = basisFec_->nextExpiry(true, maxDate);

    // Create the leg schedule using a vector of dates which are the successive basis contract expiry dates
    QL_REQUIRE(start < end, "Expected that the start date, " << io::iso_date(start)
                                                             << ", would be strictly less than the end date, "
                                                             << io::iso_date(end) << ".");
    vector<Date> expiries{ start + 1 * Days };
    vector<Time> scheduleTimes;
    while (start < end) {
        start = basisFec_->nextExpiry(true, start + 1 * Days);
        expiries.push_back(start);
        Time t = timeFromReference(start);
        // Only add to this->times_ if it is not already there. We can use dates_ for this check.
        if (find(dates_.begin(), dates_.end(), start) == dates_.end()) {
            this->times_.push_back(t);
            dates_.push_back(start);
        }
        scheduleTimes.push_back(t);
    }
    QL_REQUIRE(start == end, "Expected that the start date, " << io::iso_date(start) << ", to equal the end date, "
                                                              << io::iso_date(end)
                                                              << ", after creating the sequence of expiry dates.");

    // Sort the times and dates vector and ensure no duplicates in the times vector.
    sort(this->times_.begin(), this->times_.end());
    sort(dates_.begin(), dates_.end());
    auto it = unique(this->times_.begin(), this->times_.end(), [](double s, double t) { return close(s, t); });
    QL_REQUIRE(it == this->times_.end(), "Unexpected duplicate time, " << *it << ", in the times vector.");
    this->data_.resize(this->times_.size());

    // Populate the leg of cashflows.
    baseLeg_ = CommodityIndexedAverageLeg(Schedule(expiries), baseIndex_)
                   .withFutureExpiryCalculator(baseFec_)
                   .useFuturePrice(true)
                   .withQuantities(1.0);
    QL_REQUIRE(baseLeg_.size() == scheduleTimes.size(), "Unexpected number of averaging cashflows in the leg: "
                                                            << "got " << baseLeg_.size() << " but expected "
                                                            << scheduleTimes.size());

    // Populate the legIndexMap_
    for (Size i = 0; i < this->times_.size(); i++) {
        for (Size j = 0; j < scheduleTimes.size(); j++) {

            if (this->times_[i] < scheduleTimes[j] || close(this->times_[i], scheduleTimes[j])) {
                QL_REQUIRE(legIndexMap_.find(i) == legIndexMap_.end(),
                           "Should not already have a mapping for the " << ordinal(i) << " time.");
                legIndexMap_[i] = j;
                break;
            }

            if (j == scheduleTimes.size()) {
                QL_FAIL("Could not map the " << ordinal(i) << " time, " << this->times_[i] << ", to a cashflow.");
            }
        }
    }

    // Set up the underlying interpolation on times_ and data_
    QuantLib::InterpolatedCurve<Interpolator>::setupInterpolation();
}

template <class Interpolator> void CommodityAverageBasisPriceCurve<Interpolator>::update() {
    QuantLib::LazyObject::update();
}

template <class Interpolator> void CommodityAverageBasisPriceCurve<Interpolator>::performCalculations() const {

    // Update the basis interpolation object
    Size basisIdx = 0;
    for (const auto& kv : basisData_) {
        basisValues_[basisIdx] = addBasis_ ? kv.second->value() : -kv.second->value();
        basisIdx++;
    }
    basisInterpolation_.update();

    // Update this curve's interpolation
    for (Size i = 0; i < this->times_.size(); i++) {

        Real baseValue = baseLeg_[legIndexMap_.at(i)]->amount();

        // Get the basis with flat extrapolation
        Real basis = 0.0;
        if (this->times_[i] < basisTimes_.front()) {
            basis = basisValues_.front();
        } else if (this->times_[i] > basisTimes_.back()) {
            basis = basisValues_.back();
        } else {
            basis = basisInterpolation_(this->times_[i], true);
        }

        // Update the outright value
        this->data_[i] = baseValue + basis;
    }
    this->interpolation_.update();
}

template <class Interpolator> QuantLib::Date CommodityAverageBasisPriceCurve<Interpolator>::maxDate() const {
    return dates_.back();
}

template <class Interpolator> QuantLib::Time CommodityAverageBasisPriceCurve<Interpolator>::maxTime() const {
    return this->times_.back();
}

template <class Interpolator> QuantLib::Time CommodityAverageBasisPriceCurve<Interpolator>::minTime() const {
    return this->times_.front();
}

template <class Interpolator>
std::vector<QuantLib::Date> CommodityAverageBasisPriceCurve<Interpolator>::pillarDates() const {
    return dates_;
}

template <class Interpolator>
QuantLib::Real CommodityAverageBasisPriceCurve<Interpolator>::priceImpl(QuantLib::Time t) const {
    calculate();
    return this->interpolation_(t, true);
}

} // namespace QuantExt

#endif
