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

/*! \file qle/termstructures/crosscurrencypricetermstructure.hpp
    \brief Price term structure in a given currency derived from a price term structure in another currency
*/

#ifndef quantext_cross_currency_price_term_structure_hpp
#define quantext_cross_currency_price_term_structure_hpp

#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

//! Cross currency price term structure
/*! This class creates a price term structure in a given currency using an already constructed price term structure
    in a different currency.

    \ingroup termstructures
*/
class CrossCurrencyPriceTermStructure : public PriceTermStructure {
public:
    //! \name Constructors
    //@{
    /*! Fixed reference date based price term structure.
        \param referenceDate   This price term structure's reference date.
        \param basePriceTs     The price term structure in base currency units.
        \param fxSpot          The number of units of this price term structure's currency per unit of the base price
                               term structure's currency.
        \param baseCurrencyYts The yield term structure for the base currency.
        \param yts             The yield term structure for this price term structure's currency.
        \param currency        The price term structure's currency.
    */
    CrossCurrencyPriceTermStructure(const QuantLib::Date& referenceDate,
                                    const QuantLib::Handle<PriceTermStructure>& basePriceTs,
                                    const QuantLib::Handle<QuantLib::Quote>& fxSpot,
                                    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurrencyYts,
                                    const QuantLib::Handle<QuantLib::YieldTermStructure>& yts,
                                    const QuantLib::Currency& currency);

    /*! Floating reference date based price term structure.
        \param settlementDays  This price term structure's settlement days.
        \param basePriceTs     The price term structure in base currency units.
        \param fxSpot          The number of units of this price term structure's currency per unit of the base price
                               term structure's currency.
        \param baseCurrencyYts The yield term structure for the base currency.
        \param yts             The yield term structure for this price term structure's currency.
        \param currency        The price term structure's currency.
    */
    CrossCurrencyPriceTermStructure(QuantLib::Natural settlementDays,
                                    const QuantLib::Handle<PriceTermStructure>& basePriceTs,
                                    const QuantLib::Handle<QuantLib::Quote>& fxSpot,
                                    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurrencyYts,
                                    const QuantLib::Handle<QuantLib::YieldTermStructure>& yts,
                                    const QuantLib::Currency& currency);
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    QuantLib::Time maxTime() const override;
    //@}

    //! \name PriceTermStructure interface
    //@{
    QuantLib::Time minTime() const override;
    std::vector<QuantLib::Date> pillarDates() const override;
    const QuantLib::Currency& currency() const override { return currency_; }
    //@}

    //! \name Inspectors
    //@{
    //! The price term structure in base currency
    const QuantLib::Handle<PriceTermStructure>& basePriceTs() const { return basePriceTs_; }

    //! The FX spot rate, number of units of this price term structure's currency per unit of the base currency
    const QuantLib::Handle<QuantLib::Quote>& fxSpot() const { return fxSpot_; }

    //! The yield term structure for the base currency
    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurrencyYts() const { return baseCurrencyYts_; }

    //! The yield term structure for this price term structure's currency
    const QuantLib::Handle<QuantLib::YieldTermStructure>& yts() const { return yts_; }
    //@}

protected:
    //! Price calculation
    QuantLib::Real priceImpl(QuantLib::Time t) const override;

private:
    //! Register with underlying market data
    void registration();

    QuantLib::Handle<PriceTermStructure> basePriceTs_;
    QuantLib::Handle<QuantLib::Quote> fxSpot_;
    QuantLib::Handle<QuantLib::YieldTermStructure> baseCurrencyYts_;
    QuantLib::Handle<QuantLib::YieldTermStructure> yts_;
    QuantLib::Currency currency_;
};

} // namespace QuantExt

#endif
