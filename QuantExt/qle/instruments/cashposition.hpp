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

/*! \file qle/instruments/cashposition.hpp
    \brief cash position instrument
    \ingroup instruments
*/

#ifndef quantext_cashposition_hpp
#define quantext_cashposition_hpp

#include <ql/instrument.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Cash position Instrument

/*! This class holds the data a cash position.

    \ingroup instruments
*/
class CashPosition : public Instrument {
public:
    class arguments : public virtual PricingEngine::arguments {
    public:
        QuantLib::Real amount;
        void validate() const override { QL_REQUIRE(amount != QuantLib::Null<QuantLib::Real>(), "null Amount"); }
    };

    class results : public Instrument::results {
    public:
        void reset() override{};
    };

    class engine : public GenericEngine<CashPosition::arguments, CashPosition::results> {};

    explicit CashPosition(const Real amount) : amount_(amount){};

    //! \name Instrument interface
    //@{
    bool isExpired() const override { return false; }
    void setupArguments(PricingEngine::arguments* args) const override {
        CashPosition::arguments* arguments = dynamic_cast<CashPosition::arguments*>(args);
        
        QL_REQUIRE(arguments, "wrong argument type in CashPosition instrument");
        arguments->amount = amount_;
    }
    //@}
    //! \name Additional interface
    //@{
    const QuantLib::Real amount() const { return amount_; }
    //@}

private:
    QuantLib::Real amount_ = QuantLib::Null<QuantLib::Real>();
};

} // namespace QuantExt

#endif
