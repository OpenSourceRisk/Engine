/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#ifndef quantext_equity_forward_curve_stripper_hpp
#define quantext_equity_forward_curve_stripper_hpp

#include <ql/exercise.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

namespace QuantExt {
using namespace QuantLib;

class EquityForwardCurveStripperBase : public LazyObject {


};

class EquityForwardCurveStripper : public EquityForwardCurveStripperBase {

public:
    EquityForwardCurveStripper(const boost::shared_ptr<BlackVarianceSurfaceSparse>& callSurface,
        const boost::shared_ptr<BlackVarianceSurfaceSparse>& putSurface,
        Handle<YieldTermStructure>& forecastCurve, Handle<QuantLib::Quote>& equitySpot,
        QuantLib::Exercise::Type = QuantLib::Exercise::Type::European);



    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

private:
    const boost::shared_ptr<BlackVarianceSurfaceSparse>& callSurface_;
    const boost::shared_ptr<BlackVarianceSurfaceSparse>& putSurface_;
    Handle<YieldTermStructure> forecastCurve_; 
    Handle<QuantLib::Quote> equitySpot_;


};

} // namespace QuantExt
# endif