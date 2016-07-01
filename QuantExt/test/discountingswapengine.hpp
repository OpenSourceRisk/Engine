/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/


/*! \file test/discountingswapengine.hpp
    \brief discounting swap engine with simulated fixings test
    \ingroup 
*/

#ifndef quantext_test_discountingswapengine_hpp
#define quantext_test_discountingswapengine_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {
    
//! DiscountingSwapEngine tests
/*!
  \ingroup tests
*/
class DiscountingSwapEngineTest {
  public:
    static void testVanillaSwap();
    static void testFixingEstimationMethods();
    static boost::unit_test_framework::test_suite* suite();
};

}

#endif
