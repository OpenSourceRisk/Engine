/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file portfolio/bondfutureoption.hpp
    \brief Bond Future Option data model and serialization
    \ingroup tradedata
*/
#pragma once
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/portfolio/vanillaoption.hpp>
#include <ored/portfolio/tradestrike.hpp>

namespace ore {
namespace data {

//! Serializable Bond Future Option
/*! \ingroup tradedata
 */
class BondFutureOption : public VanillaOptionTrade {
public:
    //! Default constructor
    BondFutureOption();

    //! Detailed constructor
    BondFutureOption(Envelope& env,
        OptionData optionData,
        std::string futureContractName,
        QuantLib::Real futureContractNotional,
        QuantLib::Real strikePrice);

    //! Build QuantLib or QuantExt instrument and link pricing engine.
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;

    //! Add names relating to the bond future contract.
    std::map<AssetClass, std::set<std::string>> underlyingIndices(
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    //! \name Serialization
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! Available after `build()` has been called. Contains details of the underlying bond future contract's CTD bond.
    const BondData& bondData() const { return bondData_; }

private:
    BondData bondData_;
};
} // namespace data
} // namespace ore
