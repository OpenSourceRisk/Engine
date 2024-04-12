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

/*! \file scenario/crossassetmodelscenariogenerator.hpp
    \brief Scenario generation using cross asset model paths
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/utilities/dategrid.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/cirppimplieddefaulttermstructure.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetmodelimpliedeqvoltermstructure.hpp>
#include <qle/models/crossassetmodelimpliedfxvoltermstructure.hpp>
#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>
#include <qle/models/dkimpliedzeroinflationtermstructure.hpp>
#include <qle/models/jyimpliedyoyinflationtermstructure.hpp>
#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>
#include <qle/models/lgmimplieddefaulttermstructure.hpp>
#include <qle/models/modelimpliedyieldtermstructure.hpp>
#include <qle/models/modelimpliedpricetermstructure.hpp>

namespace ore {
namespace analytics {
using namespace data;
using namespace QuantLib;
using namespace QuantExt;

//! Scenario Generator using cross asset model paths
/*!
  The generator expects
  - a calibrated model,
  - an associated multi path generator (i.e. providing paths for all factors
    of the model ordered as described in the model),
  - a scenario factory that provides building scenario class instances,
  - the configuration of market curves to be simulated
  - a simulation date grid that starts in the future, i.e. does not include today's date
  - the associated time grid including t=0

  \ingroup scenario
 */
class CrossAssetModelScenarioGenerator : public ScenarioPathGenerator {
public:
    //! Constructor
    CrossAssetModelScenarioGenerator(QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model,
                                     QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> multiPathGenerator,
                                     QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                                     QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig,
                                     QuantLib::Date today, QuantLib::ext::shared_ptr<DateGrid> grid,
                                     QuantLib::ext::shared_ptr<ore::data::Market> initMarket,
                                     const std::string& configuration = Market::defaultConfiguration);
    //! Default destructor
    ~CrossAssetModelScenarioGenerator(){};
    std::vector<QuantLib::ext::shared_ptr<Scenario>> nextPath() override;
    void reset() override { pathGenerator_->reset(); }

private:
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator_;
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;
    QuantLib::ext::shared_ptr<ore::data::Market> initMarket_;
    const std::string configuration_;
    // generated data
    std::vector<RiskFactorKey> discountCurveKeys_, indexCurveKeys_, yieldCurveKeys_, zeroInflationKeys_,
        yoyInflationKeys_, defaultCurveKeys_, commodityCurveKeys_;
    std::vector<RiskFactorKey> fxKeys_, eqKeys_, cpiKeys_;
    std::vector<RiskFactorKey> crStateKeys_, survivalWeightKeys_, recoveryRateKeys_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CrossAssetModelImpliedFxVolTermStructure>> fxVols_;
    std::vector<QuantLib::ext::shared_ptr<QuantExt::CrossAssetModelImpliedEqVolTermStructure>> eqVols_;
    std::vector<std::vector<Period>> ten_dsc_, ten_idx_, ten_yc_, ten_efc_, ten_zinf_, ten_yinf_, ten_dfc_, ten_com_;
    std::vector<bool> modelCcyRelevant_;
    Size n_ccy_, n_eq_, n_inf_, n_cr_, n_indices_, n_curves_, n_com_, n_crstates_, n_survivalweights_;

    vector<QuantLib::ext::shared_ptr<QuantExt::ModelImpliedYieldTermStructure>> curves_, fwdCurves_, yieldCurves_;
    vector<QuantLib::ext::shared_ptr<QuantExt::ModelImpliedPriceTermStructure>> comCurves_;
    vector<QuantLib::ext::shared_ptr<IborIndex>> indices_;
    vector<Currency> yieldCurveCurrency_;
    vector<string> zeroInflationIndex_, yoyInflationIndex_;
    vector<tuple<Size, Size, CrossAssetModel::ModelType, QuantLib::ext::shared_ptr<ZeroInflationModelTermStructure>>>
        zeroInfCurves_;
    vector<tuple<Size, Size, CrossAssetModel::ModelType, QuantLib::ext::shared_ptr<YoYInflationModelTermStructure>>>
        yoyInfCurves_;
    vector<QuantLib::ext::shared_ptr<QuantExt::LgmImpliedDefaultTermStructure>> lgmDefaultCurves_;
    vector<QuantLib::ext::shared_ptr<QuantExt::CirppImpliedDefaultTermStructure>> cirppDefaultCurves_;
    vector<QuantLib::ext::shared_ptr<QuantExt::CreditCurve>> survivalWeightsDefaultCurves_;
};

} // namespace analytics
} // namespace ore
