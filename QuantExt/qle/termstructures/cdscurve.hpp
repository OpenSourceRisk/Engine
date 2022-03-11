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

/*! \file cdscurve.hpp
    \brief default curve for cds and index cds
*/

#pragma once

#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {

class CdsCurve : public QuantLib::Observer, public QuantLib::Observable {
public:
    struct RefData {
	std::string type = "SingleName"; // Index, SingleName
        QuantLib::Date startDate = QuantLib::Null<QuantLib::Date>();
	std::vector<QuantLib::Period> terms;
	std::vector<QuantLib::Date> terminationDates;
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
    };

    CdsCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve);
    CdsCurve(const std::vector<QuantLib::Period>& terms,
             const std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> termCurves,
             const RefData& RefData);

    const RefData& refData() const;
    QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> curve(const QuantLib::Period& term = 0 *
                                                                                                     QuantLib::Days);

protected:
    std::vector<QuantLib::Period> terms_;
    std::vector<QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>> termCurves_;
    RefData refData_;
    std::vector<QuantLib::Real> termTimes_;

    void update() override;
};

} // namespace QuantExt
