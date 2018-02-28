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

#include <qle/instruments/impliedbondspread.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/math/solvers1d/brent.hpp>

using namespace QuantLib;

namespace QuantExt {

    namespace {

        class PriceError {
          public:
            PriceError(const PricingEngine& engine,
                       SimpleQuote& spread,
                       Real targetValue);
            Real operator()(Real spread) const;
          private:
            const PricingEngine& engine_;
            SimpleQuote& spread_;
            Real targetValue_;
            const Instrument::results* results_;
        };

        PriceError::PriceError(const PricingEngine& engine,
                               SimpleQuote& spread,
                               Real targetValue)
        : engine_(engine), spread_(spread), targetValue_(targetValue) {
            results_ =
                dynamic_cast<const Instrument::results*>(engine_.getResults());
            QL_REQUIRE(results_ != 0,
                       "pricing engine does not supply needed results");
        }

        Real PriceError::operator()(Real spread) const {
            spread_.setValue(spread);
            engine_.calculate();
            return results_->value-targetValue_;
        }

    }


    namespace detail {

        Real ImpliedBondSpreadHelper::calculate(
            const Instrument& instrument,
            const PricingEngine& engine,
            SimpleQuote& spreadQuote,
            Real targetValue,
            Real accuracy,
            Natural maxEvaluations,
            Real minSpread,
            Real maxSpread) {

            instrument.setupArguments(engine.getArguments());
            engine.getArguments()->validate();

            PriceError f(engine, spreadQuote, targetValue);
            Brent solver;
            solver.setMaxEvaluations(maxEvaluations);
            Volatility guess = (minSpread + maxSpread)/2.0;
            Volatility result = solver.solve(f, accuracy, guess,
                minSpread, maxSpread);
            return result;
        }

    }

}
