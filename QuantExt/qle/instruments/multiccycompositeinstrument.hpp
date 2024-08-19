/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/multiccycompositeinstrument.hpp
\brief bond option class
\ingroup instruments
*/

#pragma once

#include <list>
#include <ql/instrument.hpp>
#include <ql/quotes/simplequote.hpp>
#include <tuple>
#include <utility>

namespace QuantExt {
using namespace QuantLib;

//! %Composite instrument
/*! This instrument is an aggregate of other instruments. Its NPV
    is the sum of the NPVs of its components, each possibly
    multiplied by a given factor, and an FX rate.

    \warning Methods that drive the calculation directly (such as
             recalculate(), freeze() and others) might not work
             correctly.

    \ingroup instruments
*/
class MultiCcyCompositeInstrument : public Instrument {
    typedef std::tuple<ext::shared_ptr<Instrument>, Real, Handle<Quote>> component;
    typedef std::list<component>::iterator iterator;
    typedef std::list<component>::const_iterator const_iterator;

public:
    //! adds an instrument to the composite
    void add(const ext::shared_ptr<Instrument>& instrument, Real multiplier = 1.0,
             const Handle<Quote>& fx = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)));
    //! shorts an instrument from the composite
    void subtract(const ext::shared_ptr<Instrument>& instrument, Real multiplier = 1.0,
                  const Handle<Quote>& fx = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0)));
    //! \name Observer interface
    //@{
    void deepUpdate() override;
    //@}
    //! \name Instrument interface
    //@{
    bool isExpired() const override;

protected:
    void performCalculations() const override;
    //@}
private:
    std::list<component> components_;
};

} // namespace QuantExt
