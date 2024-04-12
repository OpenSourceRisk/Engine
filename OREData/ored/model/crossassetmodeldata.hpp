/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/model/crossassetmodeldata.hpp
    \brief Cross asset model data
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/model/crcirdata.hpp>
#include <ored/model/crlgmdata.hpp>
#include <ored/model/eqbsdata.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/inflation/inflationmodeldata.hpp>
#include <ored/model/commodityschwartzmodeldata.hpp>
#include <ored/model/irmodeldata.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using QuantExt::CrossAssetModel;
using std::map;
using std::pair;
using std::string;
using std::vector;

//! InstantaneousCorrelations
/*! InstantaneousCorrelations is a class to store the correlations required by
    the CrossAssetModelData class
    \ingroup models
*/
class InstantaneousCorrelations : public XMLSerializable {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    InstantaneousCorrelations() {}

    //! Detailed constructor
    InstantaneousCorrelations(const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations)
        : correlations_(correlations) {}
    //@}

    //! \name Serialisation
    //@{
    //! Populate members from XML
    virtual void fromXML(XMLNode* node) override;
    //! Write class members to XML
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Getters
    //@{
    //!
    const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations() const { return correlations_; }
    //@}

    //! Clear all vectors and maps
    void clear() { correlations_.clear(); }

    //! \name Setters
    //@{
    //!
    void correlations(const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& corrs) {
        correlations_ = corrs;
    }
    //@}

    //! \name Operators
    //@{
    bool operator==(const InstantaneousCorrelations& rhs);
    bool operator!=(const InstantaneousCorrelations& rhs);
    //@}

private:
    std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>> correlations_;
};

//! Cross Asset Model Parameters
/*! CrossAssetModelData comprises the specification of how to build and calibrate
  the CrossAssetModel. It contains
  - specifications for each currency IR component (IrLgmData)
  - specifications for each FX component (FxBsData)
  - the correlation specification between all factors of the model
  - a tolerance for bootstrap type calibration methods
  \ingroup models
 */
class CrossAssetModelData : public XMLSerializable {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CrossAssetModelData() : bootstrapTolerance_(0.0), discretization_(CrossAssetModel::Discretization::Exact) {
        correlations_ = QuantLib::ext::make_shared<InstantaneousCorrelations>();
    }

    //! Detailed constructor (IR/FX only)
    CrossAssetModelData( //! Vector of IR model specifications
        const vector<QuantLib::ext::shared_ptr<IrModelData>>& irConfigs,
        //! Vector of FX model specifications
        const vector<QuantLib::ext::shared_ptr<FxBsData>>& fxConfigs,
        //! Correlation map
        const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4,
        //! Choice of probability measure
        const std::string& measure = "LGM",
        //! Choice of discretization
        const CrossAssetModel::Discretization discretization = CrossAssetModel::Discretization::Exact)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(std::vector<QuantLib::ext::shared_ptr<EqBsData>>()),
          bootstrapTolerance_(tolerance), measure_(measure), discretization_(discretization) {
        correlations_ = QuantLib::ext::make_shared<InstantaneousCorrelations>(c);
        domesticCurrency_ = irConfigs_[0]->ccy();
        currencies_.clear();
        for (Size i = 0; i < irConfigs_.size(); ++i)
            currencies_.push_back(irConfigs_[i]->ccy());
        validate();
    }

    //! Detailed constructor (IR/FX/EQ only)
    CrossAssetModelData( //! Vector of IR model specifications
        const std::vector<QuantLib::ext::shared_ptr<IrModelData>>& irConfigs,
        //! Vector of FX model specifications
        const std::vector<QuantLib::ext::shared_ptr<FxBsData>>& fxConfigs,
        //! Vector of EQ model specifications
        const std::vector<QuantLib::ext::shared_ptr<EqBsData>>& eqConfigs,
        //! Correlation map
        const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4,
        //! Choice of probability measure
        const std::string& measure = "LGM",
        //! Choice of discretization
        const CrossAssetModel::Discretization discretization = CrossAssetModel::Discretization::Exact)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(eqConfigs), bootstrapTolerance_(tolerance),
          measure_(measure), discretization_(discretization) {
        correlations_ = QuantLib::ext::make_shared<InstantaneousCorrelations>(c);
        domesticCurrency_ = irConfigs_[0]->ccy();
        currencies_.clear();
        for (Size i = 0; i < irConfigs_.size(); ++i)
            currencies_.push_back(irConfigs_[i]->ccy());
        validate();
    }

    //! Detailed constructor (all asset classes)
    CrossAssetModelData( //! Vector of IR model specifications
        const std::vector<QuantLib::ext::shared_ptr<IrModelData>>& irConfigs,
        //! Vector of FX model specifications
        const std::vector<QuantLib::ext::shared_ptr<FxBsData>>& fxConfigs,
        //! Vector of EQ model specifications
        const std::vector<QuantLib::ext::shared_ptr<EqBsData>>& eqConfigs,
        //! Vector of INF model specifications
        const std::vector<QuantLib::ext::shared_ptr<InflationModelData>>& infConfigs,
        //! Vector of CR LGM model specifications
        const std::vector<QuantLib::ext::shared_ptr<CrLgmData>>& crLgmConfigs,
        //! Vector of CR CIR model specifications
        const std::vector<QuantLib::ext::shared_ptr<CrCirData>>& crCirConfigs,
        //! Vector of COM Schwartz model specifications
        const std::vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>>& comConfigs,
        //! Number of credit states
        const Size numberOfCreditStates,
        //! Correlation map
        const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4,
        //! Choice of probability measure
        const std::string& measure = "LGM",
        //! Choice of discretization
        const CrossAssetModel::Discretization discretization = CrossAssetModel::Discretization::Exact)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(eqConfigs), infConfigs_(infConfigs),
          crLgmConfigs_(crLgmConfigs), crCirConfigs_(crCirConfigs), comConfigs_(comConfigs),
          numberOfCreditStates_(numberOfCreditStates), bootstrapTolerance_(tolerance), measure_(measure),
          discretization_(discretization) {
        correlations_ = QuantLib::ext::make_shared<InstantaneousCorrelations>(c);
        domesticCurrency_ = irConfigs_[0]->ccy();
        currencies_.clear();
        for (Size i = 0; i < irConfigs_.size(); ++i)
            currencies_.push_back(irConfigs_[i]->ccy());
        validate();
    }
    //@}

    //! Clear all vectors and maps
    void clear();

    //! Check consistency of config vectors
    void validate();

    //! \name Inspectors
    //@{
    const string& domesticCurrency() const { return domesticCurrency_; }
    const vector<string>& currencies() const { return currencies_; }
    const vector<string>& equities() const { return equities_; }
    const vector<string>& infIndices() const { return infindices_; }
    const vector<string>& creditNames() const { return creditNames_; }
    const vector<string>& commodities() const { return commodities_; }
    const vector<QuantLib::ext::shared_ptr<IrModelData>>& irConfigs() const { return irConfigs_; }
    const vector<QuantLib::ext::shared_ptr<FxBsData>>& fxConfigs() const { return fxConfigs_; }
    const vector<QuantLib::ext::shared_ptr<EqBsData>>& eqConfigs() const { return eqConfigs_; }
    const vector<QuantLib::ext::shared_ptr<InflationModelData>>& infConfigs() const { return infConfigs_; }
    const vector<QuantLib::ext::shared_ptr<CrLgmData>>& crLgmConfigs() const { return crLgmConfigs_; }
    const vector<QuantLib::ext::shared_ptr<CrCirData>>& crCirConfigs() const { return crCirConfigs_; }
    const vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>>& comConfigs() const { return comConfigs_; }
    Size numberOfCreditStates() const { return numberOfCreditStates_; }
    const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations() const {
        return correlations_->correlations();
    }
    Real bootstrapTolerance() const { return bootstrapTolerance_; }
    const std::string& measure() const { return measure_; }
    CrossAssetModel::Discretization discretization() const { return discretization_; }
    //@}

    //! \name Setters
    //@{
    string& domesticCurrency() { return domesticCurrency_; }
    vector<string>& currencies() { return currencies_; }
    vector<string>& equities() { return equities_; }
    vector<string>& infIndices() { return infindices_; }
    vector<string>& creditNames() { return creditNames_; }
    vector<string>& commodities() { return commodities_; }
    vector<QuantLib::ext::shared_ptr<IrModelData>>& irConfigs() { return irConfigs_; }
    vector<QuantLib::ext::shared_ptr<FxBsData>>& fxConfigs() { return fxConfigs_; }
    vector<QuantLib::ext::shared_ptr<EqBsData>>& eqConfigs() { return eqConfigs_; }
    vector<QuantLib::ext::shared_ptr<InflationModelData>>& infConfigs() { return infConfigs_; }
    vector<QuantLib::ext::shared_ptr<CrLgmData>>& crLgmConfigs() { return crLgmConfigs_; }
    vector<QuantLib::ext::shared_ptr<CrCirData>>& crCirConfigs() { return crCirConfigs_; }
    vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>>& comConfigs() { return comConfigs_; }
    void setCorrelations(const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& corrs) {
        correlations_ = QuantLib::ext::make_shared<InstantaneousCorrelations>(corrs);
    }
    void setCorrelations(const QuantLib::ext::shared_ptr<InstantaneousCorrelations>& corrs) { correlations_ = corrs; }
    Real& bootstrapTolerance() { return bootstrapTolerance_; }
    std::string& measure() { return measure_; }
    CrossAssetModel::Discretization& discretization() { return discretization_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Operators
    //@{
    bool operator==(const CrossAssetModelData& rhs);
    bool operator!=(const CrossAssetModelData& rhs);
    //@}

    //! helper to convert LGM data, possibly including defaults, into an IR config vector
    void buildIrConfigs(map<string, QuantLib::ext::shared_ptr<IrModelData>>& irMap);
    //! helper to convert FX data, possibly including defaults, into an FX config vector
    void buildFxConfigs(std::map<std::string, QuantLib::ext::shared_ptr<FxBsData>>& fxMap);
    //! helper to convert EQ data, possibly including defaults, into an EQ config vector
    void buildEqConfigs(std::map<std::string, QuantLib::ext::shared_ptr<EqBsData>>& eqMap);
    //! helper to convert INF data, possibly including defaults, into an INF config vector
    void buildInfConfigs(const std::map<std::string, QuantLib::ext::shared_ptr<InflationModelData>>& mp);
    //! helper to convert CR LGM data, possibly including defaults, into CR config vectors
    void buildCrConfigs(std::map<std::string, QuantLib::ext::shared_ptr<CrLgmData>>& crLgmMap,
                        std::map<std::string, QuantLib::ext::shared_ptr<CrCirData>>& crCirMap);
    //! helper to convert COM data, possibly including defaulta, into a COM config vector
    void buildComConfigs(std::map<std::string, QuantLib::ext::shared_ptr<CommoditySchwartzData>>& comMap);

private:
    struct HandleComp {
        bool operator()(const Handle<Quote>& x, const Handle<Quote>& y) const {
            return x.currentLink() == y.currentLink();
        }
    };

    string domesticCurrency_;
    vector<std::string> currencies_;
    vector<std::string> equities_;
    vector<std::string> infindices_;
    vector<std::string> creditNames_;
    vector<std::string> commodities_;
    vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs_;
    vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs_;
    vector<QuantLib::ext::shared_ptr<EqBsData>> eqConfigs_;
    vector<QuantLib::ext::shared_ptr<InflationModelData>> infConfigs_;
    vector<QuantLib::ext::shared_ptr<CrLgmData>> crLgmConfigs_;
    vector<QuantLib::ext::shared_ptr<CrCirData>> crCirConfigs_;
    vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>> comConfigs_;
    Size numberOfCreditStates_ = 0;

    QuantLib::ext::shared_ptr<InstantaneousCorrelations> correlations_;
    Real bootstrapTolerance_;
    std::string measure_;
    CrossAssetModel::Discretization discretization_;
};

CrossAssetModel::Discretization parseDiscretization(const string& s);

} // namespace data
} // namespace ore
