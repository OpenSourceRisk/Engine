/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/bondposition.hpp
    \brief Bond Position trade data model and serialization
    \ingroup tradedata
 */

#pragma once

#include <ored/portfolio/bond.hpp>

#include <qle/indexes/bondindex.hpp>

namespace ore {
namespace data {

using namespace ore::data;

class BondPositionData : public XMLSerializable {
public:
    BondPositionData() {}
    BondPositionData(const Real quantity, const std::vector<BondUnderlying>& underlyings)
        : quantity_(quantity), underlyings_(underlyings) {}

    Real quantity() const { return quantity_; }
    const std::string& identifier() const { return identifier_; }
    const std::vector<BondUnderlying>& underlyings() const { return underlyings_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    void populateFromBondBasketReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& ref);

private:
    Real quantity_ = QuantLib::Null<Real>();
    std::string identifier_;
    std::vector<BondUnderlying> underlyings_;
};

class BondPosition : public Trade {
public:
    BondPosition() : Trade("BondPosition") {}
    BondPosition(const Envelope& env, const BondPositionData& data)
        : Trade("BondPosition", env), originalData_(data), data_(data) {}

    // trade interface
    void build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>&) override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    std::map<AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    // additional inspectors
    const BondPositionData& data() const { return data_; }
    const std::vector<ore::data::BondBuilder::Result>& bonds() const { return bonds_; }
    const std::vector<Real>& weights() const { return weights_; }
    const std::vector<Real>& bidAskAdjustments() const { return bidAskAdjustments_; }
    bool isSingleCurrency() const { return isSingleCurrency_; }

    /*! we allow to set the npv currency to a different currency than the default npv currency = first asset's
      currency; in this case a conversion rate from the default to the new currency has to be provided */
    void setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion);

private:
    BondPositionData originalData_, data_;
    // populated during build()
    std::vector<BondBuilder::Result> bonds_;
    std::vector<Real> weights_;
    std::vector<Real> bidAskAdjustments_;
    std::vector<Handle<Quote>> fxConversion_;
    bool isSingleCurrency_;
};

//! Equity Position instrument wrapper
class BondPositionInstrumentWrapper : public InstrumentWrapper {
public:
    BondPositionInstrumentWrapper(const Real quantity, const std::vector<QuantLib::ext::shared_ptr<QuantLib::Bond>>& bonds,
                                  const std::vector<Real>& weights, const std::vector<Real>& bidAskAdjstments,
                                  const std::vector<Handle<Quote>>& fxConversion = {});
    void initialise(const std::vector<QuantLib::Date>& dates) override {}
    void reset() override {}
    QuantLib::Real NPV() const override;
    const std::map<std::string,boost::any>& additionalResults() const override;
    void updateQlInstruments() override {}

    void setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion);

private:
    Real quantity_;
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Bond>> bonds_;
    std::vector<Real> weights_;
    std::vector<Real> bidAskAdjustments_;
    std::vector<Handle<Quote>> fxConversion_;
    Handle<Quote> npvCcyConversion_;
};

} // namespace data
} // namespace ore
