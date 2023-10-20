/*
 Copyright (C) 2016-2023 Quaternion Risk Management Ltd
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

/*! \file orea/app/oreapp.hpp
  \brief Open Risk Engine App
  \ingroup app
 */

#pragma once

#include <orea/app/inputparameters.hpp>
#include <orea/app/parameters.hpp>
#include <orea/app/analyticsmanager.hpp>

#include <ored/utilities/filteredbufferedlogger.hpp>

#include <boost/make_shared.hpp>
#include <boost/timer/timer.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;

//! Orchestrates the processes covered by ORE, data loading, analytics and reporting
/*! \ingroup app
 */
class OREApp {
public:
    //! Constructor that uses ORE parameters and input data from files
    OREApp(boost::shared_ptr<Parameters> params, bool console = false, 
           const boost::filesystem::path& = boost::filesystem::path());
    //! Constructor that assumes we have already assembled input parameters via API
    OREApp(const boost::shared_ptr<InputParameters>& inputs, const std::string& logFile, Size logLevel = 31,
           bool console = false, const boost::filesystem::path& = boost::filesystem::path());

    //! Destructor
    virtual ~OREApp();

    //! Runs analytics and generates reports after using the first OREApp c'tor
    virtual void run();

    //! Runs analytics and generates reports after using the second OREApp c'tor
    void run(const std::vector<std::string>& marketData,
             const std::vector<std::string>& fixingData);
    
    boost::shared_ptr<InputParameters> getInputs() { return inputs_; }

    std::set<std::string> getAnalyticTypes();
    std::set<std::string> getSupportedAnalyticTypes();
    const boost::shared_ptr<Analytic>& getAnalytic(std::string type); 
    
    std::set<std::string> getReportNames();
    boost::shared_ptr<PlainInMemoryReport> getReport(std::string reportName);

    std::set<std::string> getCubeNames();
    boost::shared_ptr<NPVCube> getCube(std::string cubeName);

    std::set<std::string> getMarketCubeNames();
    boost::shared_ptr<AggregationScenarioData> getMarketCube(std::string cubeName);

    std::vector<std::string> getErrors();

    //! time for executing run(...) in seconds
    Real getRunTime();

    std::string version();
    
protected:
    virtual void analytics();

    //! Populate InputParameters object from classic ORE key-value pairs in Parameters 
    void buildInputParameters(boost::shared_ptr<InputParameters> inputs,
                              const boost::shared_ptr<Parameters>& params);
    vector<string> getFileNames(const string& fileString, const string& path);
    boost::shared_ptr<CSVLoader> buildCsvLoader(const boost::shared_ptr<Parameters>& params);
    //! set up logging
    void setupLog(const std::string& path, const std::string& file, QuantLib::Size mask,
                  const boost::filesystem::path& logRootPath, const std::string& progressLogFile = "",
                  QuantLib::Size progressLogRotationSize = 100 * 1024 * 1024, bool progressLogToConsole = false,
                  const std::string& structuredLogFile = "", QuantLib::Size structuredLogRotationSize = 100 * 1024 * 1024);
    //! remove logs
    void closeLog();

    //! ORE Input parameters
    boost::shared_ptr<Parameters> params_;
    boost::shared_ptr<InputParameters> inputs_;
    boost::shared_ptr<OutputParameters> outputs_;

    boost::shared_ptr<AnalyticsManager> analyticsManager_;
    boost::shared_ptr<FilteredBufferedLoggerGuard> fbLogger_;
    boost::timer::cpu_timer runTimer_;
};

} // namespace analytics
} // namespace ore
