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

// ---------------------------------------------------------------------------
// CalibrationConfiguration
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::CalibrationConfiguration)
namespace ore {
namespace data {
class CalibrationConfiguration : public ore::data::XMLSerializable {
public:
    CalibrationConfiguration(QuantLib::Real rmseTolerance = 0.0001,
                             QuantLib::Size maxIterations = 50);
    QuantLib::Real rmseTolerance() const;
    QuantLib::Size maxIterations() const;
    void add(const std::string& name, QuantLib::Real lowerBound, QuantLib::Real upperBound);
    std::pair<QuantLib::Real, QuantLib::Real> boundaries(const std::string& name) const;
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};
} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// IrLgmData / builder hierarchy
// ---------------------------------------------------------------------------

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
    QuantLib::Handle<QuantExt::IrModel> model() const;
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
    QuantLib::Handle<QuantExt::LGM> modelAsLgm() const;
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
    QuantLib::Handle<QuantExt::HwModel> modelAsHw() const;
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

// ---------------------------------------------------------------------------
// HestonModelCalibration
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::HestonModelCalibration)
namespace ore {
namespace data {
class HestonModelCalibration {
public:
    HestonModelCalibration(
        const std::string& indexName,
        const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
        const std::vector<QuantLib::Period>& expiries = {},
        const std::vector<QuantLib::Real>& moneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
        const std::vector<QuantLib::Period>& varianceTerms = {},
        const std::vector<QuantLib::Real>& initialValues = {0.04, 1.0, 0.5, -0.9, 0.04},
        const std::vector<bool>& fixedValues = {false, false, false, false, false},
        const std::string& calibrationMethod = "ConstantBestFit",
        const std::vector<QuantLib::Real>& maximumInitialValues = {0.1, 20.0, 3.0, 0.9, 0.1},
        QuantLib::Real relaxedFellerConstraint = 1.0,
        QuantLib::Size maxCalibrationAttempts = 0,
        QuantLib::Real earlyExitThreshold = 0.005,
        QuantLib::Real maxAcceptableError = 0.05,
        const HestonProcess::Discretization& discretization = HestonProcess::QuadraticExponential,
        const bool dontCalibrate = false);

    QuantLib::ext::shared_ptr<QuantLib::HestonModel> model();
};
} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// HestonModelBuilder
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::HestonModelBuilder)
namespace ore {
namespace data {
class HestonModelBuilder : public AssetModelBuilderBase {
public:
    HestonModelBuilder(
        const std::vector<std::string>& indices,
        const std::vector<QuantLib::Handle<QuantLib::YieldTermStructure>>& curves,
        const std::vector<QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>>& processes,
        const std::set<QuantLib::Date>& simulationDates = {},
        const std::set<QuantLib::Date>& addDates = {},
        const QuantLib::Size timeStepsPerYear = 1,
        const std::vector<QuantLib::Period>& calibrationExpiries = {},
        const std::vector<QuantLib::Real>& calibrationMoneyness = {-2.0, -1.0, 0.0, 1.0, 2.0},
        const std::vector<QuantLib::Period>& calibrationVarianceTerms = {},
        const std::vector<QuantLib::Real>& initialValues = {0.04, 1.0, 0.5, -0.5, 0.04},
        const std::vector<bool>& fixedValues = {false, false, false, false, false},
        const std::string& calibrationMethod = "ConstantBestFit",
        const std::vector<QuantLib::Real>& maximumInitialValues = {0.1, 20.0, 10.0, 0.9, 0.1},
        QuantLib::Real relaxedFellerConstraint = 1.0,
        QuantLib::Size maxCalibrationAttempts = 50,
        QuantLib::Real earlyExitThreshold = 0.005,
        QuantLib::Real maxAcceptableError = 0.05,
        const HestonProcess::Discretization& discretization = HestonProcess::QuadraticExponential,
        const std::string& referenceCalibrationGrid = "",
        const bool dontCalibrate = false,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurve = {});

    std::vector<QuantLib::ext::shared_ptr<QuantLib::StochasticProcess>> getCalibratedProcesses() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
};
} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// EqBsBuilder
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::EqBsBuilder)
namespace ore {
namespace data {
class EqBsBuilder : public QuantExt::ModelBuilder {
public:
    EqBsBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                const QuantLib::ext::shared_ptr<EqBsData>& data,
                const QuantLib::Currency& baseCcy,
                const std::string& configuration = Market::defaultConfiguration,
                const std::string& referenceCalibrationGrid = "",
                const std::string& id = "unknown");

    QuantLib::Real error() const;
    std::string eqName();
    QuantLib::ext::shared_ptr<QuantExt::EqBsParametrization> parametrization() const;
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void setCalibrationDone() const;
};
} // namespace data
} // namespace ore

// ---------------------------------------------------------------------------
// CommoditySchwartzModelBuilder
// ---------------------------------------------------------------------------

%shared_ptr(ore::data::CommoditySchwartzModelBuilder)
namespace ore {
namespace data {
class CommoditySchwartzModelBuilder : public QuantExt::ModelBuilder {
public:
    CommoditySchwartzModelBuilder(
        const QuantLib::ext::shared_ptr<ore::data::Market>& market,
        const QuantLib::ext::shared_ptr<CommoditySchwartzData>& data,
        const QuantLib::Currency& baseCcy,
        const std::string& configuration = Market::defaultConfiguration,
        const std::string& referenceCalibrationGrid = "");

    QuantLib::Real error() const;
    std::string name();
    std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> optionBasket() const;
    void forceRecalculate() override;
    bool requiresRecalibration() const override;
    void setCalibrationDone() const;
};
} // namespace data
} // namespace ore

#endif
