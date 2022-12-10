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

/*! \file qle/instruments/genericswaption.hpp
    \brief Swaption class
*/

#ifndef quantext_instruments_genericswaption_hpp
#define quantext_instruments_genericswaption_hpp

#include <ql/instruments/swap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Swaption class with QuantLib::Swap underlying
/*! \ingroup instruments
 */
class GenericSwaption : public Option {
public:
    class arguments;
    class results;
    class engine;
    GenericSwaption(const ext::shared_ptr<QuantLib::Swap>& swap, const ext::shared_ptr<Exercise>& exercise,
                    QuantLib::Settlement::Type delivery = QuantLib::Settlement::Physical,
                    QuantLib::Settlement::Method settlementMethod = QuantLib::Settlement::PhysicalOTC);
    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}
    //! \name Inspectors
    //@{
    Settlement::Type settlementType() const { return settlementType_; }
    Settlement::Method settlementMethod() const { return settlementMethod_; }
    const ext::shared_ptr<QuantLib::Swap>& underlyingSwap() const { return swap_; }
    Real underlyingValue() const { return underlyingValue_; }
    //@}
private:
    void fetchResults(const PricingEngine::results*) const override;
    // arguments
    ext::shared_ptr<QuantLib::Swap> swap_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
    mutable Real underlyingValue_;
};

//! %Arguments for swaption calculation
class GenericSwaption::arguments : public QuantLib::Swap::arguments, public Option::arguments {
public:
    arguments() : settlementType(Settlement::Physical) {}
    ext::shared_ptr<QuantLib::Swap> swap;
    QuantLib::Settlement::Type settlementType;
    QuantLib::Settlement::Method settlementMethod;
    void validate() const override;
};

//! %Results from CDS-option calculation
class GenericSwaption::results : public Option::results {
public:
    Real underlyingValue;
    void reset() override;
};

//! base class for swaption engines
class GenericSwaption::engine : public GenericEngine<GenericSwaption::arguments, GenericSwaption::results> {};

} // namespace QuantExt

#endif
