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

/*
 Copyright (C) 2011 Chris Kenyon


 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
 */

/*!
    \file cpicapfloorengines.hpp
    \brief Extended version of the QuantLib engine, strike adjustment for seasoned CPI Cap/Floor pricing
    \ingroup PricingEngines
*/

#ifndef quantext_cpicapfloorengines_hpp
#define quantext_cpicapfloorengines_hpp

#include <ql/experimental/inflation/cpicapfloortermpricesurface.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

//! This engine only adds timing functionality (e.g. different lag)
//! w.r.t. an existing interpolated price surface.
class InterpolatingCPICapFloorEngine : public QuantLib::CPICapFloor::engine {
public:
    explicit InterpolatingCPICapFloorEngine(const QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface>&);

    virtual void calculate() const override;
    virtual std::string name() const { return "InterpolatingCPICapFloorEngine"; }

    virtual ~InterpolatingCPICapFloorEngine() {}

protected:
    QuantLib::Handle<QuantLib::CPICapFloorTermPriceSurface> priceSurf_;
};

} // namespace QuantExt

#endif // cpicapfloorengines_hpp
