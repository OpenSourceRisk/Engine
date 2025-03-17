/*
 Copyright (C) 2025 AcadiaSoft Inc.
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

#pragma once
#include <ql/instrument.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

class CreditIndexConstituentCurveCalibration {
public:
    struct CalibrationResults {
        bool success = false;
        std::vector<Handle<DefaultProbabilityTermStructure>> curves;
        std::vector<Date> cdsMaturity;
        std::vector<double> marketNpv;
        std::vector<double> impliedNpv;
        std::vector<double> calibrationFactor;
        std::string errorMessage;
    };

    CreditIndexConstituentCurveCalibration(const Date& indexStartDate, const Period indexTenor,
                                           const double indexSpread, const Handle<Quote>& indexRecoveryRate,
                                           const Handle<DefaultProbabilityTermStructure>& indexCurve,
                                           const Handle<YieldTermStructure>& discountCurve)
        : indexStartDate_(indexStartDate), indexTenors_({indexTenor}), indexSpread_(indexSpread),
          indexRecoveryRates_({indexRecoveryRate}), indexCurves_({indexCurve}), discountCurve_(discountCurve) {}

    CreditIndexConstituentCurveCalibration(const Date& indexStartDate, const std::vector<Period> indexTenors,
                                           const double indexSpread,
                                           const std::vector<Handle<Quote>>& indexRecoveryRates,
                                           const std::vector<Handle<DefaultProbabilityTermStructure>>& indexCurves,
                                           const Handle<YieldTermStructure>& discountCurve)
        : indexStartDate_(indexStartDate), indexTenors_(indexTenors), indexSpread_(indexSpread),
          indexRecoveryRates_(indexRecoveryRates), indexCurves_(indexCurves), discountCurve_(discountCurve) {}

    CalibrationResults calibratedCurves(const std::vector<std::string>& names,
                                        const std::vector<double>& remainingNotionals,
                                        const std::vector<Handle<DefaultProbabilityTermStructure>>& creditCurves,
                                        const std::vector<double>& recoveryRates) const;
    const std::vector<Handle<QuantLib::DefaultProbabilityTermStructure>>& indexCurve() const { return indexCurves_; }
    const Handle<QuantLib::YieldTermStructure>& discountCurve() const { return discountCurve_; }

protected:
    double targetNpv(const ext::shared_ptr<Instrument>& indexCds, size_t i) const;

private:
    QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>
    buildShiftedCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
                      const std::vector<QuantLib::Date>& maturities,
                      const std::vector<QuantLib::ext::shared_ptr<SimpleQuote>>& calibrationFactors) const;
    Date indexStartDate_;
    std::vector<Period> indexTenors_;
    double indexSpread_;
    std::vector<Handle<Quote>> indexRecoveryRates_;
    std::vector<Handle<DefaultProbabilityTermStructure>> indexCurves_;
    Handle<YieldTermStructure> discountCurve_;
};
} // namespace QuantExt