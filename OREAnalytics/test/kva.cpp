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

#include <boost/test/unit_test.hpp>
#include <test/oreatoplevelfixture.hpp>

#include <orea/aggregation/postprocess.hpp>
#include <orea/cube/cubeinterpretation.hpp>
#include <orea/cube/inmemorycubeopt.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

#include <ored/marketdata/marketimpl.hpp>
#include <ored/portfolio/collateralbalance.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/nettingsetdefinition.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>

#include <qle/termstructures/creditcurve.hpp>

#include <ql/quotes/simplequote.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <iomanip>

using namespace QuantLib;
using namespace ore::data;
using namespace ore::analytics;
using namespace boost::unit_test_framework;

namespace {

struct CreditSpec {
    std::string name;
    Real hazardRate;
    Real recoveryRate;
};

class KvaTestMarket : public MarketImpl {
public:
    KvaTestMarket(const Date& asof, Real flatRate, const std::vector<CreditSpec>& credits) : MarketImpl(false) {
        asof_ = asof;
        DayCounter dc = ActualActual(ActualActual::ISDA);
        Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(asof, flatRate, dc));
        yieldCurves_[std::make_tuple(Market::defaultConfiguration, YieldCurveType::Discount, "EUR")] = yts;
        for (const auto& c : credits) {
            Handle<Quote> rr(QuantLib::ext::make_shared<SimpleQuote>(c.recoveryRate));
            Handle<DefaultProbabilityTermStructure> dts(
                QuantLib::ext::make_shared<FlatHazardRate>(asof, c.hazardRate, dc));
            recoveryRates_[std::make_pair(Market::defaultConfiguration, c.name)] = rr;
            defaultCurves_[std::make_pair(Market::defaultConfiguration, c.name)] =
                Handle<QuantExt::CreditCurve>(QuantLib::ext::make_shared<QuantExt::CreditCurve>(dts, yts, rr));
        }
    }
};

// Only the envelope and the maturity are read on the exposure / KVA path, no pricing takes place
class KvaTestTrade : public Trade {
public:
    KvaTestTrade(const std::string& id, const std::string& counterparty, const std::string& nettingSetId,
                 const Date& maturity)
        : Trade("KvaTestTrade", Envelope(counterparty, nettingSetId)) {
        this->id() = id;
        maturity_ = maturity;
        npvCurrency_ = "EUR";
    }
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override {}
};

struct TradeSpec {
    std::string id;
    std::string counterparty;
    std::string nettingSetId;
    Real up;
    Real down;
    Real t0;
};

struct KvaSetup {
    Date asof;
    std::vector<Date> dates;
    QuantLib::ext::shared_ptr<Market> market;
    QuantLib::ext::shared_ptr<Portfolio> portfolio;
    QuantLib::ext::shared_ptr<NPVCube> cube;
    QuantLib::ext::shared_ptr<NettingSetManager> nettingSetManager;
};

// Two-sample cube: sample 0 carries +up, sample 1 carries -down, both decaying linearly, so that EPE != ENE
// and both are strictly positive on every date. Uncollateralised netting sets, so flipping the view negates
// the exposure exactly and EPE(flipped) == ENE(normal) bit for bit.
KvaSetup makeSetup(const std::vector<TradeSpec>& specs) {
    KvaSetup s;
    s.asof = Date(15, June, 2022);
    Settings::instance().evaluationDate() = s.asof;
    for (Integer i = 1; i <= 12; ++i)
        s.dates.push_back(s.asof + Period(3 * i, Months));
    s.market = QuantLib::ext::make_shared<KvaTestMarket>(
        s.asof, 0.02, std::vector<CreditSpec>{{"BANK", 0.005, 0.40}, {"CPTY1", 0.05, 0.25}, {"CPTY2", 0.10, 0.10}});
    s.portfolio = QuantLib::ext::make_shared<Portfolio>();
    s.nettingSetManager = QuantLib::ext::make_shared<NettingSetManager>();
    Date maturity = s.asof + 5 * Years;
    for (const auto& t : specs) {
        s.portfolio->add(QuantLib::ext::make_shared<KvaTestTrade>(t.id, t.counterparty, t.nettingSetId, maturity));
        if (!s.nettingSetManager->has(t.nettingSetId))
            s.nettingSetManager->add(QuantLib::ext::make_shared<NettingSetDefinition>(t.nettingSetId));
    }
    QuantLib::ext::shared_ptr<NPVCube> cube =
        QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(s.asof, s.portfolio->ids(), s.dates, 2);
    for (const auto& t : specs) {
        cube->setT0(t.t0, t.id);
        for (Size i = 0; i < s.dates.size(); ++i) {
            Real decay = 1.0 - 0.05 * static_cast<Real>(i + 1);
            cube->set(t.up * decay, t.id, s.dates[i], 0);
            cube->set(-t.down * decay, t.id, s.dates[i], 1);
        }
    }
    s.cube = cube;
    return s;
}

// Deliberately asymmetric own / counterparty KVA parameters: the own PD floor binds for BANK (worst case PD
// ~0.09), neither floor binds for the counterparties, and the CVA risk weights differ by a factor 2.5.
QuantLib::ext::shared_ptr<PostProcess> runPostProcess(const KvaSetup& s, bool flipView, bool kva = true,
                                                      bool cvaSensi = false) {
    std::map<std::string, bool> analytics = {{"kva", kva}, {"flipViewXVA", flipView}, {"cvaSensi", cvaSensi}};
    auto cubeInterpretation =
        QuantLib::ext::make_shared<CubeInterpretation>(false, false, false, nullptr, 0, flipView);
    auto scenarioData = QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(s.dates.size(), 2);
    return QuantLib::ext::make_shared<PostProcess>(
        s.portfolio, s.nettingSetManager, QuantLib::ext::make_shared<CollateralBalances>(), s.market,
        Market::defaultConfiguration, s.cube, scenarioData, analytics, "EUR", "None", 1.0, 0.95, "Symmetric",
        "BANK", "", "", nullptr, cubeInterpretation, false,
        std::vector<Period>{6 * Months, 1 * Years, 3 * Years, 5 * Years, 10 * Years}, 0.0001, 0.10, 1.4, 12.5,
        0.012, 0.15, 0.03, 0.02, 0.05);
}

struct KvaResult {
    Real ourCcr;
    Real theirCcr;
    Real ourCva;
    Real theirCva;
};

KvaResult kvaOf(const QuantLib::ext::shared_ptr<PostProcess>& pp, const std::string& nettingSetId) {
    return {pp->nettingSetOurKVACCR(nettingSetId), pp->nettingSetTheirKVACCR(nettingSetId),
            pp->nettingSetOurKVACVA(nettingSetId), pp->nettingSetTheirKVACVA(nettingSetId)};
}

void checkEqual(const KvaResult& a, const KvaResult& b, const std::string& what) {
    BOOST_TEST_CONTEXT(what) {
        BOOST_CHECK_CLOSE(a.ourCcr, b.ourCcr, 1e-10);
        BOOST_CHECK_CLOSE(a.theirCcr, b.theirCcr, 1e-10);
        BOOST_CHECK_CLOSE(a.ourCva, b.ourCva, 1e-10);
        BOOST_CHECK_CLOSE(a.theirCva, b.theirCva, 1e-10);
    }
}

void checkVectorsEqual(const std::vector<Real>& a, const std::vector<Real>& b, const std::string& what) {
    BOOST_TEST_CONTEXT(what) {
        BOOST_REQUIRE(!b.empty());
        BOOST_REQUIRE_EQUAL(a.size(), b.size());
        for (Size i = 0; i < a.size(); ++i)
            BOOST_CHECK_CLOSE(a[i], b[i], 1e-10);
    }
}

const TradeSpec tradeC1a{"T_C1_a", "CPTY1", "NS_C1", 400.0, 160.0, 120.0};
const TradeSpec tradeC1b{"T_C1_b", "CPTY1", "NS_C1", -50.0, -30.0, -10.0};
const TradeSpec tradeC2a{"T_C2_a", "CPTY2", "NS_C2", -250.0, -90.0, -80.0};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(KvaTest)

BOOST_AUTO_TEST_CASE(testFlipViewMirrorsOurAndTheirKva) {
    BOOST_TEST_MESSAGE("Testing that flipViewXVA swaps our and their KVA for a single counterparty");

    KvaSetup s = makeSetup({tradeC1a, tradeC1b});
    KvaResult normal = kvaOf(runPostProcess(s, false), "NS_C1");
    KvaResult flipped = kvaOf(runPostProcess(s, true), "NS_C1");

    BOOST_TEST_MESSAGE(std::setprecision(17)
                       << "normal view NS_C1: OurKVACCR=" << normal.ourCcr << " TheirKVACCR=" << normal.theirCcr
                       << " OurKVACVA=" << normal.ourCva << " TheirKVACVA=" << normal.theirCva);

    BOOST_CHECK(normal.ourCcr > 0.0);
    BOOST_CHECK(normal.theirCcr > 0.0);
    BOOST_CHECK(normal.ourCva > 0.0);
    BOOST_CHECK(normal.theirCva > 0.0);

    BOOST_CHECK_CLOSE(flipped.ourCcr, normal.theirCcr, 1e-10);
    BOOST_CHECK_CLOSE(flipped.theirCcr, normal.ourCcr, 1e-10);
    BOOST_CHECK_CLOSE(flipped.ourCva, normal.theirCva, 1e-10);
    BOOST_CHECK_CLOSE(flipped.theirCva, normal.ourCva, 1e-10);

    // normal view results are not affected by the flip view handling and must stay as they were
    BOOST_CHECK_CLOSE(normal.ourCcr, 11.965905547224787, 1e-6);
    BOOST_CHECK_CLOSE(normal.theirCcr, 2.4404430807946147, 1e-6);
    BOOST_CHECK_CLOSE(normal.ourCva, 3.5171207035532817, 1e-6);
    BOOST_CHECK_CLOSE(normal.theirCva, 0.52254364738505887, 1e-6);
}

BOOST_AUTO_TEST_CASE(testFlipViewKvaIndependentOfOtherNettingSets) {
    BOOST_TEST_MESSAGE("Testing that flipped KVA of a netting set does not depend on other netting sets");

    KvaResult alone = kvaOf(runPostProcess(makeSetup({tradeC2a}), true), "NS_C2");
    KvaResult combined = kvaOf(runPostProcess(makeSetup({tradeC1a, tradeC1b, tradeC2a}), true), "NS_C2");

    checkEqual(combined, alone, "NS_C2 alone vs. together with NS_C1");
}

BOOST_AUTO_TEST_CASE(testFlipViewKvaIndependentOfNettingSetOrder) {
    BOOST_TEST_MESSAGE("Testing that flipped KVA does not depend on the netting set processing order");

    auto specs = [](const std::string& ns1, const std::string& ns2) {
        std::vector<TradeSpec> v{tradeC1a, tradeC1b, tradeC2a};
        v[0].nettingSetId = v[1].nettingSetId = ns1;
        v[2].nettingSetId = ns2;
        return v;
    };
    auto a = runPostProcess(makeSetup(specs("NS_1", "NS_2")), true);
    auto b = runPostProcess(makeSetup(specs("NS_2", "NS_1")), true);

    checkEqual(kvaOf(a, "NS_1"), kvaOf(b, "NS_2"), "CPTY1 processed first vs. second");
    checkEqual(kvaOf(a, "NS_2"), kvaOf(b, "NS_1"), "CPTY2 processed second vs. first");
}

BOOST_AUTO_TEST_CASE(testFlipViewCvaSensitivityUnaffectedByKva) {
    BOOST_TEST_MESSAGE("Testing that flipped CVA sensitivities are the same with and without the KVA analytic");

    KvaSetup s = makeSetup({tradeC1a, tradeC1b, tradeC2a});
    auto withKva = runPostProcess(s, true, true, true);
    auto withoutKva = runPostProcess(s, true, false, true);

    for (const std::string& ns : {"NS_C1", "NS_C2"}) {
        checkVectorsEqual(withKva->netCvaHazardRateSensitivity(ns), withoutKva->netCvaHazardRateSensitivity(ns),
                          ns + " hazard rate sensitivity");
        checkVectorsEqual(withKva->netCvaSpreadSensitivity(ns), withoutKva->netCvaSpreadSensitivity(ns),
                          ns + " spread sensitivity");
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
