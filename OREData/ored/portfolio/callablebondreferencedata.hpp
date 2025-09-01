/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/callablebondreferencedata.hpp
    \brief reference data
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/callablebond.hpp>
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

//! Callable bond reference data
class CallableBondReferenceDatum : public ReferenceDatum {
public:
    static constexpr const char* TYPE = "CallableBond";

    CallableBondReferenceDatum() : callData_("CallData"), putData_("PutData") {}

    CallableBondReferenceDatum(const string& id)
        : ReferenceDatum(TYPE, id), callData_("CallData"), putData_("PutData") {}

    CallableBondReferenceDatum(const string& id, const BondReferenceDatum::BondData& bondData,
                               const CallableBondData::CallabilityData& callData,
                               const CallableBondData::CallabilityData& putData)
        : ReferenceDatum(TYPE, id), bondData_(bondData), callData_(callData), putData_(putData) {}

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const BondReferenceDatum::BondData& bondData() const { return bondData_; }
    const CallableBondData::CallabilityData& callData() const { return callData_; }
    const CallableBondData::CallabilityData& putData() const { return putData_; }

    void setBondData(const BondReferenceDatum::BondData& bondData) { bondData_ = bondData; }
    void setCallData(const CallableBondData::CallabilityData& callData) { callData_ = callData; }
    void setPutData(const CallableBondData::CallabilityData& putData) { putData_ = putData; }

private:
    BondReferenceDatum::BondData bondData_;
    CallableBondData::CallabilityData callData_;
    CallableBondData::CallabilityData putData_;
};

} // namespace data
} // namespace ore
