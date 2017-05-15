/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2009, 2014 Jose Aparicio

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

#ifndef quantext_midpoint_cdo_engine_hpp
#define quantext_midpoint_cdo_engine_hpp

#include <ql/qldefines.hpp>

#ifndef QL_PATCH_SOLARIS

#include <ql/experimental/credit/syntheticcdo.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! CDO base engine taking schedule steps

    /* The engine obtains the cdo reference basket from its arguments and it 
    is expecting it to have a default model assigned. 
    */
    /* FIX ME: ASSUMES basket->expectedTrancheLoss(endDate) includes past 
    realized losses (between cdo inception and calculation time) .... what if 
    basket inception is not the same as CDO's ?????

    \todo non tested under realized defaults. JTD metrics might be invalid
    */
    class MidPointCDOEngine : public QuantLib::SyntheticCDO::engine {
    public:
        MidPointCDOEngine(
            const Handle<YieldTermStructure>& discountCurve)
        : discountCurve_(discountCurve) {}
        void calculate() const;
    protected:
        Handle<YieldTermStructure> discountCurve_;
    };

}

#endif

#endif
