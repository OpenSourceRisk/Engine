/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/quotes/basecorrelationquote.hpp
    \brief wrapper around base correlation term structure for given detachment point
*/

#ifndef quantext_basecorrelation_quote_hpp
#define quantext_basecorrelation_quote_hpp

#include <ql/errors.hpp>
#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <ql/handle.hpp>
#include <ql/quote.hpp>
#include <ql/types.hpp>

namespace QuantExt {

//! market element whose value depends on two other market element
/*! \test the correctness of the returned values is tested by
          checking them against numerical calculations.
*/
class BaseCorrelationQuote : public Quote, public Observer {
public:
    BaseCorrelationQuote(const Handle<QuantExt::BaseCorrelationTermStructure>& bcts, Period term, Real lossLevel,
                         bool extrapolate);
    //! \name inspectors
    //@{
    Handle<QuantExt::BaseCorrelationTermStructure> termStructure() const { return bcts_; }
    Period term() const { return term_; }
    Real lossLevel() const { return lossLevel_; }
    bool extrapolate() const { return extrapolate_; }
    //@}
    //! \name Quote interface
    //@{
    Real value() const override;
    bool isValid() const override;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
private:
    Handle<QuantExt::BaseCorrelationTermStructure> bcts_;
    Period term_;
    Real lossLevel_;
    bool extrapolate_;
};

// inline definitions


inline BaseCorrelationQuote::BaseCorrelationQuote(const Handle<QuantExt::BaseCorrelationTermStructure>& bcts,
                                                  Period term, Real lossLevel, bool extrapolate)
    : bcts_(bcts), term_(term), lossLevel_(lossLevel), extrapolate_(extrapolate) {
    QL_REQUIRE(lossLevel > 0.0 && lossLevel <= 1.0, "lossLevel " << lossLevel << " out of range");
    registerWith(bcts_);
}

inline Real BaseCorrelationQuote::value() const {
    QL_ENSURE(isValid(), "invalid BaseCorrelationQuote");
    Date ref = bcts_->referenceDate();
    Real c = bcts_->correlation(ref + term_, lossLevel_, extrapolate_);
    // FIXME this should be handled in the input correlation term structure
    return std::min(1.0 - QL_EPSILON, std::max(QL_EPSILON, c));
}

inline bool BaseCorrelationQuote::isValid() const {
    return !bcts_.empty();
}

inline void BaseCorrelationQuote::update() { notifyObservers(); }

} // namespace QuantExt

#endif
