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

/*! \file qle/termstructures/optionletstripper1.hpp
    \brief Optionlet (caplet/floorlet) volatility strippers
    \ingroup termstructures
*/

#ifndef quantext_optionletstripper1_hpp
#define quantext_optionletstripper1_hpp

#include <ql/instruments/capfloor.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/termstructures/optionletstripper.hpp>

#include <boost/optional.hpp>

namespace QuantExt {
using namespace QuantLib;
using boost::optional;

typedef std::vector<std::vector<QuantLib::ext::shared_ptr<QuantLib::CapFloor> > > CapFloorMatrix;

/*! Helper class to strip optionlet (i.e. caplet/floorlet) volatilities
    (a.k.a. forward-forward volatilities) from the (cap/floor) term
    volatilities of a CapFloorTermVolSurface.
    \ingroup termstructures
*/
class OptionletStripper1 : public QuantExt::OptionletStripper {
public:
    // If dontThrow is set to true than any vols that would throw are set to dontThrowMinVol (default is 0.0)
    OptionletStripper1(const QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface>&,
                       const QuantLib::ext::shared_ptr<IborIndex>& index, Rate switchStrikes = Null<Rate>(),
                       Real accuracy = 1.0e-6, Natural maxIter = 100,
                       const Handle<YieldTermStructure>& discount = Handle<YieldTermStructure>(),
                       const VolatilityType type = ShiftedLognormal, const Real displacement = 0.0,
                       const optional<VolatilityType> targetVolatilityType = boost::none,
                       const optional<Real> targetDisplacement = boost::none);

    const Matrix& capFloorPrices() const;
    const Matrix& capletVols() const;
    const Matrix& capFloorVolatilities() const;
    const Matrix& optionletPrices() const;
    Rate switchStrike() const;
    const Handle<YieldTermStructure>& discountCurve() const { return discount_; }

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
private:
    bool stripOptionlets(std::vector<Real>&, CapFloor::Type, Size, const Handle<YieldTermStructure>&, Real) const;

    mutable Matrix capFloorPrices_, optionletPrices_;
    mutable Matrix capFloorVols_;
    mutable Matrix optionletStDevs_, capletVols_;

    mutable CapFloorMatrix capFloors_;
    mutable std::vector<std::vector<QuantLib::ext::shared_ptr<SimpleQuote> > > volQuotes_;
    mutable std::vector<std::vector<QuantLib::ext::shared_ptr<PricingEngine> > > capFloorEngines_;
    bool floatingSwitchStrike_;
    mutable bool capFlooMatrixNotInitialized_;
    mutable Rate switchStrike_;
    Real accuracy_;
    Natural maxIter_;
    const VolatilityType inputVolatilityType_;
    const Real inputDisplacement_;
};
} // namespace QuantExt

#endif
