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

#include <algorithm>
#include <ored/marketdata/yieldvolcurve.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

YieldVolCurve::YieldVolCurve(Date asof, YieldVolatilityCurveSpec spec, const Loader& loader,
                             const CurveConfigurations& curveConfigs)
    : GenericYieldVolCurve(
          asof, loader, curveConfigs.yieldVolCurveConfig(spec.curveConfigID()), {},
          [](const boost::shared_ptr<MarketDatum>& md, Period& expiry, Period& term) -> bool {
              boost::shared_ptr<BondOptionQuote> q = boost::dynamic_pointer_cast<BondOptionQuote>(md);
              if (q == nullptr)
                  return false;
              expiry = q->expiry();
              term = q->term();
              return q->instrumentType() == MarketDatum::InstrumentType::BOND_OPTION;
          },
          [](const boost::shared_ptr<MarketDatum>& md, Period& expiry, Period& term, Real& strike) {
              boost::shared_ptr<SwaptionQuote> q = boost::dynamic_pointer_cast<SwaptionQuote>(md);
              return false;
          },
          [](const boost::shared_ptr<MarketDatum>& md, Period& term) {
              boost::shared_ptr<SwaptionShiftQuote> q = boost::dynamic_pointer_cast<SwaptionShiftQuote>(md);
              if (q == nullptr)
                  return false;
              term = q->term();
              return true;
          }),
      spec_(spec) {}

} // namespace data
} // namespace ore
