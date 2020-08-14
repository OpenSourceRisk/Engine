/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/aggregation/exposure.hpp
    \brief Exposure calculator
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/collatexposurehelper.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>

#include <boost/shared_ptr.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! XVA Calculator base class
/*!
  Derived classes implement a constructor with the relavant additional input data
  and a build function that performs the XVA calculations for all netting sets and
  along all paths.
*/
class ExposureCalculator {
public:
    ExposureCalculator(
        //! Driving portfolio consistent with the cube below
        const boost::shared_ptr<Portfolio>& portfolio,
        //! NPV cube resulting from the Monte Carlo simulation loop
        const boost::shared_ptr<NPVCube>& cube,
        const boost::shared_ptr<CubeInterpretation> cubeInterpretation,
        const boost::shared_ptr<Market>& market,
        const bool exerciseNextBreak,
        const string& baseCurrency,
        const string& configuration,
        const Real quantile,
        const CollateralExposureHelper::CalculationType calcType,
        const bool isRegularCubeStorage,
        const bool multiPath
    );

    virtual ~ExposureCalculator() {}

    //! Compute exposures along all paths and fill result structurues
    virtual void build();

    boost::shared_ptr<Portfolio> portfolio() { return portfolio_; }
    boost::shared_ptr<NPVCube> npvCube() { return cube_; }
    boost::shared_ptr<CubeInterpretation> cubeInterpretation() { return cubeInterpretation_; }
    boost::shared_ptr<Market> market() { return market_; }
    bool exerciseNextBreak() { return exerciseNextBreak_; }
    string baseCurrency() { return baseCurrency_; }
    string configuration() { return configuration_; }
    Real quantile() { return quantile_; }
    CollateralExposureHelper::CalculationType calcType() { return calcType_; }
    bool isRegularCubeStorage() { return isRegularCubeStorage_; }
    bool multiPath() { return multiPath_; }

    vector<Date> dates() { return dates_; }
    Date today() { return today_; }
    DayCounter dc() { return dc_; }
    vector<string> nettingSetIds() { return nettingSetIds_; }
    map<string, Real> nettingSetValueToday() { return nettingSetValueToday_; }
    map<string, Date> nettingSetMaturity() { return nettingSetMaturity_; }
    vector<Real> times() { return times_; };

    const boost::shared_ptr<NPVCube>& exposureCube() { return exposureCube_; }
    const map<string, vector<vector<Real>>>& nettingSetDefaultValue() { return nettingSetDefaultValue_; }
    const map<string, vector<vector<Real>>>& nettingSetCloseOutValue() { return nettingSetCloseOutValue_; }

    vector<Real> epe(const string& tid) { return getMeanExposure(tid, 0); }
    vector<Real> ene(const string& tid) { return getMeanExposure(tid, 1); }
    vector<Real> allocatedEpe(const string& tid) { return getMeanExposure(tid, 2); }
    vector<Real> allocatedEne(const string& tid) { return getMeanExposure(tid, 3); }
    vector<Real>& ee_b(const string& tid) { return ee_b_[tid]; }
    vector<Real>& eee_b(const string& tid) { return eee_b_[tid]; }
    vector<Real>& pfe(const string& tid) { return pfe_[tid]; }
    Real& epe_b(const string& tid) { return epe_b_[tid]; }
    Real& eepe_b(const string& tid) { return eepe_b_[tid]; }

protected:
    boost::shared_ptr<Portfolio> portfolio_;
    boost::shared_ptr<NPVCube> cube_;
    boost::shared_ptr<CubeInterpretation> cubeInterpretation_;
    boost::shared_ptr<Market> market_;
    bool exerciseNextBreak_;
    string baseCurrency_;
    string configuration_;
    Real quantile_;
    CollateralExposureHelper::CalculationType calcType_;
    bool isRegularCubeStorage_;
    bool multiPath_;

    vector<Date> dates_;
    const Date today_;
    const DayCounter dc_;
    vector<string> nettingSetIds_;
    map<string, Real> nettingSetValueToday_;
    map<string, Date> nettingSetMaturity_;
    vector<Real> times_;

    boost::shared_ptr<NPVCube> exposureCube_;
    map<string, vector<vector<Real>>> nettingSetDefaultValue_, nettingSetCloseOutValue_;

    map<string, std::vector<Real>> ee_b_;
    map<string, std::vector<Real>> eee_b_;
    map<string, std::vector<Real>> pfe_;
    map<string, Real> epe_b_;
    map<string, Real> eepe_b_;
    vector<Real> getMeanExposure(const string& tid, Size index);
};

} // namespace analytics
} // namespace ore
