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
    OREApp(QuantLib::ext::shared_ptr<Parameters> params, bool console = false, 
           const boost::filesystem::path& logRootPath = boost::filesystem::path())
      : params_(params), inputs_(nullptr), console_(console), logRootPath_(logRootPath) {}

    //! Constructor that assumes we have already assembled input parameters via API
    OREApp(const QuantLib::ext::shared_ptr<InputParameters>& inputs, const std::string& logFile, Size logLevel = 31,
           bool console = false, const boost::filesystem::path& logRootPath = boost::filesystem::path())
      : params_(nullptr), inputs_(inputs), logFile_(logFile), logMask_(logLevel), console_(console),
	logRootPath_(logRootPath) {}

    //! Destructor
    virtual ~OREApp();

    //! Runs analytics and generates reports after using the first OREApp c'tor
    virtual void run();

    //! Runs analytics and generates reports after using the second OREApp c'tor
    void run(const std::vector<std::string>& marketData,
             const std::vector<std::string>& fixingData);
    
    QuantLib::ext::shared_ptr<InputParameters> getInputs() { return inputs_; }

    std::set<std::string> getAnalyticTypes();
    std::set<std::string> getSupportedAnalyticTypes();
    const QuantLib::ext::shared_ptr<Analytic>& getAnalytic(std::string type); 
    
    std::set<std::string> getReportNames();
    QuantLib::ext::shared_ptr<PlainInMemoryReport> getReport(std::string reportName);

    std::set<std::string> getCubeNames();
    QuantLib::ext::shared_ptr<NPVCube> getCube(std::string cubeName);

    std::set<std::string> getMarketCubeNames();
    QuantLib::ext::shared_ptr<AggregationScenarioData> getMarketCube(std::string cubeName);

    std::vector<std::string> getErrors();

    //! time for executing run(...) in seconds
    Real getRunTime();

    std::string version();
    
protected:
    virtual void analytics();

    //! Populate InputParameters object from classic ORE key-value pairs in Parameters 
    void buildInputParameters(QuantLib::ext::shared_ptr<InputParameters> inputs,
                              const QuantLib::ext::shared_ptr<Parameters>& params);
    QuantLib::ext::shared_ptr<CSVLoader> buildCsvLoader(const QuantLib::ext::shared_ptr<Parameters>& params);
    //! set up logging
    void setupLog(const std::string& path, const std::string& file, QuantLib::Size mask,
                  const boost::filesystem::path& logRootPath, const std::string& progressLogFile = "",
                  QuantLib::Size progressLogRotationSize = 100 * 1024 * 1024, bool progressLogToConsole = false,
                  const std::string& structuredLogFile = "", QuantLib::Size structuredLogRotationSize = 100 * 1024 * 1024);
    //! remove logs
    void closeLog();

    void initFromParams();
    void initFromInputs();
      
    //! ORE Input parameters
    QuantLib::ext::shared_ptr<Parameters> params_;
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    QuantLib::ext::shared_ptr<OutputParameters> outputs_;

    QuantLib::ext::shared_ptr<AnalyticsManager> analyticsManager_;
    QuantLib::ext::shared_ptr<StructuredLogger> structuredLogger_;
    boost::timer::cpu_timer runTimer_;

    //! Logging
    string logFile_;
    Size logMask_;
    bool console_;
    string outputPath_;
    boost::filesystem::path logRootPath_;
    string progressLogFile_ = "";
    QuantLib::Size progressLogRotationSize_ = 100 * 1024 * 1024;
    bool progressLogToConsole_ = false;
    string structuredLogFile_ = "";
    QuantLib::Size structuredLogRotationSize_ = 100 * 1024 * 1024;

    // Cached error messages of a run
    std::vector<std::string> errorMessages_;
};

class OREAppInputParameters : virtual public InputParameters {
public:
    OREAppInputParameters(const QuantLib::ext::shared_ptr<Parameters>& params) : params_(params) {}

     // load input parameters
    virtual void loadParameters() override;

    //! write out parameters
    virtual void writeOutParameters() override{};

private:
    QuantLib::ext::shared_ptr<Parameters> params_;
};

} // namespace analytics
} // namespace ore
