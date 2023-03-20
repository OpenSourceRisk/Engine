/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/indexcdsoptionbaseengine.hpp
    \brief Base class for index cds option pricing engines.
*/

#pragma once

#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/indexcdsoption.hpp>
#include <qle/termstructures/creditvolcurve.hpp>

namespace QuantExt {

class IndexCdsOptionBaseEngine : public QuantExt::IndexCdsOption::engine {
public:
    //! Constructor taking a default probability term structure bootstrapped from the index spreads.
    IndexCdsOptionBaseEngine(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& probability,
                             QuantLib::Real recovery, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
                             const QuantLib::Handle<QuantExt::CreditVolCurve>& volatility);

    /*! Constructor taking a vector of default probability term structures bootstrapped from the index constituent
        spread curves and a vector of associated recovery rates.
    */
    IndexCdsOptionBaseEngine(
        const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>& probabilities,
        const std::vector<QuantLib::Real>& recoveries, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
        const QuantLib::Handle<QuantExt::CreditVolCurve>& volatility,
        QuantLib::Real indexRecovery = QuantLib::Null<QuantLib::Real>());

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>>& probabilities() const;
    const std::vector<QuantLib::Real>& recoveries() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure> discount() const;
    const QuantLib::Handle<QuantExt::CreditVolCurve> volatility() const;
    //@}

    //! \name Instrument interface
    //@{
    void calculate() const override;
    //@}

protected:
    //! Engine specific calculation
    virtual void doCalc() const = 0;

    //! Register with market data
    void registerWithMarket();

    //! Calculate the discounted value of the front end protection.
    QuantLib::Real fep() const;

    //! Store inputs
    std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> probabilities_;
    std::vector<QuantLib::Real> recoveries_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discount_;
    QuantLib::Handle<QuantExt::CreditVolCurve> volatility_;

    //! Assumed index recovery used in the flat strike spread curve calculation if provided.
    QuantLib::Real indexRecovery_;

    //! Store the underlying index CDS notional(s) during calculation.
    mutable std::vector<QuantLib::Real> notionals_;
};

} // namespace QuantExt
