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

/*! \file ored/portfolio/builder/fxeuropeanbarrieroption.hpp
   \brief FX European Barrier Option pricing engine builder
   \ingroup portfolio/builders
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/trade.hpp>
#include <ql/shared_ptr.hpp>

namespace ore {
namespace data {

class FxEuropeanBarrierOptionScriptedEngineBuilder : public DelegatingEngineBuilder {
public:
    FxEuropeanBarrierOptionScriptedEngineBuilder()
        : DelegatingEngineBuilder("ScriptedTrade", "ScriptedTrade",
                                  {"FxEuropeanBarrierOption"}) {}
    QuantLib::ext::shared_ptr<ore::data::Trade> build(const Trade* trade,
                                              const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) override;
    std::string effectiveTradeType() const override { return "ScriptedTrade"; }
};
} // namespace data
} // namespace oreplus
