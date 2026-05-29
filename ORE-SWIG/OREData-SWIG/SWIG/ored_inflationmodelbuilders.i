/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_inflationmodelbuilders_i
#define ored_inflationmodelbuilders_i

%include ored_modelbuilders.i

// ---------------------------------------------------------------------------
// InfJyData
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::InfJyData)
namespace ore {
namespace data {
class InfJyData : public InflationModelData {
public:
    InfJyData();
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};
} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// InfDkBuilder
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::InfDkBuilder)
namespace ore {
namespace data {
class InfDkBuilder : public QuantExt::ModelBuilder {
public:
    InfDkBuilder(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market,
        const QuantLib::ext::shared_ptr<InfDkData>& data,
        const std::string& configuration = Market::defaultConfiguration,
        const std::string& referenceCalibrationGrid = "",
        const bool dontCalibrate = false);

    std::string infIndex();
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void setCalibrationDone() const;
};
} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// InfJyBuilder
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::InfJyBuilder)
namespace ore {
namespace data {
class InfJyBuilder : public QuantExt::ModelBuilder {
public:
    InfJyBuilder(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market,
        const QuantLib::ext::shared_ptr<InfJyData>& data,
        const std::string& configuration = Market::defaultConfiguration,
        const std::string& referenceCalibrationGrid = "",
        const bool donCalibrate = false);

    std::string inflationIndex() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void setCalibrationDone() const;
};
} // namespace data
} // namespace ore

#endif
