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

/*! \file qle/termstructures/aposurface.hpp
    \brief Average future price option surface derived from future option surface
    \ingroup termstructures
 */

#ifndef quantext_apo_surface_hpp
#define quantext_apo_surface_hpp

#include <boost/optional.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/pricingengines/commodityapoengine.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>
#include <qle/termstructures/pricetermstructure.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Average future price option (APO) surface derived from a future option surface
class ApoFutureSurface : public QuantLib::LazyObject, public QuantLib::BlackVolatilityTermStructure {

public:
    ApoFutureSurface(const QuantLib::Date& referenceDate, const std::vector<QuantLib::Real>& moneynessLevels,
                     const QuantLib::ext::shared_ptr<CommodityIndex>& index,
                     const QuantLib::Handle<PriceTermStructure>& pts,
                     const QuantLib::Handle<QuantLib::YieldTermStructure>& yts,
                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& expCalc,
                     const QuantLib::Handle<QuantLib::BlackVolTermStructure>& baseVts,
                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseExpCalc, QuantLib::Real beta = 0.0,
                     bool flatStrikeExtrapolation = true,
                     const boost::optional<QuantLib::Period>& maxTenor = boost::none);

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Real minStrike() const override;
    QuantLib::Real maxStrike() const override;

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<BlackVarianceSurfaceMoneyness>& vts() const;
    //@}

protected:
    //! \name BlackVolatilityTermStructure
    //@{
    QuantLib::Volatility blackVolImpl(QuantLib::Time t, QuantLib::Real strike) const override;
    //@}

private:
    QuantLib::ext::shared_ptr<CommodityIndex> index_;
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> baseExpCalc_;

    //! The APO schedule dates.
    std::vector<QuantLib::Date> apoDates_;

    //! This will keep a handle on the APO vol quotes that are calculated.
    std::vector<std::vector<QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> > > vols_;

    //! The surface that is created to do the work.
    QuantLib::ext::shared_ptr<BlackVarianceSurfaceMoneyness> vts_;

    //! The engine for valuing the APOs
    QuantLib::ext::shared_ptr<CommodityAveragePriceOptionBaseEngine> apoEngine_;
};

} // namespace QuantExt

#endif
