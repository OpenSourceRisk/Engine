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

/*! \file qle/termstructures/commoditybasispricecurve.hpp
    \brief A commodity price curve created from a base price curve and a collection of basis quotes
    \ingroup termstructures
*/

#ifndef quantext_commodity_basis_price_curve_hpp
#define quantext_commodity_basis_price_curve_hpp

#include <ql/math/comparison.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Commodity basis price curve.
/*! Class representing an outright commodity price curve created from a base price curve and a collection of basis
    quotes that are added to or subtracted from the base curve. This class is intended to be used only for commodity
    future basis price curves.

    There is an assumption in the curve construction that the frequency of the base future contract is the same as
    the frequency of the basis future contract. In other words, if the base future contract is monthly then the basis
    future contract is monthly for example.

    \ingroup termstructures
*/
template <class Interpolator>
class CommodityBasisPriceCurve : public PriceTermStructure,
                                 public QuantLib::LazyObject,
                                 protected QuantLib::InterpolatedCurve<Interpolator> {
public:
    //! \name Constructors
    //@{
    //! Curve constructed from dates and quotes
    CommodityBasisPriceCurve(const QuantLib::Date& referenceDate,
                             const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> >& basisData,
                             const boost::shared_ptr<FutureExpiryCalculator>& basisFec,
                             const boost::shared_ptr<CommodityIndex>& index,
                             const QuantLib::Handle<PriceTermStructure>& basePts,
                             const boost::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                             QuantLib::Size monthOffset = 0, const Interpolator& interpolator = Interpolator());
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
    const QuantLib::Currency& currency() const override { return basePts_->currency(); }
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
    boost::shared_ptr<FutureExpiryCalculator> basisFec_;
    boost::shared_ptr<CommodityIndex> index_;
    QuantLib::Handle<PriceTermStructure> basePts_;
    boost::shared_ptr<FutureExpiryCalculator> baseFec_;
    bool addBasis_;
    QuantLib::Size monthOffset_;

    std::vector<QuantLib::Date> dates_;

    /*! Interpolator used for interpolating the basis if needed. Basis interpolation uses the same interpolator as
        the curve itself. A second template parameter could be added for this in future if it needs to be relaxed.
    */
    mutable std::vector<Time> basisTimes_;
    mutable std::vector<Real> basisValues_;
    mutable Interpolation basisInterpolation_;

    //! The commodity cashflows will give the base curve prices.
    std::map<QuantLib::Date, boost::shared_ptr<CommodityIndexedCashFlow> > baseLeg_;

    /*! Map where the key is the index of a time in the times_ vector and the value is the index of the
        cashflow in the baseLeg_ to associate with that time.
    */
    std::map<QuantLib::Size, QuantLib::Size> legIndexMap_;

    //! Get the contract date from an expiry date
    QuantLib::Date getContractDate(const QuantLib::Date& expiry,
                                   const boost::shared_ptr<FutureExpiryCalculator>& fec) const;

    //! Make a commodity indexed cashflow
    boost::shared_ptr<CommodityIndexedCashFlow> makeCashflow(const QuantLib::Date& start,
                                                             const QuantLib::Date& end) const;
};

template <class Interpolator>
CommodityBasisPriceCurve<Interpolator>::CommodityBasisPriceCurve(
    const QuantLib::Date& referenceDate, const std::map<QuantLib::Date, QuantLib::Handle<QuantLib::Quote> >& basisData,
    const boost::shared_ptr<FutureExpiryCalculator>& basisFec, const boost::shared_ptr<CommodityIndex>& index,
    const QuantLib::Handle<PriceTermStructure>& basePts, const boost::shared_ptr<FutureExpiryCalculator>& baseFec,
    bool addBasis, QuantLib::Size monthOffset, const Interpolator& interpolator)
    : PriceTermStructure(referenceDate, QuantLib::NullCalendar(), basePts->dayCounter()),
      QuantLib::InterpolatedCurve<Interpolator>(interpolator), basisData_(basisData), basisFec_(basisFec),
      index_(index), basePts_(basePts), baseFec_(baseFec), addBasis_(addBasis), monthOffset_(monthOffset) {

    using QuantLib::Date;
    using QuantLib::Schedule;
    using QuantLib::io::iso_date;
    using QuantLib::io::ordinal;
    using std::distance;
    using std::find;
    using std::max;
    using std::min;
    using std::sort;
    using std::vector;

    registerWith(basePts_);

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

    // Get the first basis contract expiry date on or after the curve reference date.
    Date basisExpiry = basisFec_->nextExpiry(true, referenceDate);

    // Get the first basis contract expiry date on or after the max date. Here, max date is defined as the maximum of
    // 1) last pillar date of base price curve and 2) basis curve data.
    Date maxDate = max(basePts_->maxDate(), basisData_.rbegin()->first);
    Date end = basisFec_->nextExpiry(true, maxDate);

    // Populate the base cashflows.
    while (basisExpiry <= end) {

        Date basisContractDate = getContractDate(basisExpiry, basisFec_);
        Date periodStart = basisContractDate - monthOffset_ * Months;
        Date periodEnd = (periodStart + 1 * Months) - 1 * Days;
        baseLeg_[basisExpiry] = makeCashflow(periodStart, periodEnd);

        // Only add to this->times_ if it is not already there. We can use dates_ for this check.
        if (find(dates_.begin(), dates_.end(), basisExpiry) == dates_.end()) {
            this->times_.push_back(timeFromReference(basisExpiry));
            dates_.push_back(basisExpiry);
        }

        basisExpiry = basisFec_->nextExpiry(true, basisExpiry + 1 * Days);
    }

    // Sort the times and dates vector and ensure no duplicates in the times vector.
    sort(this->times_.begin(), this->times_.end());
    sort(dates_.begin(), dates_.end());
    auto it = unique(this->times_.begin(), this->times_.end(), [](double s, double t) { return close(s, t); });
    QL_REQUIRE(it == this->times_.end(), "Unexpected duplicate time, " << *it << ", in the times vector.");
    this->data_.resize(this->times_.size());

    // Set up the underlying interpolation on times_ and data_
    QuantLib::InterpolatedCurve<Interpolator>::setupInterpolation();
}

template <class Interpolator> void CommodityBasisPriceCurve<Interpolator>::update() { QuantLib::LazyObject::update(); }

template <class Interpolator> void CommodityBasisPriceCurve<Interpolator>::performCalculations() const {

    // Update the basis interpolation object
    Size basisIdx = 0;
    for (const auto& kv : basisData_) {
        basisValues_[basisIdx] = addBasis_ ? kv.second->value() : -kv.second->value();
        basisIdx++;
    }
    basisInterpolation_.update();

    // Update this curve's interpolation
    for (Size i = 0; i < this->times_.size(); i++) {

        Real baseValue = 0.0;
        auto it = baseLeg_.find(dates_[i]);
        if (it != baseLeg_.end()) {
            // If we have associated the basis date with a base cashflow.
            baseValue = it->second->amount();
        } else {
            // If we didn't associate the basis date with a base cashflow, just ask the base pts at t. This will have
            // happened if the basis date that was passed in was not a basis contract expiry date wrt basisFec_.
            baseValue = basePts_->price(this->times_[i], true);
        }

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

template <class Interpolator> QuantLib::Date CommodityBasisPriceCurve<Interpolator>::maxDate() const {
    return dates_.back();
}

template <class Interpolator> QuantLib::Time CommodityBasisPriceCurve<Interpolator>::maxTime() const {
    return this->times_.back();
}

template <class Interpolator> QuantLib::Time CommodityBasisPriceCurve<Interpolator>::minTime() const {
    return this->times_.front();
}

template <class Interpolator> std::vector<QuantLib::Date> CommodityBasisPriceCurve<Interpolator>::pillarDates() const {
    return dates_;
}

template <class Interpolator> QuantLib::Real CommodityBasisPriceCurve<Interpolator>::priceImpl(QuantLib::Time t) const {
    calculate();
    return this->interpolation_(t, true);
}

template <class Interpolator>
QuantLib::Date
CommodityBasisPriceCurve<Interpolator>::getContractDate(const QuantLib::Date& expiry,
                                                        const boost::shared_ptr<FutureExpiryCalculator>& fec) const {

    using QuantLib::Date;
    using QuantLib::io::iso_date;

    // Try the expiry date's associated contract month and year. Start with the expiry month and year itself.
    // Assume that expiry is within 12 months of the contract month and year.
    Date result(1, expiry.month(), expiry.year());
    Date calcExpiry = fec->expiryDate(result, 0);
    Size maxIncrement = 12;
    while (calcExpiry != expiry && maxIncrement > 0) {
        if (calcExpiry < expiry)
            result += 1 * Months;
        else
            result -= 1 * Months;
        calcExpiry = fec->expiryDate(result, 0);
        maxIncrement--;
    }

    QL_REQUIRE(calcExpiry == expiry, "Expected the calculated expiry, " << iso_date(calcExpiry)
                                                                        << ", to equal the expiry, " << iso_date(expiry)
                                                                        << ".");

    return result;
}

template <class Interpolator>
boost::shared_ptr<CommodityIndexedCashFlow>
CommodityBasisPriceCurve<Interpolator>::makeCashflow(const QuantLib::Date& start, const QuantLib::Date& end) const {

    return boost::make_shared<CommodityIndexedCashFlow>(
        1.0, start, end, index_, 0, QuantLib::NullCalendar(), QuantLib::Unadjusted, 0, QuantLib::NullCalendar(), 0.0,
        1.0, CommodityIndexedCashFlow::PaymentTiming::InArrears, true, true, true, 0, baseFec_);
}

} // namespace QuantExt

#endif
