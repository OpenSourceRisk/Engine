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

/*! \file basecorrelationstructure.hpp
    \brief abstract base correlation structure and an 2d-interpolated base correlation structure
*/

#pragma once

#include <boost/optional/optional.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/time/dategenerationrule.hpp>

#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

class BaseCorrelationTermStructure : public CorrelationTermStructure {
public:
    BaseCorrelationTermStructure();

    BaseCorrelationTermStructure(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc,
                                 const std::vector<Period>& tenors, const std::vector<double>& detachmentPoints,
                                 const DayCounter& dc = DayCounter(), const Date& startDate = Date(),
                                 boost::optional<DateGeneration::Rule> rule = boost::none);

    BaseCorrelationTermStructure(Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                                 const std::vector<Period>& tenors, const std::vector<double>& detachmentPoints,
                                 const DayCounter& dc = DayCounter(), const Date& startDate = Date(),
                                 boost::optional<DateGeneration::Rule> rule = boost::none);

    virtual ~BaseCorrelationTermStructure() = default;

    // TermStructure interface
    virtual Date maxDate() const override { return dates_.back(); }

    virtual Time maxTime() const override { return times_.back(); }
    virtual Time minTime() const override { return times_.front(); }

    virtual double minDetachmentPoint() const { return detachmentPoints_.front(); }
    virtual double maxDetachmentPoint() const { return detachmentPoints_.back(); }

    std::vector<double> times() const { return times_; }

    std::vector<double> detachmentPoints() const { return detachmentPoints_; }

    std::vector<Date> dates() const { return dates_; }

    BusinessDayConvention businessDayConvention() const { return bdc_; }

    Date startDate() const { return startDate_; }

    boost::optional<DateGeneration::Rule> rule() const { return rule_; }

private:
    BusinessDayConvention bdc_;
    Date startDate_;
    boost::optional<DateGeneration::Rule> rule_;

    void validate() const;

    void initializeDatesAndTimes() const;

protected:
    virtual void checkRange(Time t, Real strike, bool extrapolate) const override;
    
    std::vector<Period> tenors_;
    std::vector<double> detachmentPoints_;
    mutable std::vector<Date> dates_;
    mutable std::vector<double> times_;
};

template <class Interpolator>
class InterpolatedBaseCorrelationTermStructure : public BaseCorrelationTermStructure, public LazyObject {
public:
    InterpolatedBaseCorrelationTermStructure(Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                                             const std::vector<Period>& tenors,
                                             const std::vector<Real>& detachmentPoints,
                                             const std::vector<std::vector<Handle<Quote>>>& baseCorrelations,
                                             const DayCounter& dc = DayCounter(), const Date& startDate = Date(),
                                             boost::optional<DateGeneration::Rule> rule = boost::none,
                                             Interpolator interpolator = Interpolator())
        : BaseCorrelationTermStructure(settlementDays, cal, bdc, tenors, detachmentPoints, dc, startDate, rule),
          quotes_(baseCorrelations), data_(detachmentPoints.size(), tenors.size(), 0.0), interpolator_(interpolator) {

        // check quotes
        QL_REQUIRE(quotes_.size() == detachmentPoints_.size(), "Mismatch between tenors and correlation quotes");
        for (const auto& row : this->quotes_) {
            QL_REQUIRE(row.size() == tenors_.size(),
                       "Mismatch between number of detachment points and quotes");
        }

        this->interpolation_ =
            this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->detachmentPoints_.begin(),
                                            this->detachmentPoints_.end(), this->data_);
        this->interpolation_.update();

        // register with each of the quotes
        for (Size i = 0; i < this->quotes_.size(); i++) {
            for (Size j = 0; j < this->quotes_[i].size(); ++j) {

                QL_REQUIRE(this->quotes_[i][j]->value() >= 0.0 && this->quotes_[i][j]->value() <= 1.0,
                           "correlation not in range (0.0,1.0): " << this->quotes_[i][j]->value());

                registerWith(this->quotes_[i][j]);
            }
        }
    }

    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
protected:
    //! \name CorrelationTermStructure implementation
    //@{
    Real correlationImpl(Time t, Real detachmentPoint) const override;
    //@}
    std::vector<std::vector<Handle<Quote>>> quotes_;
    mutable Matrix data_;


private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    Interpolator interpolator_;

    mutable Interpolation2D interpolation_;


};

template <class Interpolator> void InterpolatedBaseCorrelationTermStructure<Interpolator>::performCalculations() const {

    for (Size i = 0; i < this->detachmentPoints_.size(); ++i)
        for (Size j = 0; j < this->times_.size(); ++j)
            this->data_[i][j] = quotes_[i][j]->value();
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->detachmentPoints_.begin(),
                                        this->detachmentPoints_.end(), this->data_);
    this->interpolation_.update();
}

template <class Interpolator> void InterpolatedBaseCorrelationTermStructure<Interpolator>::update() {
    LazyObject::update();
    BaseCorrelationTermStructure::update();
}

template <class T>
Real InterpolatedBaseCorrelationTermStructure<T>::correlationImpl(Time t, Real detachmentPoint) const {
    calculate();
    return this->interpolation_(t, detachmentPoint, true);
}

typedef InterpolatedBaseCorrelationTermStructure<QuantLib::Bilinear> BilinearBaseCorrelationCurve;

}; // namespace QuantExt
