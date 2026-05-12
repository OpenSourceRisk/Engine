/*
 Copyright (C) 2023, 2025 Quaternion Risk Management Ltd
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
%include std_string.i
%include std_vector.i

%{
#include <ored/report/report.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/report/csvreport.hpp>
%}

%shared_ptr(ore::data::InMemoryReport)
%shared_ptr(ore::data::CSVFileReport)
%shared_ptr(ore::data::PlainInMemoryReport)

namespace ore {
namespace data {

class InMemoryReport {
public:
    InMemoryReport(QuantLib::Size bufferSize = 0);

    QuantLib::Size columns() const;
    QuantLib::Size rows() const;
    const std::string& header(QuantLib::Size i) const;
    bool hasHeader(std::string h) const;

    void end();

    void toFile(const std::string& filename, const char sep = ',',
                const bool commentCharacter = true, char quoteChar = '\0',
                const std::string& nullString = "#N/A", bool lowerHeader = false);

    %extend {
        ore::data::InMemoryReport& addColumnSize(const std::string& name, QuantLib::Size precision = 0) {
            self->addColumn(name, QuantLib::Size(0), precision);
            return *self;
        }
        ore::data::InMemoryReport& addColumnReal(const std::string& name, QuantLib::Size precision = 6, bool scientific = false) {
            self->addColumn(name, QuantLib::Real(0.0), precision, scientific);
            return *self;
        }
        ore::data::InMemoryReport& addColumnString(const std::string& name) {
            self->addColumn(name, std::string());
            return *self;
        }
        ore::data::InMemoryReport& addColumnDate(const std::string& name) {
            self->addColumn(name, QuantLib::Date());
            return *self;
        }
        ore::data::InMemoryReport& addColumnPeriod(const std::string& name) {
            self->addColumn(name, QuantLib::Period());
            return *self;
        }

        ore::data::InMemoryReport& nextRow() {
            self->next();
            return *self;
        }

        ore::data::InMemoryReport& addSize(QuantLib::Size value) {
            self->add(value);
            return *self;
        }
        ore::data::InMemoryReport& addReal(QuantLib::Real value) {
            self->add(value);
            return *self;
        }
        ore::data::InMemoryReport& addString(const std::string& value) {
            self->add(value);
            return *self;
        }
        ore::data::InMemoryReport& addDate(const QuantLib::Date& value) {
            self->add(value);
            return *self;
        }
        ore::data::InMemoryReport& addPeriod(const QuantLib::Period& value) {
            self->add(value);
            return *self;
        }
    }
};

class CSVFileReport {
public:
    CSVFileReport(const std::string& filename, const char sep = ',',
                  const bool commentCharacter = true, char quoteChar = '\0',
                  const std::string& nullString = "#N/A", bool lowerHeader = false);

    void end();
    void flush();

    %extend {
        ore::data::CSVFileReport& addColumnSize(const std::string& name, QuantLib::Size precision = 0) {
            self->addColumn(name, QuantLib::Size(0), precision);
            return *self;
        }
        ore::data::CSVFileReport& addColumnReal(const std::string& name, QuantLib::Size precision = 6, bool scientific = false) {
            self->addColumn(name, QuantLib::Real(0.0), precision, scientific);
            return *self;
        }
        ore::data::CSVFileReport& addColumnString(const std::string& name) {
            self->addColumn(name, std::string());
            return *self;
        }
        ore::data::CSVFileReport& addColumnDate(const std::string& name) {
            self->addColumn(name, QuantLib::Date());
            return *self;
        }
        ore::data::CSVFileReport& addColumnPeriod(const std::string& name) {
            self->addColumn(name, QuantLib::Period());
            return *self;
        }
        ore::data::CSVFileReport& nextRow() {
            self->next();
            return *self;
        }
        ore::data::CSVFileReport& addSize(QuantLib::Size value) {
            self->add(value);
            return *self;
        }
        ore::data::CSVFileReport& addReal(QuantLib::Real value) {
            self->add(value);
            return *self;
        }
        ore::data::CSVFileReport& addString(const std::string& value) {
            self->add(value);
            return *self;
        }
        ore::data::CSVFileReport& addDate(const QuantLib::Date& value) {
            self->add(value);
            return *self;
        }
        ore::data::CSVFileReport& addPeriod(const QuantLib::Period& value) {
            self->add(value);
            return *self;
        }
    }
};

class PlainInMemoryReport {
public:
    PlainInMemoryReport(const ext::shared_ptr<ore::data::InMemoryReport>& imReport);
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

} // namespace data
} // namespace ore

#if defined(SWIGPYTHON)
%pythoncode %{
def _report_add_dispatch(self, value):
    """Add a value to the current row. Type is inferred from the Python object."""
    if isinstance(value, int):
        return self.addSize(value)
    elif isinstance(value, float):
        return self.addReal(value)
    elif isinstance(value, str):
        return self.addString(value)
    elif isinstance(value, Date):
        return self.addDate(value)
    elif isinstance(value, Period):
        return self.addPeriod(value)
    raise TypeError(f"Unsupported report value type: {type(value)}")

InMemoryReport.add = _report_add_dispatch
CSVFileReport.add = _report_add_dispatch

def _report_add_column_dispatch(self, name, col_type="string", precision=6, scientific=False):
    """Add a column. col_type is one of: 'size', 'real', 'string', 'date', 'period'."""
    if col_type == "size":
        return self.addColumnSize(name, precision)
    elif col_type == "real":
        return self.addColumnReal(name, precision, scientific)
    elif col_type == "string":
        return self.addColumnString(name)
    elif col_type == "date":
        return self.addColumnDate(name)
    elif col_type == "period":
        return self.addColumnPeriod(name)
    raise ValueError(f"Unknown column type: {col_type}")

InMemoryReport.addColumn = _report_add_column_dispatch
CSVFileReport.addColumn = _report_add_column_dispatch
%}
#endif

#endif
