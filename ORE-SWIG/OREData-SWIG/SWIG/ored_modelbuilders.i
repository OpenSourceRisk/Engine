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

#ifndef ored_modelbuilders_i
#define ored_modelbuilders_i

%include vectors.i
%include ored_crossassetmodeldata.i

%shared_ptr(ore::data::IrLgmData)
namespace ore {
namespace data {
class IrLgmData : public LgmData {
public:
    IrLgmData();
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::IrModelBuilder)
%nodefaultctor ore::data::IrModelBuilder;
namespace ore {
namespace data {
class IrModelBuilder : public ModelBuilder {
public:
    enum class FallbackType { NoFallback, FallbackRule1 };
    Real error() const;
    std::string qualifier();
    std::string ccy();
    QuantLib::ext::shared_ptr<QuantExt::IrModel> model() const;
    RelinkableHandle<YieldTermStructure> discountCurve();
    QuantLib::ext::shared_ptr<QuantExt::Parametrization> parametrization() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> swaptionBasket() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void recalibrate() const override;
    void newCalcWithoutRecalibration() const override;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::LgmBuilder)
namespace ore {
namespace data {
class LgmBuilder : public IrModelBuilder {
public:
    LgmBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
               const QuantLib::ext::shared_ptr<IrLgmData>& data,
               const std::string& configuration = Market::defaultConfiguration,
               Real bootstrapTolerance = 0.001,
               const bool continueOnError = false,
               const std::string& referenceCalibrationGrid = "",
               const bool setCalibrationInfo = false,
               const std::string& id = "unknown",
               BlackCalibrationHelper::CalibrationErrorType calibrationErrorType =
                   BlackCalibrationHelper::RelativePriceError,
               const bool allowChangingFallbacksUnderScenarios = false,
               const bool allowModelFallbacks = false,
               const bool dontCalibrate = false);
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::HwBuilder)
namespace ore {
namespace data {
class HwBuilder : public IrModelBuilder {
public:
    HwBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
              const QuantLib::ext::shared_ptr<HwModelData>& data,
              const QuantExt::IrModel::Measure measure = QuantExt::IrModel::Measure::BA,
              const QuantExt::HwModel::Discretization discretization = QuantExt::HwModel::Discretization::Euler,
              const bool evaluateBankAccount = true,
              const std::string& configuration = Market::defaultConfiguration,
              Real bootstrapTolerance = 0.001,
              const bool continueOnError = false,
              const std::string& referenceCalibrationGrid = "",
              const bool setCalibrationInfo = false,
              const std::string& id = "unknown",
              BlackCalibrationHelper::CalibrationErrorType calibrationErrorType =
                  BlackCalibrationHelper::RelativePriceError,
              const bool allowChangingFallbacksUnderScenarios = false,
              const bool allowModelFallbacks = false,
              const bool dontCalibrate = false);
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::FxBsBuilder)
namespace ore {
namespace data {
class FxBsBuilder : public ModelBuilder {
public:
    FxBsBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                const QuantLib::ext::shared_ptr<FxBsData>& data,
                const std::string& configuration = Market::defaultConfiguration,
                const std::string& referenceCalibrationGrid = "",
                const std::string& id = "unknown");
    Real error() const;
    std::string foreignCurrency();
    QuantLib::ext::shared_ptr<QuantExt::FxBsParametrization> parametrization() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void setCalibrationDone() const;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::CrossAssetModelBuilder)
%nodefaultctor ore::data::CrossAssetModelBuilder;
namespace ore {
namespace data {
class CrossAssetModelBuilder : public ModelBuilder {
public:
    Handle<QuantExt::CrossAssetModel> model() const;
    const QuantLib::ext::shared_ptr<ore::data::CrossAssetModelData>& modelData() const;
    const std::vector<Real>& swaptionCalibrationErrors();
    const std::vector<Real>& fxOptionCalibrationErrors();
    const std::vector<Real>& eqOptionCalibrationErrors();
    const std::vector<Real>& inflationCalibrationErrors();
    const std::vector<Real>& comOptionCalibrationErrors();
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void recalibrate() const override;
    void newCalcWithoutRecalibration() const override;
};

} // namespace data
} // namespace ore

#endif
