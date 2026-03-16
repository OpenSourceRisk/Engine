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
    \brief Helper for bootstrapping optionlet volatilities from cap floor volatilities
    \ingroup termstructures
*/

#ifndef quantext_cap_floor_helper_hpp
#define quantext_cap_floor_helper_hpp

#include <ql/instruments/capfloor.hpp>
#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

/*! Helper for bootstrapping optionlet volatilities from cap floor volatilities. The helper has a date schedule that is
    relative to the global evaluation date.

    \ingroup termstructures
*/
class CapFloorHelper : public QuantLib::RelativeDateBootstrapHelper<QuantLib::OptionletVolatilityStructure> {

public:
    /*! Enum to indicate whether the instrument underlying the helper is a Cap, a Floor or should be chosen
        automatically. If Automatic is chosen and the quote type is volatility, the instrument's ATM rate is queried
        and if it is greater than the strike, the instrument is a Floor otherwise it is a Cap.
    */
    enum Type { Cap, Floor, Automatic };

    //! Enum to indicate the type of the quote provided with the CapFloorHelper
    enum QuoteType { Premium, Volatility };

    /*! Constructor
        \param type                The CapFloorHelper type as described above
        \param tenor               The underlying cap floor instrument's tenor
        \param strike              The underlying cap floor instrument's strike. Setting this to \c Null<Real>()
                                   indicates that the ATM strike for the given \p tenor should be used.
        \param quote               The quoted premium or implied volatility for the underlying cap floor instrument
        \param iborIndex           The IborIndex underlying the cap floor instrument
        \param discountingCurve    The curve used for discounting the cap floor instrument cashflows
        \param moving              If this is \c true, the helper's schedule is relative to the global evaluation date
                                   and is updated if the global evaluation date is updated. If \c false, the helper
                                   has a fixed schedule relative to the \p effectiveDate that does not change with
                                   changes in the global evaluation date.
        \param effectiveDate       The effective date of the underlying cap floor instrument. If this is the empty
                                   \c QuantLib::Date(), the effective date is determined from the global evaluation
                                   date.
        \param quoteType           The quote type
        \param quoteVolatilityType If the quote type is \c Volatility, this indicates the volatility type of the quote
        \param quoteDisplacement   If the quote type is \c Volatility and the volatility type is \c ShiftedLognormal,
                                   this provides the shift size associated with the quote.
        \param endOfMonth          Whether or not to use end of month adjustment when generating the cap floor schedule
        \param firstCapletExcluded Whether or not to exclude the first caplet in the underlying cap floor instrument
    */
    CapFloorHelper(Type type, const QuantLib::Period& tenor, QuantLib::Rate strike,
                   const QuantLib::Handle<QuantLib::Quote>& quote,
                   const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& iborIndex,
                   const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve, bool moving = true,
                   const QuantLib::Date& effectiveDate = QuantLib::Date(), QuoteType quoteType = Premium,
                   QuantLib::VolatilityType quoteVolatilityType = QuantLib::Normal,
                   QuantLib::Real quoteDisplacement = 0.0, bool endOfMonth = false, bool firstCapletExcluded = true);

    //! \name Inspectors
    //@{
    //! Return the cap floor instrument underlying the helper
    QuantLib::ext::shared_ptr<QuantLib::CapFloor> capFloor() const { return capFloor_; }
    //@}

    //! \name BootstrapHelper interface
    //@{
    //! Returns the \p capFloor_ instrument's premium
    QuantLib::Real impliedQuote() const override;

    //! Sets the helper's OptionletVolatilityStructure to \p ovts and sets up the pricing engine for \c capFloor_
    void setTermStructure(QuantLib::OptionletVolatilityStructure* ovts) override;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor&) override;
    //@}

private:
    //! RelativeDateBootstrapHelper interface
    void initializeDates() override;

    // Details needed to construct the instrument
    Type type_;
    QuantLib::Period tenor_;
    QuantLib::Rate strike_;
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex_;
    QuantLib::Handle<QuantLib::YieldTermStructure> discountHandle_;
    bool moving_;
    QuantLib::Date effectiveDate_;
    QuoteType quoteType_;
    QuantLib::VolatilityType quoteVolatilityType_;
    QuantLib::Real quoteDisplacement_;
    bool endOfMonth_;
    bool firstCapletExcluded_;
    QuantLib::Handle<QuantLib::Quote> rawQuote_;
    bool initialised_;

    //! The underlying instrument
    QuantLib::ext::shared_ptr<QuantLib::CapFloor> capFloor_;

    //! The OptionletVolatilityStructure Handle that we link to the \c capFloor_ instrument
    QuantLib::RelinkableHandle<QuantLib::OptionletVolatilityStructure> ovtsHandle_;

    //! A copy of the underlying instrument that is used in the npv method
    QuantLib::ext::shared_ptr<QuantLib::CapFloor> capFloorCopy_;

    //! A method to calculate the cap floor premium from a flat cap floor volatility value
    QuantLib::Real npv(QuantLib::Real quote);
};

//! In order to convert CapFloorHelper::Type to string
std::ostream& operator<<(std::ostream& out, CapFloorHelper::Type type);

//! In order to convert CapFloorHelper::QuoteType to string
std::ostream& operator<<(std::ostream& out, CapFloorHelper::QuoteType type);

} // namespace QuantExt

#endif
