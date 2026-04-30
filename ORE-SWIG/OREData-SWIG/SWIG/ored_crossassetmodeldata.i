/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#ifndef ored_crossassetmodeldata_i
#define ored_crossassetmodeldata_i
%include <std_pair.i>
%include <std_vector.i>
%include <std_string.i>
%include ored_xmlutils.i

%shared_ptr(ore::data::IrModelData)
namespace ore {
namespace data {
class IrModelData {
public:
    IrModelData(const std::string& name);
    IrModelData(const std::string& name,const std::string& qualifier, ore::data::CalibrationType calibrationType);
    const std::string& name();
    const std::string& qualifier();
    virtual std::string ccy() const;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::CalibrationInstrument)
namespace ore {
namespace data {
class CalibrationInstrument : public ore::data::XMLSerializable {
public:
    CalibrationInstrument(const std::string& instrumentType);
};

} // namespace data
} // namespace ore

%template(CalibrationInstrumentVector) std::vector<ext::shared_ptr<ore::data::CalibrationInstrument>>;

%shared_ptr(ore::data::CalibrationBasket)
namespace ore {
namespace data {
class CalibrationBasket : public ore::data::XMLSerializable {
public:
    CalibrationBasket();
    CalibrationBasket(const std::vector<ext::shared_ptr<CalibrationInstrument>>& instruments);
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

%template(CalibrationBasketVector) std::vector<ore::data::CalibrationBasket>;

%shared_ptr(ore::data::LgmData)
namespace ore {
namespace data {
class LgmData : public IrModelData {
public:
    enum class ReversionType { HullWhite, Hagan };
    enum class VolatilityType { HullWhite, Hagan };
    LgmData();
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::HwModelData)
namespace ore {
namespace data {
class HwModelData : public IrModelData {
public:
    HwModelData();
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::FxBsData)
namespace ore {
namespace data {
class FxBsData {
public:
    FxBsData();
    FxBsData(std::string foreignCcy, std::string domesticCcy, ore::data::CalibrationType calibrationType, bool calibrateSigma,
             ore::data::ParamType sigmaType, const std::vector<QuantLib::Time>& sigmaTimes, const std::vector<QuantLib::Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>());
    const std::string& foreignCcy() const;
    const std::string& domesticCcy() const;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::EqBsData)
namespace ore {
namespace data {
class EqBsData {
public:
    EqBsData() {}
    EqBsData(std::string name, std::string currency, ore::data::CalibrationType calibrationType, bool calibrateSigma,
             ore::data::ParamType sigmaType, const std::vector<QuantLib::Time>& sigmaTimes, const std::vector<QuantLib::Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>());

    const std::string& eqName() { return name_; }
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::InflationModelData)
namespace ore {
namespace data {
class InflationModelData : public ore::data::XMLSerializable {
public:
    InflationModelData();
    virtual void fromXML(ore::data::XMLNode* node) override;
};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::InfDkData)
namespace ore {
namespace data {
class InfDkData : public InflationModelData {
public:
    InfDkData();
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore

%template(IrModelDataMap) std::map<std::string, ext::shared_ptr<ore::data::IrModelData>>;
%template(FxDataMap) std::map<std::string, ext::shared_ptr<ore::data::FxData>>;
%template(EqBsDataMap) std::map<std::string, ext::shared_ptr<ore::data::EqBsData>>;
%template(InflationModelDataMap) std::map<std::string, ext::shared_ptr<ore::data::InflationModelData>>;
%template(CrLgmDataMap) std::map<std::string, ext::shared_ptr<ore::data::CrLgmData>>;
%template(CrCirDataMap) std::map<std::string, ext::shared_ptr<ore::data::CrCirData>>;
%template(CommoditySchwartzDataMap) std::map<std::string, ext::shared_ptr<ore::data::CommoditySchwartzData>>;

%template(IrModelDataVector) std::vector<ext::shared_ptr<ore::data::IrModelData>>;
%template(FxBsDataVector) std::vector<ext::shared_ptr<ore::data::FxData>>;
%template(EqBsDataVector) std::vector<ext::shared_ptr<ore::data::EqBsData>>;
%template(InflationModelDataVector) std::vector<ext::shared_ptr<ore::data::InflationModelData>>;
%template(CrLgmDataVector) std::vector<ext::shared_ptr<ore::data::CrLgmData>>;
%template(CrCirDataVector) std::vector<ext::shared_ptr<ore::data::CrCirData>>;
%template(CommoditySchwartzDataVector) std::vector<ext::shared_ptr<ore::data::CommoditySchwartzData>>;

%shared_ptr(ore::data::CrossAssetModelData)
namespace ore {
namespace data {
class CrossAssetModelData : public ore::data::XMLSerializable {
public:
    CrossAssetModelData();

    CrossAssetModelData(
        const std::vector<ext::shared_ptr<IrModelData>>& irConfigs,
        const std::vector<ext::shared_ptr<ore::data::FxData>>& fxConfigs,
        const std::map<ore::data::CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        QuantLib::Real tolerance = 1e-4,
        const std::string& measure = "LGM",
        const ore::data::CrossAssetModel::Discretization discretization = ore::data::CrossAssetModel::Discretization::Exact,
        const QuantLib::SalvagingAlgorithm::Type& salvagingAlgorithm = QuantLib::SalvagingAlgorithm::None);

    CrossAssetModelData(
        const std::vector<ext::shared_ptr<IrModelData>>& irConfigs,
        const std::vector<ext::shared_ptr<ore::data::FxData>>& fxConfigs,
        const std::vector<ext::shared_ptr<EqBsData>>& eqConfigs,
        const std::map<ore::data::CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        QuantLib::Real tolerance = 1e-4,
        const std::string& measure = "LGM",
        const ore::data::CrossAssetModel::Discretization discretization = ore::data::CrossAssetModel::Discretization::Exact,
        const QuantLib::SalvagingAlgorithm::Type& salvagingAlgorithm = QuantLib::SalvagingAlgorithm::None);

    CrossAssetModelData(
        const std::vector<ext::shared_ptr<IrModelData>>& irConfigs,
        const std::vector<ext::shared_ptr<ore::data::FxData>>& fxConfigs,
        const std::vector<ext::shared_ptr<EqBsData>>& eqConfigs,
        const std::vector<ext::shared_ptr<InflationModelData>>& infConfigs,
        const std::vector<ext::shared_ptr<ore::data::CrLgmData>>& crLgmConfigs,
        const std::vector<ext::shared_ptr<ore::data::CrCirData>>& crCirConfigs,
        const std::vector<ext::shared_ptr<ore::data::CommoditySchwartzData>>& comConfigs,
        const QuantLib::Size numberOfCreditStates,
        const std::map<ore::data::CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        QuantLib::Real tolerance = 1e-4,
        const std::string& measure = "LGM",
        const ore::data::CrossAssetModel::Discretization discretization = ore::data::CrossAssetModel::Discretization::Exact,
        const QuantLib::SalvagingAlgorithm::Type& salvagingAlgorithm = QuantLib::SalvagingAlgorithm::None);

    void clear();
    void validate();
    const std::string& domesticCurrency() const;
    const std::vector<std::string>& currencies() const;
    const std::vector<std::string>& equities() const;
    const std::vector<std::string>& infIndices() const;
    const std::vector<std::string>& creditNames() const;
    const std::vector<std::string>& commodities() const;
    const std::vector<ext::shared_ptr<IrModelData>>& irConfigs() const;
    const std::vector<ext::shared_ptr<ore::data::FxData>>& fxConfigs() const;
    const std::vector<ext::shared_ptr<EqBsData>>& eqConfigs() const ;
    const std::vector<ext::shared_ptr<InflationModelData>>& infConfigs() const;
    const std::vector<ext::shared_ptr<ore::data::CrLgmData>>& crLgmConfigs() const;
    const std::vector<ext::shared_ptr<ore::data::CrCirData>>& crCirConfigs() const;
    const std::vector<ext::shared_ptr<ore::data::CommoditySchwartzData>>& comConfigs() const;
    QuantLib::Size numberOfCreditStates() const;
    const std::map<ore::data::CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations() const;
    QuantLib::Real bootstrapTolerance() const;
    const std::string& measure() const ;
    ore::data::CrossAssetModel::Discretization discretization() const;
    QuantLib::SalvagingAlgorithm::Type getSalvagingAlgorithm() const;
    const std::string& integrationPolicy() const;
    bool piecewiseIntegration() const;

    void setCorrelations(const std::map<ore::data::CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& corrs);
    void setCorrelations(const ext::shared_ptr<ore::data::InstantaneousCorrelations>& corrs);
    void setNumberOfCreditStates(QuantLib::Size n);

    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;;
    bool operator==(const CrossAssetModelData& rhs);
    bool operator!=(const CrossAssetModelData& rhs);;

    void buildIrConfigs(std::map<std::string, ext::shared_ptr<IrModelData>>& irMap);
    void buildFxConfigs(std::map<std::string, ext::shared_ptr<ore::data::FxData>>& fxMap);
    void buildEqConfigs(std::map<std::string, ext::shared_ptr<EqBsData>>& eqMap);
    void buildInfConfigs(const std::map<std::string, ext::shared_ptr<InflationModelData>>& mp);
    void buildCrConfigs(std::map<std::string, ext::shared_ptr<ore::data::CrLgmData>>& crLgmMap,
                        std::map<std::string, ext::shared_ptr<ore::data::CrCirData>>& crCirMap);
    void buildComConfigs(std::map<std::string, ext::shared_ptr<ore::data::CommoditySchwartzData>>& comMap);

     %extend {
      void setDomesticCurrency(std::string ccy) {
          self->domesticCurrency() = ccy;
      }

      void setCurrencies(std::vector<std::string> ccys) {
          self->currencies() = ccys;
      }

      void setEquities(std::vector<std::string> eqs) {
          self->equities() = eqs;
      }

      void setInfIndices(std::vector<std::string> infs) {
          self->infIndices() = infs;
      }

      void setCreditNames(std::vector<std::string> names) {
          self->creditNames() = names;
      }

      void setCommodities(std::vector<std::string> coms) {
          self->commodities() = coms;
      }

      void setBootstrapTolerance(QuantLib::Real tol) {
          self->bootstrapTolerance() = tol;
      }

      void setMeasure(std::string m) {
          self->measure() = m;
      }

      void setDiscretization(std::string d) {
          self->discretization() = ore::data::parseDiscretization(d);
      }

      void setIrConfigs(std::vector<ext::shared_ptr<IrModelData>> i) {
          self->irConfigs() = i;
      }

    void setFxConfigs(std::vector<ext::shared_ptr<ore::data::FxData>> i) {
          self->fxConfigs() = i;
      }

      void setEqConfigs(std::vector<ext::shared_ptr<EqBsData>> i) {
          self->eqConfigs() = i;
      }

      void setInfConfigs(std::vector<ext::shared_ptr<InflationModelData>> i) {
          self->infConfigs() = i;
      }

    void setCrLgmConfigs(std::vector<ext::shared_ptr<ore::data::CrLgmData>> i) {
          self->crLgmConfigs() = i;
      }
    void setCrCirConfigss(std::vector<ext::shared_ptr<ore::data::CrCirData>> i) {
          self->crCirConfigs() = i;
      }
    void setComConfigs(std::vector<ext::shared_ptr<ore::data::CommoditySchwartzData>> i) {
          self->comConfigs() = i;
      }

      void setSalvagingAlgorithm(QuantLib::SalvagingAlgorithm::Type alg) {
          self->getSalvagingAlgorithm() = alg;
      }

      void setCorrelations(ore::data::CorrelationMatrixBuilder& builder) {
          self->setCorrelations(builder.correlations());
      }

      void setCorrelationData(const ore::data::InstantaneousCorrelations& corrs) {
          self->setCorrelations(corrs.correlations());
      }

      ext::shared_ptr<ore::data::InstantaneousCorrelations> correlationData() const {
          return ext::make_shared<ore::data::InstantaneousCorrelations>(self->correlations());
      }

      void setCorrelation(const ore::data::CorrelationFactor& factor1,
                          const ore::data::CorrelationFactor& factor2,
                          QuantLib::Real correlation) {
          ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
          builder.addCorrelation(factor1, factor2, correlation);
          self->setCorrelations(builder.correlations());
      }

      void setCorrelationValue(const std::string& type1, const std::string& name1,
                               QuantLib::Size index1, const std::string& type2,
                               const std::string& name2, QuantLib::Size index2,
                               QuantLib::Real correlation) {
          ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
          builder.addCorrelation(oreplusMakeCorrelationFactor(type1, name1, index1),
                                 oreplusMakeCorrelationFactor(type2, name2, index2), correlation);
          self->setCorrelations(builder.correlations());
      }

      QuantLib::Real correlationValue(const ore::data::CorrelationFactor& factor1,
                                      const ore::data::CorrelationFactor& factor2) const {
          ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
          return builder.getCorrelation(factor1, factor2)->value();
      }

      QuantLib::Real correlationValue(const std::string& type1, const std::string& name1,
                                      QuantLib::Size index1, const std::string& type2,
                                      const std::string& name2, QuantLib::Size index2) const {
                    ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
                    return builder.getCorrelation(oreplusMakeCorrelationFactor(type1, name1, index1),
                                                                                oreplusMakeCorrelationFactor(type2, name2, index2))
                            ->value();
      }
    }

};

} // namespace data
} // namespace ore

#endif
