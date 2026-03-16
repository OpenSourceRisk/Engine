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

/*! \file qle/termstructures/pricetermstructureadapter.hpp
    \brief PriceTermStructure adapter
*/

#ifndef quantext_price_term_structure_adapter_hpp
#define quantext_price_term_structure_adapter_hpp

#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

//! Adapter class for turning a PriceTermStructure in to a YieldTermStructure
/*! This class takes a price term structure and an input yield curve and constructs
    a yield curve such that the discount factor
    \f$ P_p(0, t) \f$ at time \f$ t \f$ is given by:
    \f[
    P_p(0, t) = \exp(-s(t) t)
    \f]
    where \f$ s(t) \f$ is defined by:
    \f[
    \Pi(0, t) = S(0) \exp((z(t) - s(t)) t)
    \f]
    Here, \f$ \Pi(0, t) \f$ is the forward price of the underlying from the
    input price curve, \f$ S(0) \f$ is its spot price and \f$ z(t) \f$ is the
    continuously compounded zero rate from the input yield curve. The spot price is
    determined from the price curve at time 0 by default. There are optional
    parameters that allow using a price at a time other than 0 for the spot price.
*/
class PriceTermStructureAdapter : public QuantLib::YieldTermStructure {

public:
    PriceTermStructureAdapter(const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve,
                              const QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure>& discount,
                              QuantLib::Natural spotDays = 0,
                              const QuantLib::Calendar& spotCalendar = QuantLib::NullCalendar());

    // Alternative ctor where the spot quote handle is explicitly set
    PriceTermStructureAdapter(const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve,
                              const QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure>& discount,
                              const QuantLib::Handle<QuantLib::Quote>& spotQuote);

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    const QuantLib::Date& referenceDate() const override;
    QuantLib::DayCounter dayCounter() const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve() const;
    const QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure>& discount() const;
    QuantLib::Natural spotDays() const;
    const QuantLib::Calendar& spotCalendar() const;
    //@}

protected:
    //! \name YieldTermStructure interface
    //@{
    QuantLib::DiscountFactor discountImpl(QuantLib::Time t) const override;
    //@}

private:
    QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve_;
    QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure> discount_;
    QuantLib::Natural spotDays_;
    QuantLib::Calendar spotCalendar_;
    QuantLib::Handle<QuantLib::Quote> spotQuote_;
};

} // namespace QuantExt

#endif
