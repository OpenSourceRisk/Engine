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

#include <orea/scenario/scenariowriter.hpp>
#include <ored/utilities/to_string.hpp>

using ore::data::to_string;

namespace ore {
namespace analytics {

ScenarioWriter::ScenarioWriter(const QuantLib::ext::shared_ptr<ScenarioGenerator>& src, const std::string& filename,
                               const char sep, const string& filemode, const std::vector<RiskFactorKey>& headerKeys)
    : src_(src), fp_(nullptr), i_(0), sep_(sep), headerKeys_(headerKeys) {
    open(filename, filemode);
}

ScenarioWriter::ScenarioWriter(const std::string& filename, const char sep, const string& filemode,
                               const std::vector<RiskFactorKey>& headerKeys)
    : fp_(nullptr), i_(0), sep_(sep), headerKeys_(headerKeys) {
    open(filename, filemode);
}

ScenarioWriter::ScenarioWriter(const QuantLib::ext::shared_ptr<ScenarioGenerator>& src,
                               QuantLib::ext::shared_ptr<ore::data::Report> report,
                               const std::vector<RiskFactorKey>& headerKeys)
    : src_(src), report_(report), fp_(nullptr), i_(0), sep_(','), headerKeys_(headerKeys) {}

void ScenarioWriter::open(const std::string& filename, const std::string& filemode) {
    fp_ = fopen(filename.c_str(), filemode.c_str());
    QL_REQUIRE(fp_, "Error opening file " << filename << " for scenarios");
}

ScenarioWriter::~ScenarioWriter() { close(); }

void ScenarioWriter::reset() {
    if (src_)
        src_->reset();
    close();
}

void ScenarioWriter::close() {
    if (fp_) {
        fclose(fp_);
        fp_ = nullptr;
    }
    if (report_)
        report_->end();
}

QuantLib::ext::shared_ptr<Scenario> ScenarioWriter::next(const Date& d) {
    QL_REQUIRE(src_, "No ScenarioGenerator found.");
    QuantLib::ext::shared_ptr<Scenario> s = src_->next(d);
    writeScenario(s, i_ == 0);
    return s;
}

void ScenarioWriter::writeScenario(const QuantLib::ext::shared_ptr<Scenario>& s, const bool writeHeader) {
    const Date d = s->asof();
    // take a copy of the keys here to ensure the order is preserved
    keys_ = s->keys();
    std::sort(keys_.begin(), keys_.end());
    if (fp_) {
        if (writeHeader) {
            QL_REQUIRE(keys_.size() > 0, "No keys in scenario");
            fprintf(fp_, "Date%cScenario%cNumeraire%c%s", sep_, sep_, sep_, to_string(keys_[0]).c_str());
            for (Size i = 1; i < keys_.size(); i++)
                fprintf(fp_, "%c%s", sep_, to_string(keys_[i]).c_str());
            fprintf(fp_, "\n");

            // set the first date, this will bump i_ to 1 below
            firstDate_ = d;
        }
        if (d == firstDate_)
            i_++;

        fprintf(fp_, "%s%c%zu%c%.8f", to_string(d).c_str(), sep_, i_, sep_, s->getNumeraire());
        for (auto k : keys_)
            fprintf(fp_, "%c%.8f", sep_, s->get(k));
        fprintf(fp_, "\n");
        fflush(fp_);
    }

    if (report_) {
        if (writeHeader) {
            QL_REQUIRE(keys_.size() > 0, "No keys in scenario");
            if (headerKeys_.empty())
                headerKeys_ = keys_;
            report_->addColumn("Date", string());
            report_->addColumn("Scenario", Size());
            report_->addColumn("Numeraire", double(), 8);
            for (Size i = 0; i < headerKeys_.size(); i++)
                report_->addColumn(to_string(headerKeys_[i]), double(), 8);
            // set the first date, this will bump i_ to 1 below
            firstDate_ = d;
        }
        if (d == firstDate_)
            i_++;
        report_->next();
        report_->add(to_string(d));
        report_->add(i_);
        report_->add(s->getNumeraire());
        for (auto k : headerKeys_) {
            if (s->has(k))
                report_->add(s->get(k));
            else
                report_->add(QuantLib::Null<QuantLib::Real>());
        }
    }
}

} // namespace analytics
} // namespace ore
