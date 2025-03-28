/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file orea/simm/crifgenerator.hpp
    \brief Class that generates a CRIF report
    \ingroup engine
*/

#pragma once

#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <orea/simm/simmnamemapper.hpp>
#include <orea/simm/crifrecord.hpp>
#include <orea/simm/crif.hpp>
#include <orea/simm/crifmarket.hpp>
#include <orea/simm/simmtradedata.hpp>
#include <orea/simm/crifrecordgenerator.hpp>

#include <orea/engine/sensitivitystream.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>

#include <ored/portfolio/referencedata.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/report/report.hpp>
#include <ored/portfolio/additionalfieldgetter.hpp>

#include <ql/types.hpp>

#include <string>
#include <tuple>

namespace ore {
namespace analytics {

//! Class that generates a CRIF report
class CrifGenerator {
public:
    /*! Class constructor
        The \p nameMapper is a mapping from external names to ISDA SIMM qualifiers

        The \p crifMarket is needed when generating CRIF entries for interest rate and credit vega.

        The \p currency argument denotes the currency of the sensitivities that
        will be fed to the CRIF generator and if this is different from USD, the
        \p usdSpot argument is the rate that converts the sensitivity amounts to
        USD i.e. the number of units of USD per unit of sensitivity currency.

        The \p xccyDiscounting parameter is `true` if we are treating all non-base currency discount factor risks as
        emanating from cross currency basis. The \p xccyDiscounting parameter is `false` when we only wish to add
        cross currency basis risk against cross currency interest rate swap instruments.
    */
    CrifGenerator(const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration,
                  const QuantLib::ext::shared_ptr<SimmNameMapper>& nameMapper, const SimmTradeData& tradeData,
                  const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket, bool xccyDiscounting = false,
                  const std::string& currency = "USD", QuantLib::Real usdSpot = 1.0,
                  const QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter>& fieldGetter = nullptr,
                  const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                  const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
                  const std::string& discountIndex = "");

    /*! Generate a CRIF from a sensitivity record stream

        \warning An exception is thrown if any SensitivityRecords from the stream
                 have a currency that differs from the <code>currency</code> provided
                 in the CrifGenerator constructor.

        \warning An exception is thrown if the FX spot sensitivities are not of the form
                 `FXSpot/CCY_1CCY_2/0/spot` where CCY_2 is the sensitivity currency
    */
    QuantLib::ext::shared_ptr<ore::analytics::Crif>
    generateCrif(const QuantLib::ext::shared_ptr<ore::analytics::SensitivityStream>& ss);

    // Process the sensistream and add crif recrods to the the crif
    std::vector<ore::analytics::CrifRecord> processSensitivityStream(ore::analytics::SensitivityStream& ss, std::set<std::string>& failedTrades);

    // void addExtendedAttributes(ore::analytics::CrifRecord& record) const;

    //! Return the base currency's discount index name. May be empty if not populated.
    const std::string& discountIndex() const { return discountIndex_; }

    //! Check if at least one trade in the portfolio uses netting set details, and not just netting set ID.
    const bool& hasNettingSetDetails() const { return hasNettingSetDetails_; }

private:

    QuantLib::ext::shared_ptr<SimmConfiguration> simmConfiguration_;
    QuantLib::ext::shared_ptr<SimmNameMapper> nameMapper_;
    SimmTradeData tradeData_;
    QuantLib::ext::shared_ptr<CrifMarket> crifMarket_;
    bool xccyDiscounting_;
    std::string currency_;
    QuantLib::ext::shared_ptr<ore::data::PortfolioFieldGetter> fieldGetter_;
    QuantLib::Real usdSpot_;;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    std::string discountIndex_;
    QuantLib::ext::shared_ptr<SimmRecordGenerator> simmRecord_;
    /*! This is populated at the start of each call to \c generateCrif with all of the trade IDs. When a CRIF record
        for a trade ID is written, the trade ID is removed from this set. At the end of \c generateCrif, the trade
        IDs that have had no CRIF records written are logged.
    */
    std::set<std::string> allTradeIds_;

    //! Whether netting set details are being used by a trade in the portfolio.
    bool hasNettingSetDetails_;

    //! Creates a crif record for special cases like zeroFxRisk or UseCounterpartyTrade
    ore::analytics::CrifRecord createZeroAmountCrifRecord(const string& tradeId, const ore::analytics::CrifRecord::RiskType riskType, const CrifRecord::IMModel& imModel,
							  const std::string& qualifer, bool inlcudeEndDateIfAvailable) const;

    //! Write a "zero" Risk_FX CRIF record to the report
    ore::analytics::CrifRecord createZeroRiskFxRecord(const std::string& tradeId) const;

};

} // namespace analytics
} // namespace ore
