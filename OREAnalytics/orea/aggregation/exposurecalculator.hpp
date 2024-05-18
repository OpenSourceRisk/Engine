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

/*! \file orea/aggregation/exposurecalculator.hpp
    \brief Exposure calculator
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/collatexposurehelper.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>

#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! XVA Calculator base class
/*!
  Derived classes implement a constructor with the relevant additional input data
  and a build function that performs the XVA calculations for all netting sets and
  along all paths.
*/
class ExposureCalculator {
public:
    ExposureCalculator(
        //! Driving portfolio consistent with the cube below
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        //! NPV cube resulting from the Monte Carlo simulation loop
        const QuantLib::ext::shared_ptr<NPVCube>& cube,
        //! Interpreter for cube storage (where to find which data items)
        const QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation,
        //! Market data object to access e.g. discounting and funding curves
        const QuantLib::ext::shared_ptr<Market>& market,
	    //! Flag to indicate exposure termination at the next break date
        const bool exerciseNextBreak,
        //! Expression currency for all results
        const string& baseCurrency,
        //! Market configuration to use
        const string& configuration,
        //! Quantile for Potential Future Exposure output
        const Real quantile,
        //! Collateral calculation type to be used, see class %CollateralExposureHelper
        const CollateralExposureHelper::CalculationType calcType,
	    //! Flag to indicate exposure evaluation with dynamic credit
        const bool multiPath,
        //! Flag to indicate flipped xva calculation
        const bool flipViewXVA
    );

    virtual ~ExposureCalculator() {}

    //! Compute exposures along all paths and fill result structures
    virtual void build();

    enum ExposureIndex {
        EPE = 0,
        ENE,
        allocatedEPE,
        allocatedENE
    };
    const Size EXPOSURE_CUBE_DEPTH = 4;

    QuantLib::ext::shared_ptr<Portfolio> portfolio() { return portfolio_; }
    QuantLib::ext::shared_ptr<NPVCube> npvCube() { return cube_; }
    QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation() { return cubeInterpretation_; }
    QuantLib::ext::shared_ptr<Market> market() { return market_; }
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

    const QuantLib::ext::shared_ptr<NPVCube>& exposureCube() { return exposureCube_; }
    const map<string, vector<vector<Real>>>& nettingSetDefaultValue() { return nettingSetDefaultValue_; }
    const map<string, vector<vector<Real>>>& nettingSetCloseOutValue() { return nettingSetCloseOutValue_; }
    const map<string, vector<vector<Real>>>& nettingSetMporPositiveFlow() { return nettingSetMporPositiveFlow_; }
    const map<string, vector<vector<Real>>>& nettingSetMporNegativeFlow() { return nettingSetMporNegativeFlow_; }

    vector<Real> epe(const string& tid) { return getMeanExposure(tid, ExposureIndex::EPE); }
    vector<Real> ene(const string& tid) { return getMeanExposure(tid, ExposureIndex::ENE); }
    vector<Real> allocatedEpe(const string& tid) { return getMeanExposure(tid, ExposureIndex::allocatedEPE); }
    vector<Real> allocatedEne(const string& tid) { return getMeanExposure(tid, ExposureIndex::allocatedENE); }
    vector<Real>& ee_b(const string& tid) { return ee_b_[tid]; }
    vector<Real>& eee_b(const string& tid) { return eee_b_[tid]; }
    vector<Real>& pfe(const string& tid) { return pfe_[tid]; }
    Real& epe_b(const string& tid) { return epe_b_[tid]; }
    Real& eepe_b(const string& tid) { return eepe_b_[tid]; }

protected:
    const QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    const QuantLib::ext::shared_ptr<NPVCube> cube_;
    const QuantLib::ext::shared_ptr<CubeInterpretation> cubeInterpretation_;
    const QuantLib::ext::shared_ptr<Market> market_;
    const bool exerciseNextBreak_;
    const string baseCurrency_;
    const string configuration_;
    const Real quantile_;
    const CollateralExposureHelper::CalculationType calcType_;
    const bool multiPath_;
    bool isRegularCubeStorage_;

    vector<Date> dates_;
    const Date today_;
    const DayCounter dc_;
    vector<string> nettingSetIds_;
    map<string, Real> nettingSetValueToday_;
    map<string, Date> nettingSetMaturity_;
    vector<Real> times_;

    QuantLib::ext::shared_ptr<NPVCube> exposureCube_;
    map<string, vector<vector<Real>>> nettingSetDefaultValue_, nettingSetCloseOutValue_;
    map<string, vector<vector<Real>>> nettingSetMporPositiveFlow_, nettingSetMporNegativeFlow_;

    map<string, std::vector<Real>> ee_b_;
    map<string, std::vector<Real>> eee_b_;
    map<string, std::vector<Real>> pfe_;
    map<string, Real> epe_b_;
    map<string, Real> eepe_b_;
    vector<Real> getMeanExposure(const string& tid, ExposureIndex index);
    bool flipViewXVA_;
};

} // namespace analytics
} // namespace ore
