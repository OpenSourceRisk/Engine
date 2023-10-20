/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/nullinstrument.hpp
    \brief A null instrument that always returns an NPV of 0
*/

#ifndef quantext_null_instrument_hpp
#define quantext_null_instrument_hpp

#include <ql/currency.hpp>
#include <ql/instrument.hpp>

namespace QuantExt {

/*! A null instrument that always returns an NPV of 0
 */
class NullInstrument : public QuantLib::Instrument {
public:
    NullInstrument() {}

    //! \name Instrument interface
    //@{
    //! Always returns true.
    bool isExpired() const override;
    //! Populate results
    void performCalculations() const override;
    //@}
};

inline bool NullInstrument::isExpired() const {
    return true;
}

inline void NullInstrument::performCalculations() const {
    NPV_ = 0.0;
    errorEstimate_ = 0.0;
}

}

#endif
