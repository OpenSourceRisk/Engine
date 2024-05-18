/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/app/initbuilders.hpp
    \brief add builders to factories
*/

#pragma once

#define ORE_REGISTER_LEG_DATA(NAME, CLASS, OVERWRITE)                                                                  \
    ore::data::LegDataFactory::instance().addBuilder(NAME, &ore::data::createLegData<CLASS>, OVERWRITE);

#define ORE_REGISTER_CALIBRATION_INSTRUMENT(NAME, CLASS, OVERWRITE)                                                    \
    ore::data::CalibrationInstrumentFactory::instance().addBuilder(                                                    \
        NAME, &ore::data::createCalibrationInstrument<CLASS>, OVERWRITE);

#define ORE_REGISTER_REFERENCE_DATUM(NAME, CLASS, OVERWRITE)                                                           \
    ore::data::ReferenceDatumFactory::instance().addBuilder(                                                           \
        NAME, &ore::data::createReferenceDatumBuilder<ore::data::ReferenceDatumBuilder<CLASS>>, OVERWRITE);

#define ORE_REGISTER_BOND_BUILDER(NAME, CLASS, OVERWRITE)                                                              \
    ore::data::BondFactory::instance().addBuilder(NAME, QuantLib::ext::make_shared<CLASS>(), OVERWRITE);

#define ORE_REGISTER_TRADE_BUILDER(NAME, CLASS, OVERWRITE)                                                             \
    ore::data::TradeFactory::instance().addBuilder(NAME, QuantLib::ext::make_shared<ore::data::TradeBuilder<CLASS>>(), \
                                                   OVERWRITE);

#define ORE_REGISTER_LEGBUILDER(NAME, CLASS, OVERWRITE)                                                                \
    ore::data::EngineBuilderFactory::instance().addLegBuilder([]() { return QuantLib::ext::make_shared<CLASS>(); },    \
                                                              OVERWRITE);

#define ORE_REGISTER_AMC_ENGINE_BUILDER(CLASS, OVERWRITE)                                                              \
    ore::data::EngineBuilderFactory::instance().addAmcEngineBuilder(                                                   \
        [](const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam,                                            \
           const std::vector<ore::data::Date>& grid) { return QuantLib::ext::make_shared<CLASS>(cam, grid); },         \
        OVERWRITE);

#define ORE_REGISTER_AMCCG_ENGINE_BUILDER(CLASS, OVERWRITE)                                                            \
    ore::data::EngineBuilderFactory::instance().addAmcCgEngineBuilder(                                                 \
        [](const QuantLib::ext::shared_ptr<ore::data::ModelCG>& model, const std::vector<ore::data::Date>& grid) {     \
            return QuantLib::ext::make_shared<CLASS>(model, grid);                                                     \
        },                                                                                                             \
        OVERWRITE);

#define ORE_REGISTER_ENGINE_BUILDER(CLASS, OVERWRITE)                                                                  \
    ore::data::EngineBuilderFactory::instance().addEngineBuilder([]() { return QuantLib::ext::make_shared<CLASS>(); }, \
                                                                 OVERWRITE);

#define ORE_REGISTER_TRS_UNDERLYING_BUILDER(NAME, CLASS, OVERWRITE)                                                    \
    ore::data::TrsUnderlyingBuilderFactory::instance().addBuilder(NAME, QuantLib::ext::make_shared<CLASS>(), OVERWRITE);

#define ORE_REGISTER_COMPUTE_FRAMEWORK_CREATOR(NAME, CLASS, OVERWRITE)                                                 \
    QuantExt::ComputeFrameworkRegistry::instance().add(NAME, &QuantExt::createComputeFrameworkCreator<CLASS>,          \
                                                       OVERWRITE);

namespace ore::data {

void dataBuilders();

} // namespace ore::data
