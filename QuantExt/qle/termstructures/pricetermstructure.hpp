/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/pricetermstructure.hpp
    \brief Term structure of prices
*/

#ifndef quantext_price_term_structure_hpp
#define quantext_price_term_structure_hpp

#include <ql/currency.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quote.hpp>
#include <ql/termstructure.hpp>

namespace QuantExt {

//! Price term structure
/*! This abstract class defines the interface of concrete
    price term structures which will be derived from this one.

    \ingroup termstructures
*/
class PriceTermStructure : public QuantLib::TermStructure {
public:
    //! \name Constructors
    //@{
    PriceTermStructure(const QuantLib::DayCounter& dc = QuantLib::DayCounter());
    PriceTermStructure(const QuantLib::Date& referenceDate, const QuantLib::Calendar& cal = QuantLib::Calendar(),
                       const QuantLib::DayCounter& dc = QuantLib::DayCounter());
    PriceTermStructure(QuantLib::Natural settlementDays, const QuantLib::Calendar& cal,
                       const QuantLib::DayCounter& dc = QuantLib::DayCounter());
    //@}

    //! \name Prices
    //@{
    QuantLib::Real price(QuantLib::Time t, bool extrapolate = false) const;
    QuantLib::Real price(const QuantLib::Date& d, bool extrapolate = false) const;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! The minimum time for which the curve can return values
    virtual QuantLib::Time minTime() const;

    //! The currency in which prices are expressed
    virtual const QuantLib::Currency& currency() const = 0;

    //! The pillar dates for the PriceTermStructure
    virtual std::vector<QuantLib::Date> pillarDates() const = 0;

protected:
    /*! \name Calculations

        This method must be implemented in derived classes to
        perform the actual calculations.
    */
    //@{
    //! Price calculation
    virtual QuantLib::Real priceImpl(QuantLib::Time) const = 0;
    //@}

    //! Extra time range check for minimum time, then calls TermStructure::checkRange
    void checkRange(QuantLib::Time t, bool extrapolate) const;
};

//! Helper class so that the spot price can be pulled from the price curve each time the spot price is requested.
class DerivedPriceQuote : public QuantLib::Quote, public QuantLib::Observer {

public:
    DerivedPriceQuote(const QuantLib::Handle<PriceTermStructure>& priceTs, const QuantLib::Date& date = QuantLib::Date());

    //! \name Quote interface
    //@{
    QuantLib::Real value() const override;
    bool isValid() const override;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

private:
    QuantLib::Handle<PriceTermStructure> priceTs_;
    QuantLib::Date date_;
};

} // namespace QuantExt

#endif
