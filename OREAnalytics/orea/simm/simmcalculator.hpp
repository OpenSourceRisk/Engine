/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/simm/simmcalculator.hpp
    \brief Class for calculating SIMM
*/

#pragma once

#include <orea/simm/crif.hpp>
#include <orea/simm/crifloader.hpp>
#include <orea/simm/simmresults.hpp>
#include <ored/marketdata/market.hpp>

#include <map>

namespace ore {
namespace analytics {

//! A class to calculate SIMM given a set of aggregated CRIF results for one or more portfolios.
class SimmCalculator {
public:
    typedef SimmConfiguration::SimmSide SimmSide;

    /*! Construct the SimmCalculator from a container of netted CRIF
        records and a SIMM configuration. The SIMM number is initially calculated in
        USD using the AmountUSD column. It can optionally be converted to a calculation
        currency other than USD by using the \p calculationCcy parameter. If the
        \p calculationCcy is not USD then the \p usdSpot parameter must be used to
        give the FX spot rate between USD and the \p calculationCcy. This spot rate is
        interpreted as the number of USD per unit of \p calculationCcy.
    */
    SimmCalculator(const ore::analytics::Crif& crif,
                   const QuantLib::ext::shared_ptr<SimmConfiguration>& simmConfiguration,
                   const std::string& calculationCcyCall = "USD",
                   const std::string& calculationCcyPost = "USD",
                   const std::string& resultCcy = "",
                   const QuantLib::ext::shared_ptr<ore::data::Market> market = nullptr,
                   const bool determineWinningRegulations = true, const bool enforceIMRegulations = false,
                   const bool quiet = false,
                   const std::map<SimmSide, std::set<NettingSetDetails>>& hasSEC =
                       std::map<SimmSide, std::set<NettingSetDetails>>(),
                   const std::map<SimmSide, std::set<NettingSetDetails>>& hasCFTC =
                       std::map<SimmSide, std::set<NettingSetDetails>>());

    //! Calculates SIMM for a given regulation under a given netting set
    const void calculateRegulationSimm(const ore::analytics::Crif& crif, const ore::data::NettingSetDetails& nsd,
                                       const string& regulation, const SimmSide& side);

    //! Return the winning regulation for each netting set
    const std::string& winningRegulations(const SimmSide& side,
                                          const ore::data::NettingSetDetails& nettingSetDetails) const;
    const std::map<ore::data::NettingSetDetails, string>& winningRegulations(const SimmSide& side) const;
    const std::map<SimmSide, std::map<ore::data::NettingSetDetails, string>>& winningRegulations() const;

    //! Give back the SIMM results container for the given portfolioId and SIMM side
    const SimmResults& simmResults(const SimmSide& side, const ore::data::NettingSetDetails& nettingSetDetails,
                                   const std::string& regulation) const;
    const std::map<std::string, SimmResults>& simmResults(const SimmSide& side, const ore::data::NettingSetDetails& nettingSetDetails) const;

    /*! Give back a map containing the SIMM results containers for every portfolio for the
        given SIMM \p side. The key is the portfolio ID and the value is the SIMM results
        container for that portfolio.
    */
    const std::map<ore::data::NettingSetDetails, std::map<std::string, SimmResults>>& simmResults(const SimmSide& side) const;
    const std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, SimmResults>>>& simmResults() const;

    //! Give back the SIMM results container for the given netting set and SIMM side
    const std::pair<std::string, SimmResults>& finalSimmResults(const SimmSide& side, const ore::data::NettingSetDetails& nettingSetDetails) const;

    /*! Give back a map containing the SIMM results containers for every portfolio for the given
        SIMM \p side. The key is the portfolio ID and the value is a map, with regulation as the
        key, and the value is the SIMM results container for that portfolioId-regulation combination.
    */
    const std::map<ore::data::NettingSetDetails, std::pair<std::string, SimmResults>>& finalSimmResults(const SimmSide& side) const;
    const std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::pair<std::string, SimmResults>>>& finalSimmResults() const;

    const std::map<SimmSide, std::set<std::string>>& finalTradeIds() const { return finalTradeIds_; }

    const Crif& simmParameters() const { return simmParameters_; }

    //! Return the calculator's calculation currency
    const std::string& calculationCurrency(const SimmSide& side) const {
        return side == SimmSide::Call ? calculationCcyCall_ : calculationCcyPost_;
    }

    //! Return the calculator's result currency
    const std::string& resultCurrency() const { return resultCcy_; }

    /*! Populate the finalSimmResults_ and finalAddMargins_ containers
        using the provided map of winning call/post regulations.
    */
    void populateFinalResults(const std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::string>>& winningRegulations);

private:
    //! All the net sensitivities passed in for the calculation
    ore::analytics::Crif crif_;

    //! Net sentivities at the regulation level within each netting set
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, Crif>>> regSensitivities_;

    //! Record of SIMM parameters that were used in the calculation
    ore::analytics::Crif simmParameters_;

    //! The SIMM configuration governing the calculation
    QuantLib::ext::shared_ptr<SimmConfiguration> simmConfiguration_;

    //! The SIMM exposure calculation currency i.e. the currency for which FX delta risk is ignored
    std::string calculationCcyCall_, calculationCcyPost_;

    //! The SIMM result currency i.e. the currency in which the main SIMM results are denominated
    std::string resultCcy_;

    //! Market data for FX rates to use for converting amounts to USD
    QuantLib::ext::shared_ptr<ore::data::Market> market_;

    //! If true, no logging is written out
    bool quiet_;

    std::map<SimmSide, std::set<NettingSetDetails>> hasSEC_, hasCFTC_;

    //! For each netting set, whether all CRIF records' collect regulations are empty
    std::map<ore::data::NettingSetDetails, bool> collectRegsIsEmpty_;

    //! For each netting set, whether all CRIF records' post regulations are empty
    std::map<ore::data::NettingSetDetails, bool> postRegsIsEmpty_;

    //! Regulation with highest initial margin for each given netting set
    //       side,              netting set details,          regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::string>> winningRegulations_;

    /*! Containers, one for call and post, with a map containing a SimmResults object
        for each regulation under each portfolio ID
    */
    //       side,              netting set details,                   regulation     
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, SimmResults>>> simmResults_;

    /*! Containers, one for call and post, with a SimmResults object for each portfolio ID,
        and each margin amount is that of the winning regulation applicable to the portfolio ID
    */
    //       side,              netting set details,                   regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::pair<std::string, SimmResults>>> finalSimmResults_;

    //! Container for keeping track of what trade IDs belong to each regulation
    //       side,              netting set details,                   regulation
    std::map<SimmSide, std::map<ore::data::NettingSetDetails, std::map<std::string, set<string>>>> tradeIds_;

    std::map<SimmSide, set<string>> finalTradeIds_;

    //! Calculate the Interest Rate delta margin component for the given portfolio and product class
    std::pair<std::map<std::string, QuantLib::Real>, bool>
    irDeltaMargin(const ore::data::NettingSetDetails& nettingSetDetails, const CrifRecord::ProductClass& pc,
                  const ore::analytics::Crif& netRecords, const SimmSide& side) const;

    //! Calculate the Interest Rate vega margin component for the given portfolio and product class
    std::pair<std::map<std::string, QuantLib::Real>, bool>
    irVegaMargin(const ore::data::NettingSetDetails& nettingSetDetails, const CrifRecord::ProductClass& pc,
                 const ore::analytics::Crif& netRecords, const SimmSide& side) const;

    //! Calculate the Interest Rate curvature margin component for the given portfolio and product class
    std::pair<std::map<std::string, QuantLib::Real>, bool>
    irCurvatureMargin(const ore::data::NettingSetDetails& nettingSetDetails, const CrifRecord::ProductClass& pc,
                      const SimmSide& side, const ore::analytics::Crif& crif) const;

    /*! Calculate the (delta or vega) margin component for the given portfolio, product class and risk type
        Used to calculate delta or vega or base correlation margin for all risk types except IR, IRVol
        (and by association, Inflation, XccyBasis and InflationVol)
    */
    std::pair<std::map<std::string, QuantLib::Real>, bool> margin(const ore::data::NettingSetDetails& nettingSetDetails,
                                                                  const CrifRecord::ProductClass& pc,
                                                                  const CrifRecord::RiskType& rt,
                                                                  const ore::analytics::Crif& netRecords,
                                                                  const SimmSide& side) const;

    /*! Calculate the curvature margin component for the given portfolio, product class and risk type
        Used to calculate curvature margin for all risk types except IR
    */
    std::pair<std::map<std::string, QuantLib::Real>, bool>
    curvatureMargin(const ore::data::NettingSetDetails& nettingSetDetails, const CrifRecord::ProductClass& pc,
                    const CrifRecord::RiskType& rt, const SimmSide& side, const ore::analytics::Crif& netRecords,
                    bool rfLabels = true) const;

    //! Calculate the additional initial margin for the portfolio ID and regulation
    void calcAddMargin(const SimmSide& side, const ore::data::NettingSetDetails& nsd, const string& regulation, const ore::analytics::Crif& netRecords);

    /*! Populate the results structure with the higher level results after the IMs have been
        calculated at the (product class, risk class, margin type) level for the given
        regulation under the given portfolio
    */
    void populateResults(const SimmSide& side, const ore::data::NettingSetDetails& nsd, const string& regulation);

    /*! Populate final (i.e. winning regulators') using own list of winning regulators, which were determined
        solely by the SIMM results (i.e. not including any external IMSchedule results)
    */
    void populateFinalResults();

    /*! Add a margin result to either call or post results container depending on the
        \p side parameter.

        \remark all additions to the results containers should happen in this method
    */
    void add(const ore::data::NettingSetDetails& nettingSetDetails, const string& regulation,
             const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
             const SimmConfiguration::MarginType& mt, const std::string& b, QuantLib::Real margin, SimmSide side,
             const bool overwrite = true);

    void add(const ore::data::NettingSetDetails& nettingSetDetails, const string& regulation,
             const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
             const SimmConfiguration::MarginType& mt, const std::map<std::string, QuantLib::Real>& margins, SimmSide side,
             const bool overwrite = true);

    //! Add CRIF record to the CRIF records container that correspondsd to the given regulation/s and portfolio ID
    void splitCrifByRegulationsAndPortfolios(const Crif& crif, const bool enforceIMRegulations);

    //! Give the \f$\lambda\f$ used in the curvature margin calculation
    QuantLib::Real lambda(QuantLib::Real theta) const;

    std::set<std::string> getQualifiers(const Crif& crif, const ore::data::NettingSetDetails& nettingSetDetails,
                                        const CrifRecord::ProductClass& pc,
                                        const std::vector<CrifRecord::RiskType>& riskTypes) const;

};

} // namespace analytics
} // namespace ore
