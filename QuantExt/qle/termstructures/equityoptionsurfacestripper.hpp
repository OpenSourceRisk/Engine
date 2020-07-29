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

/*! \file qle/termstructures/equityoptionsurfacestripper.hpp
    \brief Imply equity vol surface from put/call price surfaces
    \ingroup termstructures
*/

#ifndef quantext_equity_option_premium_curve_stripper_hpp
#define quantext_equity_option_premium_curve_stripper_hpp

#include <ql/exercise.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

namespace QuantExt {

class EquityOptionSurfaceStripper : public QuantLib::LazyObject {

public:
    EquityOptionSurfaceStripper(const boost::shared_ptr<OptionInterpolatorBase>& callSurface,
                                const boost::shared_ptr<OptionInterpolatorBase>& putSurface,
                                const QuantLib::Handle<QuantExt::EquityIndex>& eqIndex, const Calendar& calendar,
                                const DayCounter& dayCounter,
                                QuantLib::Exercise::Type type = QuantLib::Exercise::European,
                                bool lowerStrikeConstExtrap = true, bool upperStrikeConstExtrap = true,
                                bool timeFlatExtrapolation = false);

    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

    boost::shared_ptr<QuantLib::BlackVolTermStructure> volSurface();

private:
    boost::shared_ptr<OptionInterpolatorBase> callSurface_;
    boost::shared_ptr<OptionInterpolatorBase> putSurface_;
    QuantLib::Handle<QuantExt::EquityIndex> eqIndex_;
    const Calendar& calendar_;
    const DayCounter& dayCounter_;
    QuantLib::Exercise::Type type_;
    bool lowerStrikeConstExtrap_;
    bool upperStrikeConstExtrap_;
    bool timeFlatExtrapolation_;

    QuantLib::Real implyVol(QuantLib::Date expiry, QuantLib::Real strike, QuantLib::Option::Type type,
                            boost::shared_ptr<QuantLib::PricingEngine> engine,
                            boost::shared_ptr<QuantLib::SimpleQuote> volQuote) const;

    mutable boost::shared_ptr<QuantLib::BlackVolTermStructure> volSurface_;
};

} // namespace QuantExt
#endif
#pragma once
