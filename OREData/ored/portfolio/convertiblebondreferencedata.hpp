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

/*! \file ored/portfolio/convertiblebondreferencedata.hpp
    \brief reference data
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/convertiblebonddata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/referencedata.hpp>

#include <ql/cashflow.hpp>

#include <ql/shared_ptr.hpp>

namespace ore {
namespace data {

using QuantLib::Leg;
using std::map;
using std::pair;
using std::string;
using std::vector;

//! Convertible Bond Reference data
class ConvertibleBondReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "ConvertibleBond";

    ConvertibleBondReferenceDatum() : callData_("CallData"), putData_("PutData") {}

    ConvertibleBondReferenceDatum(const string& id)
        : ReferenceDatum(TYPE, id), callData_("CallData"), putData_("PutData") {}

    ConvertibleBondReferenceDatum(const string& id, const BondReferenceDatum::BondData& bondData,
                                  const ConvertibleBondData::CallabilityData& callData,
                                  const ConvertibleBondData::CallabilityData& putData,
                                  const ConvertibleBondData::ConversionData& conversionData,
                                  const ConvertibleBondData::DividendProtectionData& dividendProtectionData)
        : ReferenceDatum(TYPE, id), bondData_(bondData), callData_(callData), putData_(putData),
          conversionData_(conversionData), dividendProtectionData_(dividendProtectionData) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const BondReferenceDatum::BondData& bondData() const { return bondData_; }
    const ConvertibleBondData::CallabilityData& callData() const { return callData_; }
    const ConvertibleBondData::CallabilityData& putData() const { return putData_; }
    const ConvertibleBondData::ConversionData& conversionData() const { return conversionData_; }
    const ConvertibleBondData::DividendProtectionData& dividendProtectionData() const {
        return dividendProtectionData_;
    }
    std::string detachable() const { return detachable_; }

    void setBondData(const BondReferenceDatum::BondData& bondData) { bondData_ = bondData; }
    void setCallData(const ConvertibleBondData::CallabilityData& callData) { callData_ = callData; }
    void setPutData(const ConvertibleBondData::CallabilityData& putData) { putData_ = putData; }
    void setConversionData(const ConvertibleBondData::ConversionData& conversionData) {
        conversionData_ = conversionData;
    }
    void setDividendProtectionData(const ConvertibleBondData::DividendProtectionData& dividendProtectionData) {
        dividendProtectionData_ = dividendProtectionData;
    }

private:
    BondReferenceDatum::BondData bondData_;
    ConvertibleBondData::CallabilityData callData_;
    ConvertibleBondData::CallabilityData putData_;
    ConvertibleBondData::ConversionData conversionData_;
    ConvertibleBondData::DividendProtectionData dividendProtectionData_;
    std::string detachable_;
};

} // namespace data
} // namespace ore
