/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/quotes/commoditypricequote.hpp
    \brief A quote that tracks the front (t=0) point of a commodity price term structure
    \ingroup quotes
*/
#pragma once

#include <ql/quote.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

//! A quote for a commodity "spot" price that always reflects the current price(0) of a live PriceTermStructure.
/*! Unlike a plain SimpleQuote snapshot, this quote re-evaluates the underlying price curve on every call to
    value(), so it stays in sync with commodity price scenario updates applied to the underlying curve. It also
    registers as an observer of the price curve so that changes are propagated to observers of this quote (e.g.
    volatility surfaces built on top of it).
*/
class CommodityPriceQuote : public QuantLib::Quote, public QuantLib::Observer {
public:
    CommodityPriceQuote(QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve);
    //! \name Quote interface
    //@{
    QuantLib::Real value() const override;
    bool isValid() const override;
    //@}
    void update() override;

private:
    QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve_;
};

} // namespace QuantExt
