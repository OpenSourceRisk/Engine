/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/strippedoptionletadapter2.hpp
    \brief StrippedOptionlet Adapter (with a deeper update method)
    \ingroup termstructures
*/

#ifndef quantext_stripped_optionlet_adapter2_h
#define quantext_stripped_optionlet_adapter2_h

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>
#include <ql/termstructures/volatility/optionlet/optionletstripper.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>

namespace QuantExt {

/*! Adapter class for turning a StrippedOptionletBase object into an
    OptionletVolatilityStructure.
    \ingroup termstructures
*/
class StrippedOptionletAdapter2 : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {
public:
    StrippedOptionletAdapter2(const boost::shared_ptr<QuantLib::StrippedOptionletBase>&);

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const;
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const;
    QuantLib::Rate maxStrike() const;
    //@}
    //! \name LazyObject interface
    //@{
    void update();
    void performCalculations() const;
    boost::shared_ptr<QuantLib::OptionletStripper> optionletStripper() const;
    //@}
    QuantLib::VolatilityType volatilityType() const;
    QuantLib::Real displacement() const;

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    boost::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const;

    QuantLib::Volatility volatilityImpl(QuantLib::Time length, QuantLib::Rate strike) const;
    //@}
private:
    const boost::shared_ptr<QuantLib::StrippedOptionletBase> optionletStripper_;
    QuantLib::Size nInterpolations_;
    mutable std::vector<boost::shared_ptr<QuantLib::Interpolation> > strikeInterpolations_;
};

inline void StrippedOptionletAdapter2::update() {
    optionletStripper_->update(); // just in case
    TermStructure::update();
    LazyObject::update();
}

inline boost::shared_ptr<QuantLib::OptionletStripper> StrippedOptionletAdapter2::optionletStripper() const {
    return boost::dynamic_pointer_cast<QuantLib::OptionletStripper>(optionletStripper_);
}
} // namespace QuantExt

#endif
