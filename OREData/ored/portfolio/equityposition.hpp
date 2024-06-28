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

/*! \file ored/portfolio/equityposition.hpp
    \brief Equity Position trade data model and serialization
    \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/trade.hpp>

#include <qle/indexes/equityindex.hpp>

namespace ore {
namespace data {

//! Serializable Equity Position Data
class EquityPositionData : public XMLSerializable {
public:
    EquityPositionData() {}
    EquityPositionData(const Real quantity, const std::vector<EquityUnderlying>& underlyings)
        : quantity_(quantity), underlyings_(underlyings) {}

    Real quantity() const { return quantity_; }
    const std::vector<EquityUnderlying>& underlyings() const { return underlyings_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    Real quantity_ = QuantLib::Null<Real>();
    std::vector<EquityUnderlying> underlyings_;
};

//! Serializable Equity Position
class EquityPosition : public Trade {
public:
    EquityPosition() : Trade("EquityPosition") {}
    EquityPosition(const Envelope& env, const EquityPositionData& data) : Trade("EquityPosition", env), data_(data) {}

    // trade interface
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    // additional inspectors
    const EquityPositionData& data() const { return data_; }
    const std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>>& indices() const { return indices_; }
    const std::vector<Real>& weights() const { return weights_; }
    bool isSingleCurrency() const { return isSingleCurrency_; }

    /*! we allow to set the npv currency to a different currency than the default npv currency = first asset's
      currency; in this case a conversion rate from the default to the new currency has to be provided */
    void setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion);

private:
    EquityPositionData data_;
    // populated during build()
    std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>> indices_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    bool isSingleCurrency_;
};

//! Equity Position instrument wrapper
class EquityPositionInstrumentWrapper : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;

    EquityPositionInstrumentWrapper(const Real quantity,
                                    const std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>>& equities,
                                    const std::vector<Real>& weights,
                                    const std::vector<Handle<Quote>>& fxConversion = {});
    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(QuantLib::PricingEngine::arguments*) const override;
    void fetchResults(const QuantLib::PricingEngine::results*) const override;
    //@}

    void setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion);

private:
    void setupExpired() const override;
    Real quantity_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>> equities_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
};

class EquityPositionInstrumentWrapper::arguments : public virtual QuantLib::PricingEngine::arguments {
public:
    Real quantity_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>> equities_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
    void validate() const override {}
};

class EquityPositionInstrumentWrapper::results : public Instrument::results {
public:
    void reset() override {}
};

class EquityPositionInstrumentWrapper::engine
    : public QuantLib::GenericEngine<EquityPositionInstrumentWrapper::arguments,
                                     EquityPositionInstrumentWrapper::results> {};

class EquityPositionInstrumentWrapperEngine : public EquityPositionInstrumentWrapper::engine {
public:
    void calculate() const override;
};

} // namespace data
} // namespace ore
