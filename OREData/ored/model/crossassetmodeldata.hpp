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

#include <ql/types.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <ored/marketdata/market.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/eqbsdata.hpp>

using namespace QuantLib;
using std::string;
using std::map;
using std::vector;
using std::pair;

namespace ore {
namespace data {

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
        const vector<boost::shared_ptr<LgmData>>& irConfigs,
        //! Vector of FX model specifications
        const vector<boost::shared_ptr<FxBsData>>& fxConfigs,
        //! Correlation map, key is a pair of factors labeled as IR:EUR, IR:GBP, FX:GBPEUR, FX:USDEUR,
        const map<pair<string, string>, Real>& c,
        //! Bootstrap tolerance used in model calibration
        Real tolerance = 1e-4)
        : irConfigs_(irConfigs), fxConfigs_(fxConfigs), eqConfigs_(std::vector<boost::shared_ptr<EqBsData>>()),
          correlations_(c), bootstrapTolerance_(tolerance) {
        domesticCurrency_ = irConfigs_[0]->ccy();
        currencies_.clear();
        for (Size i = 0; i < irConfigs_.size(); ++i)
            currencies_.push_back(irConfigs_[i]->ccy());
        validate();
    }
    //! Detailed constructor (all asset classes) - TODO: add inflation, credit, commodity
    CrossAssetModelData( //! Vector of IR model specifications
        const std::vector<boost::shared_ptr<LgmData>>& irConfigs,
        //! Vector of FX model specifications
        const std::vector<boost::shared_ptr<FxBsData>>& fxConfigs,
        //! Vector of EQ model specifications
        const std::vector<boost::shared_ptr<EqBsData>>& eqConfigs,
        //! Correlation map, key is a pair of factors labeled as IR:EUR, IR:GBP, FX:GBPEUR, EQ:Apple,
        const std::map<std::pair<std::string, std::string>, Real>& c,
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
    const vector<boost::shared_ptr<LgmData>>& irConfigs() const { return irConfigs_; }
    const vector<boost::shared_ptr<FxBsData>>& fxConfigs() const { return fxConfigs_; }
    const vector<boost::shared_ptr<EqBsData>>& eqConfigs() const { return eqConfigs_; }
    const map<pair<string, string>, Real>& correlations() const { return correlations_; }
    Real bootstrapTolerance() const { return bootstrapTolerance_; }
    //@}

    //! \name Setters
    //@{
    string& domesticCurrency() { return domesticCurrency_; }
    vector<string>& currencies() { return currencies_; }
    vector<string>& equities() { return equities_; }
    vector<boost::shared_ptr<LgmData>>& irConfigs() { return irConfigs_; }
    vector<boost::shared_ptr<FxBsData>>& fxConfigs() { return fxConfigs_; }
    vector<boost::shared_ptr<EqBsData>>& eqConfigs() { return eqConfigs_; }
    map<pair<string, string>, Real>& correlations() { return correlations_; }
    Real& bootstrapTolerance() { return bootstrapTolerance_; }
    //@}

    //! \name Serialisation
    //@{
    //!Populate members from XML
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
    void buildIrConfigs(map<string, boost::shared_ptr<LgmData>>& irMap);
    //! helper to convert FX data, possibly including defaults, into an FX config vector
    void buildFxConfigs(std::map<std::string, boost::shared_ptr<FxBsData>>& fxMap);
    //! helper to convert EQ data, possibly including defaults, into an EQ config vector
    void buildEqConfigs(std::map<std::string, boost::shared_ptr<EqBsData>>& eqMap);

    string domesticCurrency_;
    vector<std::string> currencies_;
    vector<std::string> equities_;
    vector<boost::shared_ptr<LgmData>> irConfigs_;
    vector<boost::shared_ptr<FxBsData>> fxConfigs_;
    vector<boost::shared_ptr<EqBsData>> eqConfigs_;
    map<pair<string, string>, Real> correlations_;
    Real bootstrapTolerance_;
};
}
}
