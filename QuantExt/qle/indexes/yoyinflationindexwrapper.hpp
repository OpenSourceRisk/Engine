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
 Copyright (C) 2007 Chris Kenyon

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

/*! \file yoyinflationindexwrapper.hpp
    \brief wrapper class to create yoy from zc indices
*/

#ifndef quantext_yoy_inflation_index_wrapper_hpp
#define quantext_yoy_inflation_index_wrapper_hpp

#include <ql/indexes/inflationindex.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Wrapper that creates a yoy from a zc index
/*! This creates a "ratio" - type YoYInflationIndex with the same family name as the zero
  index so that historical fixings can be reused. If a yoy ts is given, this is used to
  forecast fixings. If the ts is not given, the forecast falls back on the zero index, i.e.
  if the zero index has a curve attached a plain yoy rate without convexity adjustment
  is estimated using this index. */
class YoYInflationIndexWrapper : public YoYInflationIndex {
public:
    YoYInflationIndexWrapper(const boost::shared_ptr<ZeroInflationIndex> zeroIndex,
                             const Handle<YoYInflationTermStructure>& ts = Handle<YoYInflationTermStructure>());
    Real forecastFixing(const Date& fixingDate) const;
private:
    const boost::shared_ptr<ZeroInflationIndex> zeroIndex_;
};

} // namesapce QuantExt

#endif
