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
#include <ql/time/dategenerationrule.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>

#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {

using namespace QuantLib;

class BaseCorrelationTermStructure : public CorrelationTermStructure {
public:
    BaseCorrelationTermStructure(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc,
                                 const std::vector<Period>& tenors, 
                                 const std::vector<Real>& lossLevel,
                                 const DayCounter& dc = DayCounter(), const Date& startDate = Date(),
                                 boost::optional<DateGeneration::Rule> rule = boost::none);

    BaseCorrelationTermStructure(Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                                 const std::vector<Period>& tenors,
                                 const std::vector<Real>& lossLevel,
                                 const DayCounter& dc = DayCounter(), const Date& startDate = Date(),
                                 boost::optional<DateGeneration::Rule> rule = boost::none);

    virtual ~BaseCorrelationTermStructure() = default;

    // TermStructure interface
    Date maxDate() const override { return dates_.back(); }

    std::vector<double> times() const { return times_; }

    std::vector<double> lossLevels() const { return detachmentPoints_; }

    std::vector<Date> dates() const { return dates_; }

private:
    void validate() const;

    void initializeDatesAndTimes(const std::vector<Period>& tenors, const Date& startDate, BusinessDayConvention bdc,
                                  boost::optional<DateGeneration::Rule> rule) const;
    
protected:    
    virtual void checkRange(Time t, Real strike, bool extrapolate) const override;

    std::vector<Period> tenors_;
    std::vector<Real> detachmentPoints_;
    mutable std::vector<Date> dates_;
    mutable std::vector<Time> times_;
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
        : BaseCorrelationTermStructureBase(settlementDays, cal, bdc, tenors, lossLevel, dc, startDate, rule),
          correlHandles_(correls), nLosses_(lossLevel.size()), interpolator_(interpolator),
          data_(tenors.size(), detachmentPoints_.size(), 0.0) {
        
        

        // Initialize quotes
        QL_REQUIRE(baseCorrelations.size() == times.size(), "Mismatch between tenors and correlation quotes");
        for (const auto& row : baseCorrelations) {
            QL_REQUIRE(row.size() == detachmentPoints_.size(),
                       "Mismatch between number of detachment points and quotes");
            data_.push_back(vector<double>(row.size(), 0.0));
        }

        this->interpolation_ =
            this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->detachmentPoints_.begin(),
                                            this->detachmentPoints_.end(), this->data_);
        this->interpolation_.update();

        // register with each of the quotes
        for (Size i = 0; i < this->quotes_.size(); i++) {
            QL_REQUIRE(this->quotes_[i]->value() >= 0.0 && this->quotes_[i]->value() <= 1.0,
                       "correlation not in range (0.0,1.0): " << this->data_[i]);

            registerWith(this->quotes_[i]);
        }
    }

 //@}
    //! \name TermStructure interface
    //@{
    Date maxDate() const override { return Date::maxDate(); } // flat extrapolation
    Time maxTime() const override { return QL_MAX_REAL; }

    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    Interpolator interpolator_;
        
    boost::shared_ptr<Interpolation> interpolation_;


protected:
    //! \name CorrelationTermStructure implementation
    //@{
    Real correlationImpl(Time t, Real) const override;
    //@}
    std::vector<std::vector<<Handle<Quote>>> quotes_;
    vector<vector<double>> data_;
};

template <class Interpolator>
void InterpolatedBaseCorrelationTermStructure<Interpolator>::performCalculations() const {
    
    for (Size i = 0; i < this->times_.size(); ++i)
        for (Size j = 0; j < this->detachmentPoints_.size(); ++j)
            this->data_[i][j] = quotes_[i][j]->value();
    this->interpolation_ =
        this->interpolator_.interpolate(this->times_.begin(), this->times_.end(), this->detachmentPoints_.begin(),
                                        this->detachmentPoints_.end(), this->data_);
    this->interpolation_.update();
}


template <class Interpolator> void InterpolatedBaseCorrelationTermStructure<Interpolator>::update() {
    LazyObject::update();
    CorrelationTermStructure::update();
}

template <class T> Real InterpolatedBaseCorrelationTermStructure<T>::correlationImpl(Time t, Real x) const {
    calculate();
    return this->interpolation_(t, x, true);
}

typedef InterpolatedBaseCorrelationTermStructure<QuantLib::Bilinear> BilinearBaseCorrelationCurve;

}; // namespace QuantExt
