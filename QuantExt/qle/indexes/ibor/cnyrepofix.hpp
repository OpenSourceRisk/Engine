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

/*! \file qle/indexes/ibor/cnyrepofix.hpp
    \brief CNY-CNREPOFIX=CFXS-Reuters index
    \ingroup indexes
*/

#ifndef quantext_cny_repo_fix_hpp
#define quantext_cny_repo_fix_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/china.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {

//! CNY-CNREPOFIX=CFXS-Reuters index
/*! CNY repo fixing rate published by the China Foreign Exchange Trade System (CFETS). See 
    http://www.chinamoney.com.cn/english/bmkfrr. The 7 day maturity rate is the ISDA <em>Floating Rate Option</em> 
    defined in section 7.1(ah) of supplement number 21 to the 2006 ISDA definitions.

    \remark We have used the China inter-bank market calendar for the fixing calendar here. The ISDA definitions 
            refer to <em>Beijing Banking Day</em> as the business days. They may be one and the same.

*/
class CNYRepoFix : public QuantLib::IborIndex {
public:
    CNYRepoFix(const QuantLib::Period& tenor, const QuantLib::Handle<QuantLib::YieldTermStructure>& h =
        QuantLib::Handle<QuantLib::YieldTermStructure>())
        : IborIndex("CNY-REPOFIX", tenor, 1, QuantLib::CNYCurrency(), QuantLib::China(QuantLib::China::IB),
            QuantLib::Following, false, QuantLib::Actual365Fixed(), h) {}
};

}

#endif
