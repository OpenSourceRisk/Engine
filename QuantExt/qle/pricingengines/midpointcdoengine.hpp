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

#ifndef quantext_midpoint_cdo_engine_hpp
#define quantext_midpoint_cdo_engine_hpp

#include <ql/qldefines.hpp>

#ifndef QL_PATCH_SOLARIS

//#include <ql/experimental/credit/syntheticcdo.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/syntheticcdo.hpp>

namespace QuantExt {
using namespace QuantLib;

//! CDO base engine taking schedule steps

/* The engine obtains the cdo reference basket from its arguments and it
is expecting it to have a default model assigned.
*/
/* FIX ME: ASSUMES basket->expectedTrancheLoss(endDate) includes past
realized losses (between cdo inception and calculation time) .... what if
basket inception is not the same as CDO's ?????

\todo non tested under realized defaults. JTD metrics might be invalid
*/
class MidPointCDOEngine : public QuantExt::SyntheticCDO::engine {
public:
    MidPointCDOEngine(const Handle<YieldTermStructure>& discountCurve,
                      boost::optional<bool> includeSettlementDateFlows = boost::none)
        : discountCurve_(discountCurve), includeSettlementDateFlows_(includeSettlementDateFlows) {
        registerWith(discountCurve);
    }
    void calculate() const override;

protected:
    Handle<YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
};

} // namespace QuantExt

#endif

#endif
