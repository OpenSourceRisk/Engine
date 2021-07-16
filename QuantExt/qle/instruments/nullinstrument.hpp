/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
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
