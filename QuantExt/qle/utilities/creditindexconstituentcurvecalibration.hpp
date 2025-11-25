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
#include <qle/termstructures/creditcurve.hpp>

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

    CreditIndexConstituentCurveCalibration(
        const Date& indexStartDate, const Period& indexTerm, const double indexSpread,
        const Handle<Quote>& indexRecoveryRate, const Handle<DefaultProbabilityTermStructure>& indexCurve,
        const Handle<YieldTermStructure>& discountCurve,
        QuantLib::Period tenor = 3 * Months,
        QuantLib::Calendar calendar = QuantLib::WeekendsOnly(),
        QuantLib::BusinessDayConvention convention = QuantLib::Following,
        QuantLib::BusinessDayConvention termConvention = QuantLib::Unadjusted,
        QuantLib::DateGeneration::Rule rule = QuantLib::DateGeneration::CDS2015, bool endOfMonth = false,
        QuantLib::BusinessDayConvention payConvention = QuantLib::Following,
        QuantLib::DayCounter dayCounter = QuantLib::Actual360(false),
        QuantLib::DayCounter lastPeriodDayCounter = QuantLib::Actual360(true), QuantLib::Natural cashSettlementDays = 3)
        : startDate_(indexStartDate), indexTerm_(indexTerm), tenor_(tenor),
          calendar_(calendar), convention_(convention), termConvention_(termConvention), rule_(rule),
          endOfMonth_(endOfMonth), runningSpread_(indexSpread), payConvention_(payConvention),
          dayCounter_(dayCounter), lastPeriodDayCounter_(lastPeriodDayCounter),
          cashSettlementDays_(cashSettlementDays), indexRecoveryRate_(indexRecoveryRate),
          indexCurve_(indexCurve), discountCurve_(discountCurve) {
        QL_REQUIRE(startDate_ != Null<Date>(), "CreditIndexConstituentCurveCalibration: Index start date is null");
        QL_REQUIRE(indexTerm_ != 0 * Days, "CreditIndexConstituentCurveCalibration: Index term is null");
        QL_REQUIRE(runningSpread_ != Null<double>(), "CreditIndexConstituentCurveCalibration: Index running spread is null");
        QL_REQUIRE(!indexCurve_.empty(), "CreditIndexConstituentCurveCalibration: Index curve handle is empty");
        QL_REQUIRE(!indexRecoveryRate_.empty(), "CreditIndexConstituentCurveCalibration: Index recovery rate handle is empty");
        QL_REQUIRE(!discountCurve_.empty(), "CreditIndexConstituentCurveCalibration: Discount curve handle is empty");
    }

    CreditIndexConstituentCurveCalibration(const Handle<CreditCurve>& indexRefData)
        : CreditIndexConstituentCurveCalibration(
              indexRefData->refData().startDate, indexRefData->refData().indexTerm,
              indexRefData->refData().runningSpread, indexRefData->recovery(), indexRefData->curve(),
              indexRefData->rateCurve(), indexRefData->refData().tenor, indexRefData->refData().calendar,
              indexRefData->refData().convention, indexRefData->refData().termConvention, indexRefData->refData().rule,
              indexRefData->refData().endOfMonth, indexRefData->refData().payConvention,
              indexRefData->refData().dayCounter, indexRefData->refData().lastPeriodDayCounter,
              indexRefData->refData().cashSettlementDays) {
        lastPeriodDayCounter_ = lastPeriodDayCounter_ == DayCounter() ? Actual360(true) : lastPeriodDayCounter_;
    }

    CreditIndexConstituentCurveCalibration(const boost::shared_ptr<CreditCurve>& indexRefData)
        : CreditIndexConstituentCurveCalibration(
              indexRefData->refData().startDate, indexRefData->refData().indexTerm,
              indexRefData->refData().runningSpread, indexRefData->recovery(), indexRefData->curve(),
              indexRefData->rateCurve(), indexRefData->refData().tenor, indexRefData->refData().calendar,
              indexRefData->refData().convention, indexRefData->refData().termConvention, indexRefData->refData().rule,
              indexRefData->refData().endOfMonth, indexRefData->refData().payConvention,
              indexRefData->refData().dayCounter, indexRefData->refData().lastPeriodDayCounter,
              indexRefData->refData().cashSettlementDays) {
        lastPeriodDayCounter_ = lastPeriodDayCounter_ == DayCounter() ? Actual360(true) : lastPeriodDayCounter_;
    }

    CalibrationResults calibratedCurves(const std::vector<std::string>& names,
                                        const std::vector<double>& remainingNotionals,
                                        const std::vector<Handle<DefaultProbabilityTermStructure>>& creditCurves,
                                        const std::vector<double>& recoveryRates) const;
    const Handle<QuantLib::DefaultProbabilityTermStructure>& indexCurve() const { return indexCurve_; }
    const Handle<QuantLib::YieldTermStructure>& discountCurve() const { return discountCurve_; }

protected:
    double targetNpv(const ext::shared_ptr<Instrument>& indexCds) const;

private:
    QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>
    buildShiftedCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
                      const QuantLib::Date& maturity,
                      const QuantLib::ext::shared_ptr<SimpleQuote>& calibrationFactor) const;
    // Index Reference Data
    QuantLib::Date startDate_;
    QuantLib::Period indexTerm_;
    QuantLib::Period tenor_;
    // Index Conventions
    QuantLib::Calendar calendar_;
    QuantLib::BusinessDayConvention convention_;
    QuantLib::BusinessDayConvention termConvention_;
    QuantLib::DateGeneration::Rule rule_;
    bool endOfMonth_;
    QuantLib::Real runningSpread_;
    QuantLib::BusinessDayConvention payConvention_;
    QuantLib::DayCounter dayCounter_;
    QuantLib::DayCounter lastPeriodDayCounter_;
    QuantLib::Natural cashSettlementDays_;
    // Index Market Data
    Handle<Quote> indexRecoveryRate_;
    Handle<DefaultProbabilityTermStructure> indexCurve_;
    Handle<YieldTermStructure> discountCurve_;
};
} // namespace QuantExt