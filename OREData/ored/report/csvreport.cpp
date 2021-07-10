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
#include <ql/math/comparison.hpp>
#include <ql/math/rounding.hpp>

using std::string;

namespace ore {
namespace data {

// Local class for printing each report type via fprintf
class ReportTypePrinter : public boost::static_visitor<> {
public:
    ReportTypePrinter(FILE* fp, int prec, char quoteChar = '\0', const string& nullString = "#N/A")
        : fp_(fp), rounding_(prec, QuantLib::Rounding::Closest), quoteChar_(quoteChar), null_(nullString) {}

    void operator()(const Size i) const {
        if (i == QuantLib::Null<Size>()) {
            fprintNull();
        } else {
            fprintf(fp_, "%zu", i);
        }
    }
    void operator()(const Real d) const {
        if (d == QuantLib::Null<Real>() || !std::isfinite(d)) {
            fprintNull();
        } else {
            Real r = rounding_(d);
            fprintf(fp_, "%.*f", rounding_.precision(), QuantLib::close_enough(r, 0.0) ? 0.0 : r);
        }
    }
    void operator()(const string& s) const { fprintString(s); }
    void operator()(const Date& d) const {
        if (d == QuantLib::Null<Date>()) {
            fprintNull();
        } else {
            string s = to_string(d);
            fprintString(s);
        }
    }
    void operator()(const Period& p) const {
        string s = to_string(p);
        fprintString(s);
    }

private:
    void fprintNull() const { fprintf(fp_, "%s", null_.c_str()); }

    // Shared implementation to include the quote character.
    void fprintString(const string& s) const {
        if (quoteChar_ != '\0')
            fputc(quoteChar_, fp_);
        fprintf(fp_, "%s", s.c_str());
        if (quoteChar_ != '\0')
            fputc(quoteChar_, fp_);
    }

    FILE* fp_;
    QuantLib::Rounding rounding_;
    char quoteChar_;
    string null_;
};

CSVFileReport::CSVFileReport(const string& filename, const char sep, const bool commentCharacter, char quoteChar,
                             const string& nullString, bool lowerHeader)
    : filename_(filename), sep_(sep), commentCharacter_(commentCharacter), quoteChar_(quoteChar),
      nullString_(nullString), lowerHeader_(lowerHeader), i_(0), fp_(NULL) {
    fp_ = fopen(filename_.c_str(), "w+");
    QL_REQUIRE(fp_, "Error opening file " << filename_);
}

CSVFileReport::~CSVFileReport() { end(); }

void CSVFileReport::flush() { fflush(fp_); }

Report& CSVFileReport::addColumn(const string& name, const ReportType& rt, Size precision) {
    columnTypes_.push_back(rt);
    printers_.push_back(ReportTypePrinter(fp_, precision, quoteChar_, nullString_));
    if (i_ == 0 && commentCharacter_)
        fprintf(fp_, "#");
    if (i_ > 0)
        fprintf(fp_, "%c", sep_);
    string cpName = name;
    if (lowerHeader_ && !cpName.empty())
        cpName[0] = std::tolower(static_cast<unsigned char>(cpName[0]));
    fprintf(fp_, "%s", cpName.c_str());
    i_++;
    return *this;
}

Report& CSVFileReport::next() {
    QL_REQUIRE(i_ == columnTypes_.size(), "Cannot go to next line, only " << i_ << " entries filled");
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
