/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file forwardrateagreement.hpp
    \brief Forward Rate Agreement class
*/

#ifndef quantext_forwardrateagreement_hpp
#define quantext_forwardrateagreement_hpp

#include <ql/instruments/forwardrateagreement.hpp>

namespace QuantExt {

using QuantLib::Date;
using QuantLib::GenericEngine;
using QuantLib::Handle;
using QuantLib::IborIndex;
using QuantLib::InterestRate;
using QuantLib::Position;
using QuantLib::PricingEngine;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::YieldTermStructure;
namespace ext = QuantLib::ext;

//! Forward Rate Agreement class
class ForwardRateAgreement : public QuantLib::ForwardRateAgreement {
public:
    class arguments;
    class engine;
    class results;
    ForwardRateAgreement(
        const Date& valueDate,
        const Date& maturityDate,
        Position::Type type,
        Rate strikeForwardRate,
        Real notionalAmount,
        const ext::shared_ptr<IborIndex>& index,
        Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>(),
        bool useIndexedCoupon = true);
    ForwardRateAgreement(
        const Date& valueDate,
        Position::Type type,
        Rate strikeForwardRate,
        Real notionalAmount,
        const ext::shared_ptr<IborIndex>& index,
        Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>());

    //! \name Instrument interface
    //@{
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

protected:
    void performCalculations() const override;
};

class ForwardRateAgreement::arguments : public virtual PricingEngine::arguments {
public:
    Position::Type type;
    Real notionalAmount;
    ext::shared_ptr<IborIndex> index;
    Date valueDate;
    Date maturityDate;
    Handle<YieldTermStructure> discountCurve;
    InterestRate strikeForwardRate;
    void validate() const override {}
};

class ForwardRateAgreement::results : public Instrument::results {};

class ForwardRateAgreement::engine : public GenericEngine<ForwardRateAgreement::arguments,
                                                          ForwardRateAgreement::results> {};

} // namespace QuantExt

#endif
