/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/report/report.hpp
    \brief Report interface class
    \ingroup report
*/

#pragma once

#include <boost/variant.hpp>
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>
#include <string>

namespace ore {
namespace data {
using QuantLib::Date;
using QuantLib::Period;
using QuantLib::Real;
using QuantLib::Size;
using std::string;

/*! Abstract Report interface class
 *
 *  A Report can be thought of as a CSV file or SQL table, it has columns (each with a name and type) which
 *  are set before we add any data, then each row of data is added with calls to add().
 *
 *  ReportType is a boost::variant which covers all the allowable types for a report.
 *
 *  Usage of the report API is as follows
 *  <pre>
 *   Report npv_report = makeReport();
 *
 *   // create headers
 *   npv_report.addColumn("Id", string())
 *             .addColumn("NPV", double(), 2)
 *             .addColumn("CP", string());
 *
 *   // add rows
 *   npv_report.next().add("t1").add(123.45).add("cp");
 *   npv_report.next().add("t2").add(3.14).add("cp");
 *   npv_report.next().add("t3").add(100.0).add("cp2");
 *   npv_report.end();
 *   </pre>
  \ingroup report
 */
class Report {
public:
    typedef boost::variant<Size, Real, string, Date, Period> ReportType;

    virtual ~Report() {}
    virtual Report& addColumn(const string& name, const ReportType&, Size precision = 0) = 0;
    virtual Report& next() = 0;
    virtual Report& add(const ReportType& rt) = 0;
    virtual void end() = 0;
    // make sure that (possibly) buffered output data is written to the result object (e.g. a file)
    virtual void flush() {}
};
} // namespace data
} // namespace ore
