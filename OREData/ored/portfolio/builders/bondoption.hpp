/*
Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file portfolio/builders/bondoption.hpp
\brief Engine builder for bond option
\ingroup builders
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/blackbondoptionengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

namespace ore {
    namespace data {

        //! Engine builder for bond option
        /*! Pricing engines are cached by currency, creditCurve, securityId and referenceCurve
        -
        \ingroup builders
        */
        class BondOptionEngineBuilder : public CachingPricingEngineBuilder<string, const Currency&, const string&, const string&, const string&, const bool&> {
        public:
            BondOptionEngineBuilder()
                : CachingEngineBuilder("Black", "BlackBondOptionEngine", { "BondOption" }) {}

        protected:
            virtual std::string keyImpl(const Currency& ccy, const string& creditCurveId, const string& securityId,
                const string& referenceCurveId, const bool& bondInPrice) {
                return ccy.code() + "_" + creditCurveId + "_" + securityId + "_" + referenceCurveId + "_" + "BondOption";
            }

            virtual boost::shared_ptr<QuantLib::PricingEngine> engineImpl(const Currency& ccy, const string& creditCurveId, const string& securityId,
                const string& referenceCurveId, const bool& bondInPrice) {

                Handle<YieldTermStructure> discountCurve =
                    market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));
                Handle<QuantLib::SwaptionVolatilityStructure> yieldVola;

                try {
                    yieldVola = market_->yieldVol(securityId, configuration(MarketContext::pricing));
                }
                catch (std::exception) {
                    yieldVola = market_->swaptionVol(ccy.code(), configuration(MarketContext::pricing));
                }
                    
                return boost::make_shared<QuantExt::BlackBondOptionEngine>(yieldVola, discountCurve, bondInPrice);
            };
        };
    }
}
