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

/*! \file qle/termstructures/spreadedsmilesection.hpp
    \brief Adds floor to QuantLib::SmileSection
    \ingroup termstructures
*/

#ifndef quantext_spreaded_smile_section_hpp
#define quantext_spreaded_smile_section_hpp

#include <ql/termstructures/volatility/spreadedsmilesection.hpp>

using QuantLib::Handle;
using QuantLib::Quote;
using QuantLib::Rate;
using QuantLib::SmileSection;
using QuantLib::Volatility;

namespace QuantExt {

class SpreadedSmileSection : public QuantLib::SpreadedSmileSection {
public:
    SpreadedSmileSection(const boost::shared_ptr<SmileSection>& underlyingSection, const Handle<Quote>& spread);
    SpreadedSmileSection(const boost::shared_ptr<QuantLib::SpreadedSmileSection>& underlyingSection);

protected:
    Volatility volatilityImpl(Rate strike) const;
};
} // namespace QuantExt

#endif
