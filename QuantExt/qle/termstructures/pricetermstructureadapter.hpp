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

#include <qle/termstructures/pricetermstructure.hpp>

namespace QuantExt {

//! Adapter class for turning a PriceTermStructure in to a YieldTermStructure
/*! This class takes a spot quote, a price term structure and an input yield
    curve and constructs a yield curve such that the discount factor 
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
    continuously compounded zero rate from the input yield curve.
*/
class PriceTermStructureAdapter : public QuantLib::YieldTermStructure {
    
public:
    PriceTermStructureAdapter(const boost::shared_ptr<QuantLib::Quote>& spotQuote, 
        const boost::shared_ptr<PriceTermStructure>& priceCurve, 
        const boost::shared_ptr<QuantLib::YieldTermStructure>& discount);

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const;
    const QuantLib::Date& referenceDate() const;
    QuantLib::DayCounter dayCounter() const;
    //@}

    //! \name Inspectors
    //@{
    const boost::shared_ptr<QuantLib::Quote>& spotQuote() const;
    const boost::shared_ptr<PriceTermStructure>& priceCurve() const;
    const boost::shared_ptr<QuantLib::YieldTermStructure>& discount() const;
    //@}

protected:
    //! \name YieldTermStructure interface
    //@{
    QuantLib::DiscountFactor discountImpl(QuantLib::Time t) const;
    //@} 

private:
    boost::shared_ptr<QuantLib::Quote> spotQuote_;
    boost::shared_ptr<PriceTermStructure> priceCurve_;
    boost::shared_ptr<QuantLib::YieldTermStructure> discount_;
};

}

#endif
