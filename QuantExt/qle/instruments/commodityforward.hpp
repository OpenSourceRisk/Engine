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

/*! \file qle/instruments/commodityforward.hpp
    \brief Instrument representing a commodity forward contract

    \ingroup instruments
*/

#ifndef quantext_commodity_forward_hpp
#define quantext_commodity_forward_hpp

#include <ql/currency.hpp>
#include <ql/instrument.hpp>
#include <ql/position.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {

/*! Instrument representing a commodity forward contract.

    \ingroup instruments
*/
class CommodityForward : public QuantLib::Instrument {
public:
    class arguments;
    class engine;

    //! \name Constructors
    //@{
    /*! Constructs a cash settled or physically settled commodity forward instrument.
        
        \param index             The underlying commodity index.
        \param currency          The currency of the commodity trade.
        \param position          Long (Short) for buying (selling) commodity forward
        \param quantity          Number of underlying commodity units referenced
        \param maturityDate      Maturity date of forward. For a cash settled forward, this is the date on which the 
                                 underlying price is observed.
        \param strike            The agreed forward price
        \param physicallySettled Set to \c true if the forward is physically settled and \c false if the forward is 
                                 cash settled. If omitted, physical settlement is assumed.
        \param paymentDate       If the forward is cash settled, provide a date on or after the \p maturityDate for 
                                 the cash settlement payment. If omitted, it is assumed equal to \p maturityDate.
        \param payCcy            If cash settled, the settlement currency
        \param fixingDate        If cash settled, the fixing date
        \param fxIndex           If cash settled, the FX index from which to take the fixing on the fixing date
    */
    CommodityForward(const QuantLib::ext::shared_ptr<CommodityIndex>& index, const QuantLib::Currency& currency,
                     QuantLib::Position::Type position, QuantLib::Real quantity, const QuantLib::Date& maturityDate,
                     QuantLib::Real strike, bool physicallySettled = true, const Date& paymentDate = Date(),
                     const QuantLib::Currency& payCcy = Currency(), const Date& fixingDate = Date(),
                     const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr);
    //@}

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<CommodityIndex>& index() const { return index_; }
    const QuantLib::Currency& currency() const { return currency_; }
    QuantLib::Position::Type position() const { return position_; }
    QuantLib::Real quantity() const { return quantity_; }
    const QuantLib::Date& maturityDate() const { return maturityDate_; }
    QuantLib::Real strike() const { return strike_; }
    bool physicallySettled() const { return physicallySettled_; }
    const QuantLib::Date& paymentDate() const { return paymentDate_; }
    Currency payCcy() const { return payCcy_; }
    Date fixingDate() const { return fixingDate_; }
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex() const { return fxIndex_; }
    //@}

private:
    QuantLib::ext::shared_ptr<CommodityIndex> index_;
    QuantLib::Currency currency_;
    QuantLib::Position::Type position_;
    QuantLib::Real quantity_;
    QuantLib::Date maturityDate_;
    QuantLib::Real strike_;
    bool physicallySettled_;
    QuantLib::Date paymentDate_;
    Currency payCcy_;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex_;
    Date fixingDate_;
};

//! \ingroup instruments
class CommodityForward::arguments : public virtual QuantLib::PricingEngine::arguments {
public:
    QuantLib::ext::shared_ptr<CommodityIndex> index;
    QuantLib::Currency currency;
    QuantLib::Position::Type position;
    QuantLib::Real quantity;
    QuantLib::Date maturityDate;
    QuantLib::Real strike;
    bool physicallySettled;
    QuantLib::Date paymentDate;
    Currency payCcy;
    QuantLib::ext::shared_ptr<FxIndex> fxIndex;
    Date fixingDate;
    void validate() const override;
};

//! \ingroup instruments
class CommodityForward::engine : public QuantLib::GenericEngine<CommodityForward::arguments, Instrument::results> {};
} // namespace QuantExt

#endif
