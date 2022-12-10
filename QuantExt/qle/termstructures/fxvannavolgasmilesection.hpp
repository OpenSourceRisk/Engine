/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file fxvannavolgasmilesection.hpp
    \brief FX smile section assuming a strike/volatility space using vanna volga method
    \ingroup termstructures
*/

#ifndef quantext_fx_vanna_volga_smile_section_hpp
#define quantext_fx_vanna_volga_smile_section_hpp

#include <ql/experimental/barrieroption/vannavolgainterpolation.hpp>
#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <qle/termstructures/fxsmilesection.hpp>

namespace QuantExt {
using namespace QuantLib;

/*! Vanna Volga Smile section
 *
 *  Consistent Pricing of FX Options
 *  Castagna & Mercurio (2006)
 *  http://papers.ssrn.com/sol3/papers.cfm?abstract_id=873788
 \ingroup termstructures
 */
class VannaVolgaSmileSection : public FxSmileSection {
public:
    VannaVolgaSmileSection(Real spot, Real rd, Real rf, Time t, Volatility atmVol, Volatility rr, Volatility bf,
                           bool firstApprox = false,
                           const DeltaVolQuote::AtmType& atmType = DeltaVolQuote::AtmType::AtmDeltaNeutral,
                           const DeltaVolQuote::DeltaType& deltaType = DeltaVolQuote::DeltaType::Spot,
                           const Real delta = 0.25);

    //! getters for unit test
    Real k_atm() const { return k_atm_; }
    Real k_c() const { return k_c_; }
    Real k_p() const { return k_p_; }
    Volatility vol_atm() const { return atmVol_; }
    Volatility vol_c() const { return vol_c_; }
    Volatility vol_p() const { return vol_p_; }

    //! \name FxSmileSection interface
    //@{
    Volatility volatility(Real strike) const override;
    //}@

private:
    Real d1(Real x) const;
    Real d2(Real x) const;

    Real k_atm_, k_c_, k_p_;
    Volatility atmVol_, rr_, bf_;
    Volatility vol_c_, vol_p_;
    bool firstApprox_;
};

} // namespace QuantExt

#endif
