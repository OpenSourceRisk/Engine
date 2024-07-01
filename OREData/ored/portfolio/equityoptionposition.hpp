/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/equityoptionposition.hpp
    \brief Equity Option Position trade data model and serialization
    \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/optiondata.hpp>
#include <ored/portfolio/trade.hpp>

#include <qle/indexes/genericindex.hpp>

#include <ql/instruments/vanillaoption.hpp>

namespace ore {
namespace data {

using namespace ore::data;

//! Serializable Equity Option Underlying Data, this represents one underlying in EquityOptionPositionData
class EquityOptionUnderlyingData : public XMLSerializable {
public:
    EquityOptionUnderlyingData() {}
    EquityOptionUnderlyingData(const EquityUnderlying& underlying, const OptionData& optionData, const Real strike)
        : underlying_(underlying), optionData_(optionData), strike_(strike) {}

    const EquityUnderlying& underlying() const { return underlying_; }
    const OptionData& optionData() const { return optionData_; }
    Real strike() const { return strike_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    EquityUnderlying underlying_;
    OptionData optionData_;
    Real strike_;
};

//! Serializable Equity Option Position Data
class EquityOptionPositionData : public XMLSerializable {
public:
    EquityOptionPositionData() {}
    EquityOptionPositionData(const Real quantity, const std::vector<EquityOptionUnderlyingData>& underlyings)
        : quantity_(quantity), underlyings_(underlyings) {}

    Real quantity() const { return quantity_; }
    const std::vector<EquityOptionUnderlyingData>& underlyings() const { return underlyings_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    Real quantity_ = QuantLib::Null<Real>();
    std::vector<EquityOptionUnderlyingData> underlyings_;
};

//! Serializable Equity Option Position
class EquityOptionPosition : public Trade {
public:
    EquityOptionPosition() : Trade("EquityOptionPosition") {}
    EquityOptionPosition(const Envelope& env, const EquityOptionPositionData& data)
        : Trade("EquityOptionPosition", env), data_(data) {}

    // trade interface
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    // additional inspectors
    const EquityOptionPositionData& data() const { return data_; }
    /* the underlying option instruments */
    const std::vector<QuantLib::ext::shared_ptr<QuantLib::VanillaOption>>& options() const { return options_; }
    /* by convention, these are generic indices of the form
       GENERIC-MD/EQUITY_OPTION/PRICE/RIC:GOGO.OQ/USD/2020-03-20/2250/P
       holding historical price information on the underlying option, needed for GenericTRS trades on
       an equity option position. */
    /* the underlying equity option currencies (equal to the equity currencies) */
    const std::vector<Real>& positions() const { return positions_; }
    const std::vector<std::string>& currencies() const { return currencies_; }
    const std::vector<QuantLib::ext::shared_ptr<QuantExt::GenericIndex>> historicalPriceIndices() { return indices_; }
    const std::vector<Real>& weights() const { return weights_; }
    bool isSingleCurrency() const { return isSingleCurrency_; }

    /*! we allow to set the npv currency to a different currency than the default npv currency = first asset's
      currency; in this case a conversion rate from the default to the new currency has to be provided */
    void setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion);

private:
    EquityOptionPositionData data_;
    // populated during build()
    std::vector<QuantLib::ext::shared_ptr<QuantLib::VanillaOption>> options_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::GenericIndex>> indices_;
    std::vector<Real> positions_;
    std::vector<std::string> currencies_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    bool isSingleCurrency_;
};

//! Equity Option Position instrument wrapper
class EquityOptionPositionInstrumentWrapper : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;

    EquityOptionPositionInstrumentWrapper(const Real quantity,
                                          const std::vector<QuantLib::ext::shared_ptr<QuantLib::VanillaOption>>& options,
                                          const std::vector<Real>& positions,
                                          const std::vector<Real>& weights,
                                          const std::vector<Handle<Quote>>& fxConversion = {});

    void setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;
    //@}

private:
    Real quantity_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::VanillaOption>> options_;
    std::vector<Real> weights_;
    std::vector<Real> positions_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
};

class EquityOptionPositionInstrumentWrapper::arguments : public virtual QuantLib::PricingEngine::arguments {
public:
    Real quantity_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::VanillaOption>> options_;
    std::vector<Real> weights_;
    std::vector<Real> positions_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
    void validate() const override {}
};

class EquityOptionPositionInstrumentWrapper::results : public Instrument::results {
public:
    void reset() override {}
};

class EquityOptionPositionInstrumentWrapper::engine
    : public QuantLib::GenericEngine<EquityOptionPositionInstrumentWrapper::arguments,
                                     EquityOptionPositionInstrumentWrapper::results> {};

class EquityOptionPositionInstrumentWrapperEngine : public EquityOptionPositionInstrumentWrapper::engine {
public:
    void calculate() const override;
};

} // namespace data
} // namespace ore
