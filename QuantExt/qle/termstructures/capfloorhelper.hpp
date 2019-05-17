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

/*! \file qle/termstructures/capfloorhelper.hpp
    \brief Helper for bootstrapping optionlet volatilties from cap floor volatilities
    \ingroup termstructures
*/

#ifndef quantext_cap_floor_helper_hpp
#define quantext_cap_floor_helper_hpp

#include <ql/instruments/capfloor.hpp>
#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

/*! Tenor based helper for bootstrapping optionlet volatilties from cap floor volatilities
    \ingroup termstructures
*/
class CapFloorHelper : public QuantLib::RelativeDateBootstrapHelper<QuantLib::OptionletVolatilityStructure> {
public:
    //! Enum to indicate the type of the quote provided with the CapFloorHelper
    enum QuoteType { Premium, Volatility };

    /*! Constructor
        \param type                The underlying instrument type
        \param tenor               The underlying cap floor instrument's tenor
        \param strike              The underlying cap floor instrument's strike
        \param premium             The quoted premium or implied volatility for the underlying cap floor instrument
        \param iborIndex           The IborIndex underlying the cap floor instrument
        \param discountingCurve    The curve used for discounting the cap floor instrument cashflows
        \param quoteType           The quote type
        \param quoteVolatilityType If the quote type is \c Volatility, this indicates the volatility type of the quote
        \param quoteDisplacement   If the quote type is \c Volatility and the volatility type is \c ShiftedLognormal, 
                                   this provides the shift size associated with the quote. 
        \param endOfMonth          Whether or not to use end of month adjustment when generating the cap floor schedule
    */
    CapFloorHelper(
        QuantLib::CapFloor::Type type,
        const QuantLib::Period& tenor,
        QuantLib::Rate strike,
        const QuantLib::Handle<QuantLib::Quote>& quote,
        const boost::shared_ptr<QuantLib::IborIndex>& iborIndex,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve,
        QuoteType quoteType = Premium,
        QuantLib::VolatilityType quoteVolatilityType = QuantLib::Normal,
        QuantLib::Real quoteDisplacement = 0.0,
        bool endOfMonth = false);

    //! \name Inspectors
    //@{
    //! Return the cap floor instrument underlying the helper
    boost::shared_ptr<QuantLib::CapFloor> capFloor() const { return capFloor_; }
    //@}

    //! \name BootstrapHelper interface
    //@{
    //! Returns the \p capFloor_ instrument's premium
    QuantLib::Real impliedQuote() const;

    //! Sets the helper's OptionletVolatilityStructure to \p ovts and sets up the pricing engine for \c capFloor_
    void setTermStructure(QuantLib::OptionletVolatilityStructure* ovts);
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&);
    //@}

protected:
    //! RelativeDateBootstrapHelper interface
    void initializeDates();

    // Details needed to construct the instrument
    QuantLib::CapFloor::Type type_;
    QuantLib::Period tenor_;
    QuantLib::Rate strike_;
    boost::shared_ptr<QuantLib::IborIndex> iborIndex_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountHandle_;
    QuoteType quoteType_;
    QuantLib::VolatilityType quoteVolatilityType_;
    QuantLib::Real quoteDisplacement_;
    bool endOfMonth_;

    //! The underlying instrument
    boost::shared_ptr<QuantLib::CapFloor> capFloor_;

    //! The OptionletVolatilityStructure Handle that we link to the \c capFloor_ instrument
    QuantLib::RelinkableHandle<QuantLib::OptionletVolatilityStructure> ovtsHandle_;
};

//! In order to convert CapFloorHelper::QuoteType to string
std::ostream& operator<<(std::ostream& out, CapFloorHelper::QuoteType type);

}

#endif
