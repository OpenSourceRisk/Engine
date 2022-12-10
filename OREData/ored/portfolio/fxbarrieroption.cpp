/*
  Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/fxbarrieroption.hpp>
#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <qle/indexes/fxindex.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxBarrierOption::checkBarriers() {
    QL_REQUIRE(barrier().levels().size() == 1, "Invalid number of barrier levels");
    QL_REQUIRE(barrier().style().empty() || barrier().style() == "American", "Only american barrier style suppported");
}

boost::shared_ptr<QuantLib::PricingEngine>
FxBarrierOption::vanillaPricigingEngine(const boost::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                                        const QuantLib::Date& paymentDate) {   

    if (paymentDate > expiryDate) {
        boost::shared_ptr<EngineBuilder> builder = ef->builder("FxOptionEuropeanCS");
        QL_REQUIRE(builder, "No builder found for FxOptionEuropeanCS");

        boost::shared_ptr<FxEuropeanCSOptionEngineBuilder> fxOptBuilder =
            boost::dynamic_pointer_cast<FxEuropeanCSOptionEngineBuilder>(builder);
        QL_REQUIRE(fxOptBuilder, "No FxEuropeanCSOptionEngineBuilder found");

        return fxOptBuilder->engine(parseCurrency(boughtCurrency_), parseCurrency(soldCurrency_), paymentDate);
    } else {
        boost::shared_ptr<EngineBuilder> builder = ef->builder("FxOption");
        QL_REQUIRE(builder, "No builder found for FxOption");

        boost::shared_ptr<FxEuropeanOptionEngineBuilder> fxOptBuilder =
            boost::dynamic_pointer_cast<FxEuropeanOptionEngineBuilder>(builder);
        QL_REQUIRE(fxOptBuilder, "No FxEuropeanOptionEngineBuilder found");

        return fxOptBuilder->engine(parseCurrency(boughtCurrency_), parseCurrency(soldCurrency_), expiryDate);
    }
}

boost::shared_ptr<QuantLib::PricingEngine>
FxBarrierOption::barrierPricigingEngine(const boost::shared_ptr<EngineFactory>& ef, const QuantLib::Date& expiryDate,
                                        const QuantLib::Date& paymentDate) {

    boost::shared_ptr<EngineBuilder> builder = ef->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);

    boost::shared_ptr<FxBarrierOptionEngineBuilder> fxBarrierOptBuilder =
        boost::dynamic_pointer_cast<FxBarrierOptionEngineBuilder>(builder);
    QL_REQUIRE(fxBarrierOptBuilder, "No FxBarrierOptionEngineBuilder found");
    
    return fxBarrierOptBuilder->engine(parseCurrency(boughtCurrency_), parseCurrency(soldCurrency_), expiryDate, paymentDate);
}

} // namespace data
} // namespace oreplus
