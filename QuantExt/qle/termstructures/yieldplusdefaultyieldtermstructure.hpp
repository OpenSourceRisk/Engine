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

/*! \file yieldplusdefaultyieldtermstructure.hpp
    \brief yield term structure given as a yield ts plus weighted sum of default term structures
    \ingroup models
*/

#ifndef quantext_yieldplusdefault_yts_hpp
#define quantext_yieldplusdefault_yts_hpp

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! yield plus default yield term structure
/*! this yield term structure is defined by discount factors given by a weighted sum of survival probabilities of
  underlying default curves plus the discount factor of a reference yield curve
*/
class YieldPlusDefaultYieldTermStructure : public YieldTermStructure {
public:
    YieldPlusDefaultYieldTermStructure(const Handle<YieldTermStructure>& yts,
                                       const std::vector<Handle<DefaultProbabilityTermStructure> >& df,
                                       const std::vector<Handle<Quote> >& rr, const std::vector<Real>& weights)
        : YieldTermStructure(yts->dayCounter()), yts_(yts), df_(df), rr_(rr), weights_(weights) {
        QL_REQUIRE(df_.size() == weights_.size(), "YieldPlusDefaultYieldTermStructure: default curve size ("
                                                      << df_.size() << ") must match weights size (" << weights_.size()
                                                      << ")");
        QL_REQUIRE(df_.size() == rr_.size(), "YieldPlusDefaultYieldTermStructure: rec rate size ("
                                                 << rr_.size() << ") must match weights size (" << weights_.size()
                                                 << ")");
        // todo check consistent day counters?
        registerWith(yts);
        for (Size i = 0; i < df_.size(); ++i)
            registerWith(df_[i]);
        for (Size i = 0; i < rr_.size(); ++i)
            registerWith(rr_[i]);
    }
    Date maxDate() const override;
    const Date& referenceDate() const override;

protected:
    Real discountImpl(Time t) const override;
    const Handle<YieldTermStructure> yts_;
    const std::vector<Handle<DefaultProbabilityTermStructure> > df_;
    const std::vector<Handle<Quote> > rr_;
    const std::vector<Real> weights_;
};

// inline

inline Date YieldPlusDefaultYieldTermStructure::maxDate() const { return yts_->maxDate(); }

inline const Date& YieldPlusDefaultYieldTermStructure::referenceDate() const {
    // todo check consistent reference dates?
    return yts_->referenceDate();
}

inline Real YieldPlusDefaultYieldTermStructure::discountImpl(Time t) const {
    Real d = yts_->discount(t);
    for (Size i = 0; i < df_.size(); ++i) {
        // use implied surv prob adjusted by a factor corresponding to a market value recovery model:
        // adj_S = S^{1-R}
        d *= std::pow(df_[i]->survivalProbability(t), weights_[i] * (1.0 - rr_[i]->value()));
    }
    return d;
}

} // namespace QuantExt

#endif
