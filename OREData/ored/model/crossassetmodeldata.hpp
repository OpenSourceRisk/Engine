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
#include <ored/model/eqbsdata.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/infdkdata.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using std::map;
using std::pair;
using std::string;
using std::vector;

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
    CrossAssetModelData() {}
    //! Detailed constructor (IR/FX only)
    CrossAssetModelData( //! Vector of IR model specifications
        const vector<boost::shared_ptr<IrLgmData>>& irConfigs,
        //! Vector of FX model specifications
        const vector<boost::shared_ptr<FxBsData>>& fxConfigs,
        //! Correlation map, key is a pair of factors labeled as IR:EUR, IR:GBP, FX:GBPEUR, FX:USDEUR,
        const map<pair<string, string>, Handle<Quote>>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(std::vector<boost::shared_ptr<EqBsData>>()),
          infConfigs_(std::vector<boost::shared_ptr<InfDkData>>()), correlations_(c), bootstrapTolerance_(tolerance) {
        domesticCurrency_ = irConfigs_[0]->ccy();
        currencies_.clear();
        for (Size i = 0; i < irConfigs_.size(); ++i)
            currencies_.push_back(irConfigs_[i]->ccy());
        validate();
    }
    //! Detailed constructor (IR/FX/EQ only)
    CrossAssetModelData( //! Vector of IR model specifications
        const std::vector<boost::shared_ptr<IrLgmData>>& irConfigs,
        //! Vector of FX model specifications
        const std::vector<boost::shared_ptr<FxBsData>>& fxConfigs,
        //! Vector of EQ model specifications
        const std::vector<boost::shared_ptr<EqBsData>>& eqConfigs,
        //! Correlation map, key is a pair of factors labeled as IR:EUR, IR:GBP, FX:GBPEUR, EQ:Apple,
        const std::map<std::pair<std::string, std::string>, Handle<Quote>>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(eqConfigs), correlations_(c),
          bootstrapTolerance_(tolerance) {
        domesticCurrency_ = irConfigs_[0]->ccy();
        currencies_.clear();
        for (Size i = 0; i < irConfigs_.size(); ++i)
            currencies_.push_back(irConfigs_[i]->ccy());
        validate();
    }
    //! Detailed constructor (all asset classes) - TODO: add inflation, credit, commodity
    CrossAssetModelData( //! Vector of IR model specifications
        const std::vector<boost::shared_ptr<IrLgmData>>& irConfigs,
        //! Vector of FX model specifications
        const std::vector<boost::shared_ptr<FxBsData>>& fxConfigs,
        //! Vector of EQ model specifications
        const std::vector<boost::shared_ptr<EqBsData>>& eqConfigs,
        //! Vector of INF model specifications
        const std::vector<boost::shared_ptr<InfDkData>>& infConfigs,
        //! Correlation map, key is a pair of factors labeled as IR:EUR, IR:GBP, FX:GBPEUR, EQ:Apple,
        const std::map<std::pair<std::string, std::string>, Handle<Quote>>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(eqConfigs), infConfigs_(infConfigs),
          correlations_(c), bootstrapTolerance_(tolerance) {
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
    const vector<boost::shared_ptr<IrLgmData>>& irConfigs() const { return irConfigs_; }
    const vector<boost::shared_ptr<FxBsData>>& fxConfigs() const { return fxConfigs_; }
    const vector<boost::shared_ptr<EqBsData>>& eqConfigs() const { return eqConfigs_; }
    const vector<boost::shared_ptr<InfDkData>>& infConfigs() const { return infConfigs_; }
    const map<pair<string, string>, Handle<Quote>>& correlations() const { return correlations_; }
    Real bootstrapTolerance() const { return bootstrapTolerance_; }
    //@}

    //! \name Setters
    //@{
    string& domesticCurrency() { return domesticCurrency_; }
    vector<string>& currencies() { return currencies_; }
    vector<string>& equities() { return equities_; }
    vector<string>& infIndices() { return infindices_; }
    vector<boost::shared_ptr<IrLgmData>>& irConfigs() { return irConfigs_; }
    vector<boost::shared_ptr<FxBsData>>& fxConfigs() { return fxConfigs_; }
    vector<boost::shared_ptr<EqBsData>>& eqConfigs() { return eqConfigs_; }
    vector<boost::shared_ptr<InfDkData>>& infConfigs() { return infConfigs_; }
    map<pair<string, string>, Handle<Quote>>& correlations() { return correlations_; }
    Real& bootstrapTolerance() { return bootstrapTolerance_; }
    //@}

    //! \name Serialisation
    //@{
    //! Populate members from XML
    virtual void fromXML(XMLNode* node);
    //! Write class mambers to XML
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Operators
    //@{
    bool operator==(const CrossAssetModelData& rhs);
    bool operator!=(const CrossAssetModelData& rhs);
    //@}

private:
    //! helper to convert LGM data, possibly including defaults, into an IR config vector
    void buildIrConfigs(map<string, boost::shared_ptr<IrLgmData>>& irMap);
    //! helper to convert FX data, possibly including defaults, into an FX config vector
    void buildFxConfigs(std::map<std::string, boost::shared_ptr<FxBsData>>& fxMap);
    //! helper to convert EQ data, possibly including defaults, into an EQ config vector
    void buildEqConfigs(std::map<std::string, boost::shared_ptr<EqBsData>>& eqMap);
    //! helper to convert INF data, possibly including defaults, into an EQ config vector
    void buildInfConfigs(std::map<std::string, boost::shared_ptr<InfDkData>>& infMap);

    //
    struct HandleComp {
        bool operator()(const Handle<Quote>& x, const Handle<Quote>& y) const {
            return x.currentLink() == y.currentLink();
        }
    };

    string domesticCurrency_;
    vector<std::string> currencies_;
    vector<std::string> equities_;
    vector<std::string> infindices_;
    vector<boost::shared_ptr<IrLgmData>> irConfigs_;
    vector<boost::shared_ptr<FxBsData>> fxConfigs_;
    vector<boost::shared_ptr<EqBsData>> eqConfigs_;
    vector<boost::shared_ptr<InfDkData>> infConfigs_;
    map<pair<string, string>, Handle<Quote>> correlations_;
    Real bootstrapTolerance_;
};
} // namespace data
} // namespace ore
