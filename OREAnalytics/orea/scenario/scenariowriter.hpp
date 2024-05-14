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

/*! \file scenario/scenariowriter.hpp
    \brief ScenarioWriter class
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <ored/report/report.hpp>

namespace ore {
namespace analytics {

//! Class for writing scenarios to file.
class ScenarioWriter : public ScenarioGenerator {
public:
    //! Constructor
    ScenarioWriter(const QuantLib::ext::shared_ptr<ScenarioGenerator>& src, const std::string& filename, const char sep = ',',
                   const string& filemode = "w+", const std::vector<RiskFactorKey>& headerKeys = {});

    //! Constructor to write single scenarios
    ScenarioWriter(const std::string& filename, const char sep = ',', const string& filemode = "w+",
                   const std::vector<RiskFactorKey>& headerKeys = {});

    //! Constructor to write into an in-memory report for later io
    ScenarioWriter(const QuantLib::ext::shared_ptr<ScenarioGenerator>& src, QuantLib::ext::shared_ptr<ore::data::Report> report,
                   const std::vector<RiskFactorKey>& headerKeys = {});

    //! Destructor
    virtual ~ScenarioWriter();

    //! Return the next scenario for the given date.
    virtual QuantLib::ext::shared_ptr<Scenario> next(const Date& d) override;

    //! Write a single scenario
    void writeScenario(const QuantLib::ext::shared_ptr<Scenario>& s, const bool writeHeader);

    //! Reset the generator so calls to next() return the first scenario.
    virtual void reset() override;

    //! Close the file if it is open, not normally needed by client code
    void close();

private:
    void open(const std::string& filename, const std::string& filemode = "w+");

    QuantLib::ext::shared_ptr<ScenarioGenerator> src_;
    std::vector<RiskFactorKey> keys_;
    QuantLib::ext::shared_ptr<ore::data::Report> report_;
    FILE* fp_;
    Date firstDate_;
    Size i_;
    const char sep_ = ',';
    std::vector<RiskFactorKey> headerKeys_;
};
} // namespace analytics
} // namespace ore
