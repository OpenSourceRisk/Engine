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

/*! \file orea_saccr.i
    \brief SWIG bindings for the SA-CCR engine layer:
           SaccrTradeData (enums, Contribution struct, Impl base) and SaccrCalculator.
*/

#ifndef orea_saccr_i
#define orea_saccr_i

%include stl.i
%include std_set.i
%include std_map.i
%include types.i

// NettingSetDetails, NettingSetManager, CollateralBalances, CounterpartyManager
%include ored_portfolio_support.i
// Crif (needed for SaccrCalculator ctor)
%include orea_simm.i
// optional<Real>, optional<bool>, optional<Integer>, optional<std::string>
%include qle_common_typemaps.i

%{
#include <orea/engine/saccrtradedata.hpp>
#include <orea/engine/saccrcalculator.hpp>
// Aliases to allow SWIG to wrap nested classes as top-level module types.
// This block is C++-compiler-only: the typedef makes SaccrContribution
// and SaccrImpl identical to their nested originals at link time, while the
// SWIG-parsed declarations below use the short names for Python visibility.
namespace ore { namespace analytics {
    typedef SaccrTradeData::Contribution SaccrContribution;
    typedef SaccrTradeData::Impl         SaccrImpl;
}}
%}

// ---------------------------------------------------------------------------
// optional<Size> typemap — not in qle_common_typemaps.i
// ---------------------------------------------------------------------------
#if defined(SWIGPYTHON)
%typemap(in) QuantLib::ext::optional<QuantLib::Size> %{
    if ($input == Py_None)
        $1 = QuantLib::ext::nullopt;
    else if (PyLong_Check($input))
        $1 = (QuantLib::Size)PyLong_AsUnsignedLong($input);
    else
        SWIG_exception(SWIG_TypeError, "int expected");
%}
%typecheck (SWIG_TYPECHECK_INTEGER) QuantLib::ext::optional<QuantLib::Size> %{
    $1 = (PyLong_Check($input) || $input == Py_None) ? 1 : 0;
%}
%typemap(out) QuantLib::ext::optional<QuantLib::Size> %{
    $result = !$1 ? Py_None : PyLong_FromSize_t(*$1);
    Py_INCREF($result);
%}
#endif

// ---------------------------------------------------------------------------
// shared_ptr registrations — must appear before class bodies.
// Note: SaccrImpl and SaccrContribution are the top-level aliases (typedef'd
// in the %{...%} block above) for the nested C++ types.
// ---------------------------------------------------------------------------
%shared_ptr(ore::analytics::SaccrTradeData)
%shared_ptr(ore::analytics::SaccrImpl)
%shared_ptr(ore::analytics::SaccrCalculator)

// ---------------------------------------------------------------------------
// Main namespace block
// ---------------------------------------------------------------------------
namespace ore {
namespace analytics {

// ── SaccrContribution ───────────────────────────────────────────────────────
// Wraps ore::analytics::SaccrTradeData::Contribution (typedef'd as
// SaccrContribution in the %{...%} block so C++ sees them as the same type).
struct SaccrContribution {
    SaccrContribution();

    // Plain public fields
    std::string currency;
    QuantLib::Real adjustedNotional;
    QuantLib::Real delta;
    QuantLib::Real maturity;
    QuantLib::Real maturityFactor;
    bool isOption;
    bool isVol;
    std::string bucket;

    // optional fields — typemaps in qle_common_typemaps.i handle None<->nullopt
    QuantLib::ext::optional<QuantLib::Real> supervisoryDuration;
    QuantLib::ext::optional<QuantLib::Real> startDate;
    QuantLib::ext::optional<QuantLib::Real> endDate;
    QuantLib::ext::optional<QuantLib::Real> lastExerciseDate;
    QuantLib::ext::optional<QuantLib::Real> currentPrice;
    QuantLib::ext::optional<QuantLib::Real> optionDeltaPrice;
    QuantLib::ext::optional<QuantLib::Real> strike;
    QuantLib::ext::optional<QuantLib::Size> numNominalFlows;

    // HedgingData and UnderlyingData fields exposed via %extend helpers
    %extend {
        std::string hedgingSet() const {
            return $self->hedgingData.hedgingSet;
        }
        bool isBasis() const {
            return $self->hedgingData.isBasis();
        }
        bool isVolHedging() const {
            return $self->hedgingData.isVol;
        }
        QuantLib::ext::optional<std::string> hedgingSubset() const {
            return $self->hedgingData.hedgingSubset;
        }
        std::string qualifier() const {
            return $self->underlyingData.qualifier;
        }
        // Returns the SA-CCR asset class as an integer (cast of AssetClass enum)
        int saccrAssetClassInt() const {
            return static_cast<int>($self->underlyingData.saccrAssetClass);
        }
        bool isIndex() const {
            return $self->underlyingData.isIndex;
        }
    }
};

// ── SaccrImpl ───────────────────────────────────────────────────────────────
// Wraps ore::analytics::SaccrTradeData::Impl (typedef'd as SaccrImpl).
// Abstract base class — no constructor, accessed only via shared_ptr from
// SaccrTradeData::data().
%nodefaultctor SaccrImpl;
class SaccrImpl {
public:
    virtual std::string name() const;
    const std::vector<ore::analytics::SaccrContribution>& getContributions() const;
    const ore::data::NettingSetDetails& nettingSetDetails() const;
    const std::string& counterparty() const;
    const QuantLib::Real NPV() const;
};

// ── SaccrTradeData ──────────────────────────────────────────────────────────
%nodefaultctor SaccrTradeData;
class SaccrTradeData {
public:

    // Enumerations
    enum AssetClass : char { IR, FX, Credit, Equity, Commodity, None };
    enum class CommodityHedgingSet : char { Energy, Agriculture, Metal, Other };

    // Public API
    void initialise(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);
    const std::set<ore::data::NettingSetDetails>& nettingSets() const;
    const std::map<std::string, QuantLib::ext::shared_ptr<ore::analytics::SaccrImpl>>& data() const;
    QuantLib::Size size() const;
    const std::string& baseCurrency() const;
    const QuantLib::Real NPV(const ore::data::NettingSetDetails& nsd) const;
    const std::string& counterparty(const ore::data::NettingSetDetails& nsd) const;
};

// ── SaccrCalculator ─────────────────────────────────────────────────────────
%nodefaultctor SaccrCalculator;
class SaccrCalculator {
public:
    // Simplified constructor — omits the optional reports map
    %extend {
        SaccrCalculator(const QuantLib::ext::shared_ptr<ore::analytics::Crif>& capitalCrif,
                        const QuantLib::ext::shared_ptr<ore::analytics::SaccrTradeData>& saccrTradeData,
                        const std::string& baseCurrency,
                        const QuantLib::ext::shared_ptr<ore::data::NettingSetManager>& nettingSetManager,
                        const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager,
                        const QuantLib::ext::shared_ptr<ore::data::Market>& market) {
            return new ore::analytics::SaccrCalculator(
                capitalCrif, saccrTradeData, baseCurrency,
                nettingSetManager, counterpartyManager, market);
        }
    }

    // Netting-set enumeration
    const std::vector<ore::data::NettingSetDetails>& nettingSetDetails() const;
    const std::set<ore::analytics::SaccrTradeData::AssetClass>& assetClasses(
        ore::data::NettingSetDetails nettingSetDetails) const;
    const std::vector<std::string>& hedgingSets(
        ore::data::NettingSetDetails nettingSetDetails,
        ore::analytics::SaccrTradeData::AssetClass assetClass) const;

    // Portfolio-level aggregates
    QuantLib::Real NPV() const;
    QuantLib::Real CC() const;

    // Per-netting-set results
    QuantLib::Real EAD(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real EAD(std::string nettingSet) const;
    QuantLib::Real RW(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real RC(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real PFE(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real multiplier(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real addOn(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real NPV(ore::data::NettingSetDetails nettingSetDetails) const;
    QuantLib::Real CC(ore::data::NettingSetDetails nettingSetDetails) const;

    // Per-asset-class and per-hedging-set add-ons
    QuantLib::Real addOn(ore::data::NettingSetDetails nettingSetDetails,
                         ore::analytics::SaccrTradeData::AssetClass assetClass) const;
    QuantLib::Real addOn(ore::data::NettingSetDetails nettingSetDetails,
                         ore::analytics::SaccrTradeData::AssetClass assetClass,
                         std::string hedgingSet) const;
};

} // namespace analytics
} // namespace ore

// ---------------------------------------------------------------------------
// STL container templates — must come after class declarations
// ---------------------------------------------------------------------------
%template(NettingSetDetailsSet)     std::set<ore::data::NettingSetDetails>;
%template(SaccrContributionVector)  std::vector<ore::analytics::SaccrContribution>;
%template(SaccrAssetClassSet)       std::set<ore::analytics::SaccrTradeData::AssetClass>;
%template(SaccrImplMap)             std::map<std::string, QuantLib::ext::shared_ptr<ore::analytics::SaccrImpl>>;

// ---------------------------------------------------------------------------
// Inject Contribution and Impl as attributes of SaccrTradeData in Python so
// users can write ORE.SaccrTradeData.Contribution() and SaccrTradeData.Impl.
// ---------------------------------------------------------------------------
%pythoncode %{
SaccrTradeData.Contribution = SaccrContribution
SaccrTradeData.Impl         = SaccrImpl
%}

#endif
