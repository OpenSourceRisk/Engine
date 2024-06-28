/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/cashpositionengine.hpp
    \brief Cash position discounting engine
    \ingroup engines
*/

#ifndef quantext_cash_position_engine_hpp
#define quantext_cash_position_engine_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

#include <qle/instruments/cashposition.hpp>

namespace QuantExt {

//! Cash position engine

/*! This class sets the NPV to the be cash position amount

  \ingroup engines
*/
class CashPositionEngine : public CashPosition::engine {
public:
    //! \name Constructors
    //@{
    CashPositionEngine() {}
    //@}

    //! \name PricingEngine interface
    //@{
    void calculate() const override { 
        results_.value = arguments_.amount;
    }
    //@}
};
} // namespace QuantExt

#endif
