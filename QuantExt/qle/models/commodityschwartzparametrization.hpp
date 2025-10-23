/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file commodityschwartzparametrization.hpp
    \brief Schwartz commodity model parametrization
    \ingroup models
*/

#ifndef quantext_com_schwartz_parametrization_hpp
#define quantext_com_schwartz_parametrization_hpp

#include <ql/handle.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/models/parametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {
//! COM Schwartz parametrization
/*! Base class for COM parametrization for the Schwartz (1997) mean-reverting one-factor model
    \ingroup models
*/
class CommoditySchwartzParametrization : public Parametrization{
public:
    /*! The currency refers to the commodity currency, the
        fx spot is as of today (i.e. the discounted spot) */
    CommoditySchwartzParametrization(const Currency& currency, const std::string& name,
                                     const Handle<QuantExt::PriceTermStructure>& priceCurve,
                                     const Handle<Quote>& fxSpotToday,
                                     bool driftFreeState = false);

    Size numberOfParameters() const override { return 3; }
    //! State variable variance on [0, t]
    virtual Real variance(const Time t) const { return 0.0; }
    //! State variable Y's diffusion at time t: sigma * exp(kappa * t)
    virtual Real sigma(const Time t) const { return 0.0; }
    //! Inspector for the current value of model parameter sigma (direct)
    virtual Real sigmaParameter() const { return 0.0; }
    //! Inspector for the current value of model parameter kappa (direct)
    virtual Real kappaParameter() const { return 0.0; }
    //! Inspector for the current value of model parameter a (direct)
    virtual Real a(const QuantLib::Time t) const { return 0.0; }
    //! Inspector for the current value of model parameter m:= exp(a) (direct)
    virtual Real m(const QuantLib::Time t) const { return 0.0; }
    //! Variance V(t,T) used in the computation of F(t,T)
    virtual Real VtT(Real t, Real T) { return 0.0; }    

    Handle<QuantExt::PriceTermStructure> priceCurve() const { return priceCurve_; }
    const Handle<Quote> fxSpotToday() const { return fxSpotToday_; }
    bool driftFreeState() const { return driftFreeState_; }
    
protected:
    const Handle<QuantExt::PriceTermStructure> priceCurve_;
    const Handle<Quote> fxSpotToday_;
    std::string comName_;
    bool driftFreeState_;

};

} // namespace QuantExt

#endif
