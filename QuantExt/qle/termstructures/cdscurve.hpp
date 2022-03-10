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

#include <ql/termstructures/credit/defaulttermstructure.hpp>

namespace QuantExt {
using QuantLib::Date;

class CdsCurve {
public:
    class RefData {
        Date startDate_ = Null<Date>();
        Date terminationDate_ = Null<Date>();
        Period tenor = 3 * QuantLib::Months;
        Calendar calendar_ = WeekendsOnly();
        BusinessDayConvention convention_ = QuantLib::Following;
        BusinessDayConvention termConvention_ = QuantLib::Following;
        QuantLib::DateGeneration::Rule rule_ = QuantLib::CDS2015;
        bool endOfMonth_ = false;
        Real couponRate_ = Null<Real>();
        BusinessDayConvention payConvention_ = QuantLib::Following;
        DayCounter dayCounter_ = Actual360(false);
        DayCounter lastPeriodDayCounter_ = Actual360(true);
        Natural cashSettlementDays_ = 3;
    };

    CdsCurve(const Handle<DefaultProbabilityTermStructure>& curve);
    CdsCurve(const std::vector<Period>& terms, const std::vector<Handle<DefaultProbabilityTermStructure>> termCurves,
             const CdsRefData& RefData);

    const RefData& refData() const;
    Handle<DefaultProbabilityTermStructure> curve(const Period& term = 0 * Days);

protected:
    std::vector<Perio> terms_;
    std::vector<Handle<DefaultProbabilityTermStructure>> termCurves_;
    CdsRefData refData_;

    void performCalculations() const override;
};

} // namespace QuantExt
