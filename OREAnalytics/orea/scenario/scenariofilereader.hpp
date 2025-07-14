/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/scenario/scenariofilereader.hpp
    \brief Class for reading historical scenarios from file
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenarioreader.hpp>

#include <orea/scenario/scenariofactory.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <string>
#include <vector>

namespace ore {
namespace analytics {

//! Class for reading  scenarios from a csv file
class ScenarioCSVReader : public ScenarioReader {
public:
    /*! Constructor where \p filename gives the path to the file from which to
        read the scenarios and \p scenarioFactory is a factory for building Scenarios
    */
    ScenarioCSVReader(const QuantLib::ext::shared_ptr<ore::data::CSVReader>& reader,
                                 const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory);
    //! Destructor
    ~ScenarioCSVReader() override{};
    //! Return true if there is another Scenario to read and move to it
    bool next() override;
    //! Return the current scenario's date if reader is still valid and `Null<Date>()` otherwise
    QuantLib::Date date() const override;
    //! Return the current scenario if reader is still valid and `nullptr` otherwise
    QuantLib::ext::shared_ptr<Scenario> scenario() const override;

protected:
    //! Scenario factory
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory_;
    //! Handle on the csv file
    QuantLib::ext::shared_ptr<ore::data::CSVReader> reader_;
    //! The risk factor keys of the scenarios in the file
    std::vector<RiskFactorKey> keys_;
    //! Flag indicating if the reader has no more scenarios to read
    bool finished_;
};

class ScenarioFileReader : public ScenarioCSVReader {
public:
    ScenarioFileReader(const std::string& file, const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory) :
        ScenarioCSVReader(QuantLib::ext::make_shared<ore::data::CSVFileReader>(file, true), scenarioFactory){}

    ~ScenarioFileReader() override;
};

class ScenarioBufferReader : public ScenarioCSVReader {
public:
    ScenarioBufferReader(const std::string& buffer, const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory)
        : ScenarioCSVReader(QuantLib::ext::make_shared<ore::data::CSVBufferReader>(buffer, true), scenarioFactory){}
};

} // namespace analytics
} // namespace ore
