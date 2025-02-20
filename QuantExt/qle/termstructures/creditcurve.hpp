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

/*! \file qle/termstructures/creditcurve.hpp
    \brief wrapper for default curves, adding (index) reference data
*/

#pragma once

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {

class CreditCurve : public QuantLib::Observer, public QuantLib::Observable {
public:
    enum class Seniority {
        SNRFOR,
        SNRLAC,
        SUBLT2,
        SECDOM,
    };

    struct RefData {
        RefData() {}
        QuantLib::Date startDate = QuantLib::Null<QuantLib::Date>();
        QuantLib::Period indexTerm = 0 * QuantLib::Days;
        QuantLib::Period tenor = 3 * QuantLib::Months;
        QuantLib::Calendar calendar = QuantLib::WeekendsOnly();
        QuantLib::BusinessDayConvention convention = QuantLib::Following;
        QuantLib::BusinessDayConvention termConvention = QuantLib::Following;
        QuantLib::DateGeneration::Rule rule = QuantLib::DateGeneration::CDS2015;
        bool endOfMonth = false;
        QuantLib::Real runningSpread = QuantLib::Null<QuantLib::Real>();
        QuantLib::BusinessDayConvention payConvention = QuantLib::Following;
        QuantLib::DayCounter dayCounter = QuantLib::Actual360(false);
        QuantLib::DayCounter lastPeriodDayCounter = QuantLib::Actual360(true);
        QuantLib::Natural cashSettlementDays = 3;
        Seniority seniority = Seniority::SNRFOR;

    };

    explicit CreditCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& rateCurve =
                             QuantLib::Handle<QuantLib::YieldTermStructure>(),
                         const QuantLib::Handle<QuantLib::Quote>& recovery = QuantLib::Handle<QuantLib::Quote>(),
                         const RefData& refData = RefData());

    const RefData& refData() const;
    const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure>& rateCurve() const;
    const QuantLib::Handle<QuantLib::Quote>& recovery() const;

protected:
    QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> curve_;
    QuantLib::Handle<QuantLib::YieldTermStructure> rateCurve_;
    QuantLib::Handle<QuantLib::Quote> recovery_;
    RefData refData_;
    void update() override;
};

//! If no seniority is given, the default is Seniority::SNRFOR
CreditCurve::Seniority parseSeniority(const std::string& seniority);

} // namespace QuantExt
