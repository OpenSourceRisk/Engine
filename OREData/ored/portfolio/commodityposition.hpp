/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/commodityposition.hpp
    \brief Commodity Position trade data model and serialization
    \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/underlying.hpp>
#include <qle/indexes/commodityindex.hpp>

namespace ore {
namespace data {

//! Serializable Commodity Position Data
class CommodityPositionData : public XMLSerializable {
public:
    CommodityPositionData() {}
    CommodityPositionData(const Real quantity, const std::vector<CommodityUnderlying>& underlyings)
        : quantity_(quantity), underlyings_(underlyings) {}

    Real quantity() const { return quantity_; }
    const std::vector<CommodityUnderlying>& underlyings() const { return underlyings_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    Real quantity_ = QuantLib::Null<Real>();
    std::vector<CommodityUnderlying> underlyings_;
};

//! Serializable Commodity Position
class CommodityPosition : public Trade {
public:
    CommodityPosition() : Trade("CommodityPosition") {}
    CommodityPosition(const Envelope& env, const CommodityPositionData& data) : Trade("CommodityPosition", env), data_(data) {}

    // trade interface
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    // additional inspectors
    const CommodityPositionData& data() const { return data_; }
    const std::vector<QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>>& indices() const { return indices_; }
    const std::vector<Real>& weights() const { return weights_; }
    bool isSingleCurrency() const { return isSingleCurrency_; }

    /*! we allow to set the npv currency to a different currency than the default npv currency = first asset's
      currency; in this case a conversion rate from the default to the new currency has to be provided */
    void setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion);

private:
    CommodityPositionData data_;
    // populated during build()
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>> indices_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    bool isSingleCurrency_;
};

//! Commodity Position instrument wrapper
class CommodityPositionInstrumentWrapper : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;

    CommodityPositionInstrumentWrapper(const Real quantity,
                                       const std::vector<QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>>& commodities,
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
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>> commodities_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
};

class CommodityPositionInstrumentWrapper::arguments : public virtual QuantLib::PricingEngine::arguments {
public:
    Real quantity_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>> commodities_;
    std::vector<Real> weights_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
    void validate() const override {}
};

class CommodityPositionInstrumentWrapper::results : public Instrument::results {
public:
    void reset() override {}
};

class CommodityPositionInstrumentWrapper::engine
    : public QuantLib::GenericEngine<CommodityPositionInstrumentWrapper::arguments,
                                     CommodityPositionInstrumentWrapper::results> {};

class CommodityPositionInstrumentWrapperEngine : public CommodityPositionInstrumentWrapper::engine {
public:
    void calculate() const override;
};

} // namespace data
} // namespace ore
