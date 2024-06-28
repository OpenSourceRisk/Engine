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

/*! \file ored/portfolio/trade.hpp
    \brief base trade data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/instrumentwrapper.hpp>
#include <ored/portfolio/premiumdata.hpp>
#include <ored/portfolio/tradeactions.hpp>
#include <ored/portfolio/tradefactory.hpp> // just convenience so that client code needs to include trade.hpp only

#include <ored/utilities/parsers.hpp>

#include <ql/cashflow.hpp>
#include <ql/instrument.hpp>
#include <ql/time/date.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using QuantLib::Date;
using std::string;

//! Trade base class
/*! Instrument interface to pricing and risk applications
    Derived classes should
    - contain additional serializable data classes
    - implement a build() function that parses data and constructs QuantLib
      and QuantExt objects
 \ingroup portfolio
*/
class Trade : public XMLSerializable {
public:

    //! Default constructor
    Trade() {}

    //! Base class constructor
    Trade(const string& tradeType, const Envelope& env = Envelope(), const TradeActions& ta = TradeActions())
        : tradeType_(tradeType), envelope_(env), tradeActions_(ta) {
        reset();
    }

    //! Default destructor
    virtual ~Trade() {}

    /*! Build QuantLib/QuantExt instrument, link pricing engine. If build() is called multiple times, reset() should
        be called between these calls. */
    virtual void build(const QuantLib::ext::shared_ptr<EngineFactory>&) = 0;

    /*! Return the fixings that will be requested in order to price this Trade given the \p settlementDate.



        If the \p settlementDate is not provided, the current evaluation date is taken as the settlement date.
        If a Trade does not have any fixings, this method will return an empty map.
        The map key is the ORE name of the index and the map value is the set of fixing dates.

        \warning This method will return an empty map if the Trade has not been built.
    */
    virtual std::map<std::string,  RequiredFixings::FixingDates>
    fixings(const QuantLib::Date& settlementDate = QuantLib::Date()) const {
        return requiredFixings_.fixingDatesIndices(settlementDate);
    }

    /*! Return the full required fixing information */
    const RequiredFixings& requiredFixings() const { return requiredFixings_; }

    virtual std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const {
        return {};
    }

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! Reset trade, clear all base class data. This does not reset accumulated timings for this trade.
    void reset();

    //! Reset accumulated timings to given values
    void resetPricingStats(const std::size_t numberOfPricings = 0,
                           const boost::timer::nanosecond_type cumulativePricingTime = 0) {
        savedNumberOfPricings_ = numberOfPricings;
        savedCumulativePricingTime_ = cumulativePricingTime;
        if (instrument_ != nullptr)
            instrument_->resetPricingStats();
    }

    //! \name Setters
    //@{
    //! Set the trade id
    string& id() { return id_; }

    //! Set the envelope with counterparty and portfolio info
    void setEnvelope(const Envelope& envelope);

    void setAdditionalData(const std::map<std::string, boost::any>& additionalData);

    //! Set the trade actions
    TradeActions& tradeActions() { return tradeActions_; }
    //@}

    //! \name Inspectors
    //@{
    const string& id() const { return id_; }

    const string& tradeType() const { return tradeType_; }

    const Envelope& envelope() const { return envelope_; }

    const set<string>& portfolioIds() const { return envelope().portfolioIds(); }

    const TradeActions& tradeActions() const { return tradeActions_; }

    const QuantLib::ext::shared_ptr<InstrumentWrapper>& instrument() const { return instrument_; }

    const std::vector<QuantLib::Leg>& legs() const { return legs_; }

    const std::vector<string>& legCurrencies() const { return legCurrencies_; }

    const std::vector<bool>& legPayers() const { return legPayers_; }

    const string& npvCurrency() const { return npvCurrency_; }

    //! Return the current notional in npvCurrency. See individual sub-classes for the precise definition
    // of notional, for exotic trades this may not be what you expect.
    virtual QuantLib::Real notional() const { return notional_; }

    virtual string notionalCurrency() const { return notionalCurrency_; }

    const Date& maturity() const { return maturity_; }

    virtual bool isExpired(const Date& d) { return d >= maturity_; }

    const string& issuer() const { return issuer_; }

    //! returns any additional datum.
    template <typename T> T additionalDatum(const std::string& tag) const;
    //! returns all additional data returned by the trade once built
    const virtual std::map<std::string,boost::any>& additionalData() const;

    /*! returns the sensi template, e.g. "IR_Analytical" for this trade,
        this is only available after build() has been called */
    const std::string& sensitivityTemplate() const;
    //@}

    //! \name Utility
    //@{
    //! Utility to validate that everything that needs to be set in this base class is actually set
    void validate() const;

    /*! Utility method indicating if the trade has cashflows for the cashflow report. The default implementation
        returns \c true so that a trade is automatically considered when cashflows are being written. To prevent a
        trade from being asked for its cashflows, the method can be overridden to return \c false.
    */
    virtual bool hasCashflows() const { return true; }
    //@}

    //! Get cumulative timing spent on pricing
    boost::timer::nanosecond_type getCumulativePricingTime() const {
        return savedCumulativePricingTime_ + (instrument_ != nullptr ? instrument_->getCumulativePricingTime() : 0);
    }

    //! Get number of pricings
    std::size_t getNumberOfPricings() const {
        return savedNumberOfPricings_ + (instrument_ != nullptr ? instrument_->getNumberOfPricings() : 0);
    }

protected:
    string tradeType_; // class name of the derived class
    QuantLib::ext::shared_ptr<InstrumentWrapper> instrument_;
    std::vector<QuantLib::Leg> legs_;
    std::vector<string> legCurrencies_;
    std::vector<bool> legPayers_;
    string npvCurrency_;
    QuantLib::Real notional_;
    string notionalCurrency_;
    Date maturity_;
    string issuer_;
    string sensitivityTemplate_;
    bool sensitivityTemplateSet_ = false;

    std::size_t savedNumberOfPricings_ = 0;
    boost::timer::nanosecond_type savedCumulativePricingTime_ = 0;

    // Utility to add premiums such that they are taken into account in pricing and cash flow projection.
    // For example, an option premium flow is not covered by the underlying option instrument in
    // QuantLib and needs to be represented separately. This is done by inserting it as an additional instrument
    // into the InstrumentWrapper. This utility creates the additional instrument. The actual insertion into the
    // instrument wrapper is done in the individual trade builders when they instantiate the InstrumentWrapper.
    // The returned date is the latest premium payment date added.
    Date addPremiums(std::vector<QuantLib::ext::shared_ptr<Instrument>>& instruments, std::vector<Real>& multipliers,
                     const Real tradeMultiplier, const PremiumData& premiumData, const Real premiumMultiplier,
                     const Currency& tradeCurrency, const QuantLib::ext::shared_ptr<EngineFactory>& factory,
                     const string& configuration);

    RequiredFixings requiredFixings_;
    mutable std::map<std::string,boost::any> additionalData_;

    /* sets additional data based on given internal legNo (0, 1, ...), the result leg id is derived from this
       as "legNo + 1", i.e. starting with 1 (1, 2, ...). The result leg id can be overwriten using the second
       parameter resultLegId. */
    void setLegBasedAdditionalData(const Size legNo, Size resultLegId = Null<Size>()) const;

    /* sets the sensitivity template for this trade */
    void setSensitivityTemplate(const EngineBuilder& builder);
    void setSensitivityTemplate(const std::string& id);

private:
    string id_;
    Envelope envelope_;
    TradeActions tradeActions_;
};

template <class T>
inline T Trade::additionalDatum(const std::string& tag) const {
    std::map<std::string,boost::any>::const_iterator value =
        additionalData_.find(tag);
    QL_REQUIRE(value != additionalData_.end(),
               tag << " not provided");
    return boost::any_cast<T>(value->second);
}

} // namespace data
} // namespace ore
