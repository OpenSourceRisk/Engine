/*
  Copyright (C) 2024 AcadiaSoft Inc.
  All rights reserved.
*/

/*! \file orepcapital/orea/saccrcrifgenerator.hpp
    \brief Class that generates a SA-CCR CRIF report
 */

#pragma once
//
// #include <orea/engine/sensitivitystream.hpp>
// #include <orea/simm/crif.hpp>
// #include <orea/simm/crifconfiguration.hpp>
// #include <orea/simm/crifrecord.hpp>
// #include <orea/simm/simmnamemapper.hpp>
// #include <ored/configuration/curveconfigurations.hpp>
// #include <ored/portfolio/referencedata.hpp>
 #include <orea/engine/saccrtradedata.hpp>
// #include <orepsimm/orea/crifmarket.hpp>
// #include <orepsimm/orea/crifrecordgenerator.hpp>
// #include <orepsimm/orea/simmtradedata.hpp>
// #include <orepsimm/ored/portfolio/additionalfieldgetter.hpp>
// #include <string>
// #include <tuple>

namespace ore {
namespace data {
class CollateralBalances;
class NettingSetManager;
} // namespace data
} // namespace ore

namespace oreplus {
namespace data {
class CounterpartyManager;
} // namespace data
} // namespace oreplus

namespace ore {
namespace analytics {

class Crif;
struct CrifRecord;
class SaccrTradeData;

//! Class that generates a CRIF report
class SaccrCrifGenerator {
public:
    // SaccrCrifGenerator(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);
    SaccrCrifGenerator(const QuantLib::ext::shared_ptr<SaccrTradeData>& tradeData)
        : tradeData_(tradeData) {}

    QuantLib::ext::shared_ptr<ore::analytics::Crif> generateCrif() const;
    std::vector<ore::analytics::CrifRecord>
    generateTradeCrifRecords(const QuantLib::ext::shared_ptr<SaccrTradeData::Impl>& tradeDataImpl) const;

private:
    QuantLib::ext::shared_ptr<SaccrTradeData> tradeData_;

    //std::string getAssetClassString(const QuantLib::ext::shared_ptr<SaccrTradeData::Impl>& tradeDataImpl) const;
};

} // namespace analytics
} // namespace oreplus
