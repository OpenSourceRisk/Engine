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

#include <boost/make_shared.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/impliedbondspread.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {

class PriceError {
public:
    PriceError(const Bond& engine, SimpleQuote& spread, Real targetValue, bool isCleanPrice);
    Real operator()(Real spread) const;

private:
    const Bond& bond_;
    SimpleQuote& spread_;
    Real targetValue_;
    bool isCleanPrice_;
};

PriceError::PriceError(const Bond& bond, SimpleQuote& spread, Real targetValue, bool isCleanPrice)
    : bond_(bond), spread_(spread), targetValue_(targetValue), isCleanPrice_(isCleanPrice) {}

Real PriceError::operator()(Real spread) const {
    spread_.setValue(spread);
    Real guessValue = isCleanPrice_ ? bond_.cleanPrice() : bond_.dirtyPrice();
    return guessValue - targetValue_;
}

} // namespace

namespace detail {

Real ImpliedBondSpreadHelper::calculate(const QuantLib::ext::shared_ptr<Bond>& bond,
                                        const QuantLib::ext::shared_ptr<PricingEngine>& engine,
                                        const QuantLib::ext::shared_ptr<SimpleQuote>& spreadQuote, Real targetValue,
                                        bool isCleanPrice, Real accuracy, Natural maxEvaluations, Real minSpread,
                                        Real maxSpread) {

    Bond clonedBond = *bond;
    clonedBond.setPricingEngine(engine);
    clonedBond.recalculate();
    spreadQuote->setValue(0.005);

    PriceError f(clonedBond, *spreadQuote, targetValue, isCleanPrice);
    Brent solver;
    solver.setMaxEvaluations(maxEvaluations);
    Real guess = (minSpread + maxSpread) / 2.0;
    Real result = solver.solve(f, accuracy, guess, minSpread, maxSpread);
    return result;
}

} // namespace detail

} // namespace QuantExt
