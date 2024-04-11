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

/*! \file qle/termstructures/optionletstripper2.hpp
    \brief ATM optionlet (caplet/floorlet) volatility stripper
    \ingroup termstructures
*/

#ifndef quantext_optionletstripper2_hpp
#define quantext_optionletstripper2_hpp

#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>
#include <qle/termstructures/optionletstripper1.hpp>

namespace QuantExt {
using namespace QuantLib;
using std::vector;

/*! Helper class to extend a QuantExt::OptionletStripper object stripping
    additional optionlet (i.e. caplet/floorlet) volatilities (a.k.a.
    forward-forward volatilities) from the (cap/floor) At-The-Money
    term volatilities of a CapFloorTermVolCurve.

    \ingroup termstructures
*/
class OptionletStripper2 : public QuantExt::OptionletStripper {
public:
    //! Optionlet stripper that modifies the stripped optionlets from \p optionletStripper by adding optionlet
    //! volatilities stripped from an ATM volatility curve \p atmCapFloorTermVolCurve
    OptionletStripper2(const QuantLib::ext::shared_ptr<QuantExt::OptionletStripper>& optionletStripper,
                       const Handle<QuantLib::CapFloorTermVolCurve>& atmCapFloorTermVolCurve,
                       const Handle<YieldTermStructure>& discount = Handle<YieldTermStructure>(),
                       const VolatilityType type = ShiftedLognormal, const Real displacement = 0.0);

    vector<Rate> atmCapFloorStrikes() const;
    vector<Real> atmCapFloorPrices() const;

    vector<Volatility> spreadsVol() const;

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}
private:
    vector<Volatility> spreadsVolImplied(const Handle<YieldTermStructure>& discount) const;

    class ObjectiveFunction {
    public:
        ObjectiveFunction(const QuantLib::ext::shared_ptr<QuantExt::OptionletStripper>&,
                          const QuantLib::ext::shared_ptr<QuantLib::CapFloor>&, Real targetValue,
                          const Handle<YieldTermStructure>& discount);
        Real operator()(Volatility spreadVol) const;

    private:
        QuantLib::ext::shared_ptr<SimpleQuote> spreadQuote_;
        QuantLib::ext::shared_ptr<QuantLib::CapFloor> cap_;
        Real targetValue_;
        const Handle<YieldTermStructure> discount_;
    };

    const QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper_;
    const Handle<QuantLib::CapFloorTermVolCurve> atmCapFloorTermVolCurve_;
    DayCounter dc_;
    Size nOptionExpiries_;
    mutable vector<Rate> atmCapFloorStrikes_;
    mutable vector<Real> atmCapFloorPrices_;
    mutable vector<Volatility> spreadsVolImplied_;
    mutable vector<QuantLib::ext::shared_ptr<QuantLib::CapFloor> > caps_;
    Size maxEvaluations_;
    Real accuracy_;
    const VolatilityType inputVolatilityType_;
    const Real inputDisplacement_;
};
} // namespace QuantExt

#endif
