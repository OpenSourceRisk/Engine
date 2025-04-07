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

%{
using ore::data::CrossAssetModelData;
using ore::data::IrModelData;
using ore::data::FxBsData;
using ore::data::EqBsData;
using ore::data::InflationModelData;
using ore::data::CrLgmData;
using ore::data::CrCirData;
using ore::data::CommoditySchwartzData;
using ore::data::CorrelationKey;
using ore::data::CrossAssetModel;
using ore::data::InstantaneousCorrelations;
using ore::data::parseDiscretization;
using ore::data::CalibrationType;
using ore::data::ParamType;
%}

%shared_ptr(IrModelData)
class IrModelData {
public:
    IrModelData(const std::string& name);
    IrModelData(const std::string& name,const std::string& qualifier, CalibrationType calibrationType);
    const std::string& name();
    const std::string& qualifier();
    virtual std::string ccy() const;       
};

%shared_ptr(FxBsData)
class FxBsData {
public:
    FxBsData();
    FxBsData(std::string foreignCcy, std::string domesticCcy, CalibrationType calibrationType, bool calibrateSigma,
             ParamType sigmaType, const std::vector<QuantLib::Time>& sigmaTimes, const std::vector<QuantLib::Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>());
    const std::string& foreignCcy();
    const std::string& domesticCcy();
};

%shared_ptr(EqBsData)
class EqBsData {
public:
    EqBsData() {}
    EqBsData(std::string name, std::string currency, CalibrationType calibrationType, bool calibrateSigma,
             ParamType sigmaType, const std::vector<Time>& sigmaTimes, const std::vector<Real>& sigmaValues,
             std::vector<std::string> optionExpiries = std::vector<std::string>(),
             std::vector<std::string> optionStrikes = std::vector<std::string>());

    const std::string& eqName() { return name_; }
};

%template(IrModelDataMap) std::map<std::string, ext::shared_ptr<IrModelData>>;
%template(FxBsDataMap) std::map<std::string, ext::shared_ptr<FxBsData>>;
%template(EqBsDataMap) std::map<std::string, ext::shared_ptr<EqBsData>>;
%template(InflationModelDataMap) std::map<std::string, ext::shared_ptr<InflationModelData>>;
%template(CrLgmDataMap) std::map<std::string, ext::shared_ptr<CrLgmData>>;
%template(CrCirDataMap) std::map<std::string, ext::shared_ptr<CrCirData>>;
%template(CommoditySchwartzDataMap) std::map<std::string, ext::shared_ptr<CommoditySchwartzData>>;

%template(IrModelDataVector) std::vector<ext::shared_ptr<IrModelData>>;
%template(FxBsDataVector) std::vector<ext::shared_ptr<FxBsData>>;
%template(EqBsDataVector) std::vector<ext::shared_ptr<EqBsData>>;
%template(InflationModelDataVector) std::vector<ext::shared_ptr<InflationModelData>>;
%template(CrLgmDataVector) std::vector<ext::shared_ptr<CrLgmData>>;
%template(CrCirDataVector) std::vector<ext::shared_ptr<CrCirData>>;
%template(CommoditySchwartzDataVector) std::vector<ext::shared_ptr<CommoditySchwartzData>>;

%shared_ptr(CrossAssetModelData)
class CrossAssetModelData : public XMLSerializable {
public:
    CrossAssetModelData();

    CrossAssetModelData(
        const std::vector<ext::shared_ptr<IrModelData>>& irConfigs,
        const std::vector<ext::shared_ptr<FxBsData>>& fxConfigs,
        const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        QuantLib::Real tolerance = 1e-4,
        const std::string& measure = "LGM",
        const CrossAssetModel::Discretization discretization = CrossAssetModel::Discretization::Exact,
        const QuantLib::SalvagingAlgorithm::Type& salvagingAlgorithm = SalvagingAlgorithm::None);

    CrossAssetModelData(
        const std::vector<ext::shared_ptr<IrModelData>>& irConfigs,
        const std::vector<ext::shared_ptr<FxBsData>>& fxConfigs,
        const std::vector<ext::shared_ptr<EqBsData>>& eqConfigs,
        const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        QuantLib::Real tolerance = 1e-4,
        const std::string& measure = "LGM",
        const CrossAssetModel::Discretization discretization = CrossAssetModel::Discretization::Exact,
        const QuantLib::SalvagingAlgorithm::Type& salvagingAlgorithm = SalvagingAlgorithm::None);

    CrossAssetModelData(
        const std::vector<ext::shared_ptr<IrModelData>>& irConfigs,
        const std::vector<ext::shared_ptr<FxBsData>>& fxConfigs,
        const std::vector<ext::shared_ptr<EqBsData>>& eqConfigs,
        const std::vector<ext::shared_ptr<InflationModelData>>& infConfigs,
        const std::vector<ext::shared_ptr<CrLgmData>>& crLgmConfigs,
        const std::vector<ext::shared_ptr<CrCirData>>& crCirConfigs,
        const std::vector<ext::shared_ptr<CommoditySchwartzData>>& comConfigs,
        const Size numberOfCreditStates,
        const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        QuantLib::Real tolerance = 1e-4,
        const std::string& measure = "LGM",
        const CrossAssetModel::Discretization discretization = CrossAssetModel::Discretization::Exact,
        const QuantLib::SalvagingAlgorithm::Type& salvagingAlgorithm = SalvagingAlgorithm::None);

    void clear();
    void validate();
    const std::string& domesticCurrency() const;
    const std::vector<std::string>& currencies() const;
    const std::vector<std::string>& equities() const;
    const std::vector<std::string>& infIndices() const;
    const std::vector<std::string>& creditNames() const;
    const std::vector<std::string>& commodities() const;
    const std::vector<ext::shared_ptr<IrModelData>>& irConfigs() const;
    const std::vector<ext::shared_ptr<FxBsData>>& fxConfigs() const;
    const std::vector<ext::shared_ptr<EqBsData>>& eqConfigs() const ;
    const std::vector<ext::shared_ptr<InflationModelData>>& infConfigs() const;
    const std::vector<ext::shared_ptr<CrLgmData>>& crLgmConfigs() const;
    const std::vector<ext::shared_ptr<CrCirData>>& crCirConfigs() const;
    const std::vector<ext::shared_ptr<CommoditySchwartzData>>& comConfigs() const;
    Size numberOfCreditStates() const;
    const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations() const;
    QuantLib::Real bootstrapTolerance() const;
    const std::string& measure() const ;
    CrossAssetModel::Discretization discretization() const;   
    QuantLib::SalvagingAlgorithm::Type getSalvagingAlgorithm() const;
    const string& integrationPolicy() const;
    bool piecewiseIntegration() const;

    void setCorrelations(const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& corrs);
    void setCorrelations(const ext::shared_ptr<InstantaneousCorrelations>& corrs);
    void setNumberOfCreditStates(QuantLib::Size n);

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;;
    bool operator==(const CrossAssetModelData& rhs);
    bool operator!=(const CrossAssetModelData& rhs);;

    void buildIrConfigs(std::map<std::string, ext::shared_ptr<IrModelData>>& irMap);
    void buildFxConfigs(std::map<std::string, ext::shared_ptr<FxBsData>>& fxMap);
    void buildEqConfigs(std::map<std::string, ext::shared_ptr<EqBsData>>& eqMap);
    void buildInfConfigs(const std::map<std::string, ext::shared_ptr<InflationModelData>>& mp);
    void buildCrConfigs(std::map<std::string, ext::shared_ptr<CrLgmData>>& crLgmMap,
                        std::map<std::string, ext::shared_ptr<CrCirData>>& crCirMap);
    void buildComConfigs(std::map<std::string, ext::shared_ptr<CommoditySchwartzData>>& comMap);

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

      void setCurrencies(std::vector<std::string> ccys) {
          self->currencies() = ccys;
      }
      
      void setBootstrapTolerance(QuantLib::Real tol) {
          self->bootstrapTolerance() = tol;
      }

      void setMeasure(std::string m) {
          self->measure() = m;
      }

      void setDiscretization(std::string d) {
          self->discretization() = parseDiscretization(d);
      }
      
      void setIrConfigs(std::vector<ext::shared_ptr<IrModelData>> i) {
          self->irConfigs() = i;
      }
      
      void setFxConfigs(std::vector<ext::shared_ptr<FxBsData>> i) {
          self->fxConfigs() = i;
      }

      void setEqConfigs(std::vector<ext::shared_ptr<EqBsData>> i) {
          self->eqConfigs() = i;
      }

      void setInfConfigs(std::vector<ext::shared_ptr<InflationModelData>> i) {
          self->infConfigs() = i;
      }

      void setCrLgmConfigs(std::vector<ext::shared_ptr<CrLgmData>> i) {
          self->crLgmConfigs() = i;
      }
      void setCrCirConfigss(std::vector<ext::shared_ptr<CrCirData>> i) {
          self->crCirConfigs() = i;
      }
      void setComConfigs(std::vector<ext::shared_ptr<CommoditySchwartzData>> i) {
          self->comConfigs() = i;
      }

      void setSalvagingAlgorithm(QuantLib::SalvagingAlgorithm::Type alg) {
          self->getSalvagingAlgorithm() = alg;
      } 
    }

};

#endif