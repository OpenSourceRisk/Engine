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

#include <boost/variant/static_visitor.hpp>
#include <ored/report/csvreport.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

// Local class for printing each report type via fprintf
class ReportTypePrinter : public boost::static_visitor<> {
public:
    ReportTypePrinter(FILE* fp, int prec) : fp_(fp), prec_(prec) {}

    void operator()(const Size i) const { fprintf(fp_, "%lu", i); }
    void operator()(const Real d) const { fprintf(fp_, "%.*f", prec_, d); }
    void operator()(const string& s) const { fprintf(fp_, "%s", s.c_str()); }
    void operator()(const Date& d) const {
        string s = to_string(d);
        fprintf(fp_, "%s", s.c_str());
    }
    void operator()(const Period& p) const {
        string s = to_string(p);
        fprintf(fp_, "%s", s.c_str());
    }

private:
    FILE* fp_;
    int prec_;
};

CSVFileReport::CSVFileReport(const string& filename, const char sep)
    : filename_(filename), sep_(sep), i_(0), fp_(NULL) {
    fp_ = fopen(filename_.c_str(), "w+");
    QL_REQUIRE(fp_, "Error opening file " << filename_);
}

CSVFileReport::~CSVFileReport() { end(); }

Report& CSVFileReport::addColumn(const string& name, const ReportType& rt, Size precision) {
    columnTypes_.push_back(rt);
    printers_.push_back(ReportTypePrinter(fp_, precision));
    fprintf(fp_, "%c%s", (i_ == 0 ? '#' : sep_), name.c_str());
    i_++;
    return *this;
}

Report& CSVFileReport::next() {
    QL_REQUIRE(i_ == columnTypes_.size(), "Cannot go to next line, only " << i_ << " entires filled");
    fprintf(fp_, "\n");
    i_ = 0;
    return *this;
}

Report& CSVFileReport::add(const ReportType& rt) {
    QL_REQUIRE(i_ < columnTypes_.size(), "No column to add [" << rt << "] to.");
    QL_REQUIRE(rt.which() == columnTypes_[i_].which(), "Cannot add value " << rt << " of type " << rt.which()
                                                                           << " to column " << i_ << " of type "
                                                                           << columnTypes_[i_].which());

    if (i_ != 0)
        fprintf(fp_, "%c", sep_);
    boost::apply_visitor(printers_[i_], rt);
    i_++;
    return *this;
}
void CSVFileReport::end() {
    if (fp_) {
        fprintf(fp_, "\n");
        fclose(fp_);
        fp_ = NULL;
    }
}
} // namespace data
} // namespace ore
