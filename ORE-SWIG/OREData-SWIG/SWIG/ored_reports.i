/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#ifndef ored_reports_i
#define ored_reports_i

%include boost_shared_ptr.i

%{
using ore::data::InMemoryReport;
using ore::data::PlainInMemoryReport;
%}

%shared_ptr(PlainInMemoryReport)
class PlainInMemoryReport {
public:
    PlainInMemoryReport(const ext::shared_ptr<InMemoryReport>& imReport);
    Size columns() const;
    std::string header(Size i);
    Size columnType(Size i) const;
    std::vector<int> dataAsSize(Size i) const;
    std::vector<Real> dataAsReal(Size i) const;
    std::vector<std::string> dataAsString(Size i) const;
    std::vector<QuantLib::Date> dataAsDate(Size i) const;
    std::vector<QuantLib::Period> dataAsPeriod(Size i) const;
    Size rows() const;
    int dataAsSize(Size j, Size i) const;
    Real dataAsReal(Size j, Size i) const;
    std::string dataAsString(Size j, Size i) const;
    QuantLib::Date dataAsDate(Size j, Size i) const;
    QuantLib::Period dataAsPeriod(Size j, Size i) const;
};

#endif
