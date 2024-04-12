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

/*! \file qle/termstructures/equityforwardcurvestripper.hpp
    \brief Imply equity forwards from option put/call parity
    \ingroup termstructures
*/

#ifndef quantext_equity_forward_curve_stripper_hpp
#define quantext_equity_forward_curve_stripper_hpp

#include <ql/exercise.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

namespace QuantExt {

class EquityForwardCurveStripper : public QuantLib::LazyObject {

public:
    EquityForwardCurveStripper(const QuantLib::ext::shared_ptr<OptionPriceSurface>& callSurface,
                               const QuantLib::ext::shared_ptr<OptionPriceSurface>& putSurface,
                               QuantLib::Handle<QuantLib::YieldTermStructure>& forecastCurve,
                               QuantLib::Handle<QuantLib::Quote>& equitySpot,
                               QuantLib::Exercise::Type type = QuantLib::Exercise::European);

    //! return the expiries
    const std::vector<QuantLib::Date> expiries() const;
    //! return the stripped forwards
    const std::vector<QuantLib::Real> forwards() const;

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

private:
    const QuantLib::ext::shared_ptr<OptionPriceSurface> callSurface_;
    const QuantLib::ext::shared_ptr<OptionPriceSurface> putSurface_;
    QuantLib::Handle<QuantLib::YieldTermStructure> forecastCurve_;
    QuantLib::Handle<QuantLib::Quote> equitySpot_;
    QuantLib::Exercise::Type type_;

    //! store the stripped forward rates
    mutable std::vector<QuantLib::Real> forwards_;

    QuantLib::Real forwardFromPutCallParity(QuantLib::Date d, QuantLib::Real call,
                                            const OptionPriceSurface& callSurface,
                                            const OptionPriceSurface& putSurface) const;
};

} // namespace QuantExt
#endif
