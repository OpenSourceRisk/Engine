#pragma once

#include <ored/portfolio/builders/cachingenginebuilder.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>

namespace ore::data{
//! Engine builder for Commodity Spread Options
/*! Pricing engines are cached by currency
-
\ingroup builders
*/

class CommoditySpreadOptionEngineBuilder : public CachingPricingEngineBuilder<std::string, const Currency&>{
public:

};
}