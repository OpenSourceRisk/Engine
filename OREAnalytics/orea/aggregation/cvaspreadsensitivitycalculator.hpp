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

/*! \file orea/aggregation/dimcalculator.hpp
    \brief Dynamic Initial Margin calculator base class
    \ingroup analytics
*/

#pragma once

#include <orea/aggregation/collatexposurehelper.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>

#include <ql/time/date.hpp>

#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;
using namespace data;
using namespace std;

//! CVA Spread Sensitivity Calculator
/*!
  Compute hazard rate and CDS spread sensitivities for a given exposure profile 
  on an externally provided sensitivity grid.
*/
class CVASpreadSensitivityCalculator {
public:
    CVASpreadSensitivityCalculator(//! For logging purposes to distinguish sensi runs, e.g. for different netting sets
				   const std::string& key,
				   //! Asof date
				   const Date& asof,
				   //! Netting set EPE profile
				   const vector<Real>& epe,
				   //! Date grid
				   const vector<Date>& dates,
				   //! Default term structure for the netting set
				   const Handle<DefaultProbabilityTermStructure>& dts,
				   //! Market recovery rate
				   const Real& recovery,
				   //! CDS Discount curve 
				   const Handle<YieldTermStructure>& yts,
				   //! Shift grid
				   const vector<Period>& shiftTenors,
				   //! Shift size
				   Real shiftSize = 0.0001);  
  
    //! Inspectors
    // @{
    const string key() { return key_; }
    Date asof() { return asof_; }
    const vector<Real>& exposureProfile() { return epe_; }
    const vector<Date>& exposureDateGrid() { return dates_; }
    const Handle<DefaultProbabilityTermStructure>& defaultTermStructure() { return dts_; }
    Real recoveryRate() { return recovery_; }
    const Handle<YieldTermStructure>& discountCurve() { return yts_; }
    const vector<Period> shiftTenors() { return shiftTenors_; }
    // @}
  
    //! Results
    // @{
    const vector<Real> shiftTimes() { return shiftTimes_; }
    Real shiftSize() { return shiftSize_; }
    const vector<Real> hazardRateSensitivities() { return hazardRateSensitivities_; }
    const vector<Real> cdsSpreadSensitivities() { return cdsSpreadSensitivities_; }
    const Matrix& jacobi() { return jacobi_; }
    // @}

private:
    // survival probability with shifted hazard rates in the specified bucket 
    Real survivalProbability(Date d, bool shift, Size index);
    Real survivalProbability(Time t, bool shift, Size index);
    // CVA calculation with and without shifted hazard rates
    Real cva(bool shift = false, Size index = 0);
    //! Fair CDS Spread calculation with and without shifted hazard rates
    Real fairCdsSpread(Size term, bool shift = false, Size index = 0);
		     
    string key_;
    Date asof_;
    vector<Real> epe_;
    vector<Date> dates_;
    Handle<DefaultProbabilityTermStructure> dts_;
    Real recovery_;
    Handle<YieldTermStructure> yts_;
    vector<Period> shiftTenors_;

    vector<Real> shiftTimes_;
    Real shiftSize_;
    vector<Real> hazardRateSensitivities_;
    vector<Real> cdsSpreadSensitivities_;  
    Matrix jacobi_;
};

} // namespace analytics
} // namespace ore
