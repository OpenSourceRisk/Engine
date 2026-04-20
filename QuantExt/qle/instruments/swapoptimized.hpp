/*
 Copyright (C) 2026 AcadiaSoft, Inc.

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

/*! \file qle/instruments/fxforwardoptimized.hpp
    \brief fxforward instrument, speed optimized for use in exposure simulation
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/instrument.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/timeseries.hpp>

namespace QuantExt {
using namespace QuantLib;

class SwapOptimized : public Instrument {
public:
    SwapOptimized(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                  const Handle<YieldTermStructure>& discountCurve);
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override {}
    void fetchResults(const PricingEngine::results*) const override {}
    void performCalculations() const override;

private:
    enum class Type { Fixed, Ibor };
    struct CashflowData {
        Date payDate_;
        Date fixingDate_;
        Date index_d1_;
        Date index_d2_;
        Real multiplier_;
        Real amount_;
        Real gearing_;
        Real spread_;
        Real accrualPeriod_;
        Real indexPeriod_;
        YieldTermStructure* forwardCurve_;
        const TimeSeries<Real>* indexTimeSeries_;
        std::string indexName_;
        Type type_;
    };

    std::vector<CashflowData> cashflowData_;

    Handle<YieldTermStructure> discountCurve_;
};

} // namespace QuantExt
