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

#include <boost/algorithm/string.hpp>
#include <ored/configuration/cdsvolcurveconfig.hpp>
#include <ored/marketdata/curvespecparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;
using namespace QuantLib;
using std::string;
using std::vector;

namespace ore {
namespace data {

CDSVolatilityCurveConfig::PriceInfo::PriceInfo(string cdsConventionsId, Real runningCoupon,
    ext::optional<IndexFactors> indexFactors, string engineOverride, ext::optional<OneDimSolverConfig> solverConfig,
    ext::optional<QuoteDimension> quoteDimension)
    : cdsConventionsId_(std::move(cdsConventionsId)), runningCoupon_(runningCoupon),
      indexFactors_(std::move(indexFactors)), engineOverride_(std::move(engineOverride)),
      solverConfig_(std::move(solverConfig)), quoteDimension_(std::move(quoteDimension)) {}

const string& CDSVolatilityCurveConfig::PriceInfo::cdsConventionsId() const {
    return cdsConventionsId_;
}

const Real CDSVolatilityCurveConfig::PriceInfo::runningCoupon() const {
    return runningCoupon_;
}

const ext::optional<CDSVolatilityCurveConfig::PriceInfo::IndexFactors>& 
CDSVolatilityCurveConfig::PriceInfo::indexFactors() const {
    return indexFactors_;
}

const string& CDSVolatilityCurveConfig::PriceInfo::engineOverride() const {
    return engineOverride_;
}

const ext::optional<OneDimSolverConfig>& CDSVolatilityCurveConfig::PriceInfo::solverConfig() const {
    return solverConfig_;
}

QuoteDimension CDSVolatilityCurveConfig::PriceInfo::quoteDimension() const {
    if (quoteDimension_)
        return *quoteDimension_;
    else
        return QuoteDimension::BpsPerOutstandingNtl;
}

void CDSVolatilityCurveConfig::PriceInfo::fromXML(XMLNode* node)
{
    XMLUtils::checkNode(node, "PriceInfo");
    cdsConventionsId_ = XMLUtils::getChildValue(node, "CdsConventions", true);
    runningCoupon_ = XMLUtils::getChildValueAsDouble(node, "RunningCoupon", true);
    if (auto n = XMLUtils::getChildNode(node, "IndexFactors")) {
        // Note: MSVC is more permissive here and allows indexFactors_.emplace() but clang rejects it.
        indexFactors_ = IndexFactors{};
        indexFactors_->indexFactor = XMLUtils::getChildValueAsDouble(n, "IndexFactor", true);
        indexFactors_->indexFactorStrike = XMLUtils::getChildValueAsDouble(n, "IndexFactorStrike", true);
        indexFactors_->realisedFep = XMLUtils::getChildValueAsDouble(n, "RealisedFep", true);
    }
    engineOverride_ = XMLUtils::getChildValue(node, "EngineOverride", false);
    if (auto n = XMLUtils::getChildNode(node, "OneDimSolverConfig")) {
        solverConfig_ = OneDimSolverConfig{};
        solverConfig_->fromXML(n);
    }
    if (auto n = XMLUtils::getChildNode(node, "QuoteDimension")) {
        auto quoteDimStr = XMLUtils::getNodeValue(n);
        quoteDimension_ = quoteDimStr == "BpsPerOptionNtl" ? QuoteDimension::BpsPerOptionNtl :
            QuoteDimension::BpsPerOutstandingNtl;
    }
}

XMLNode* CDSVolatilityCurveConfig::PriceInfo::toXML(XMLDocument& doc) const
{
    XMLNode* node = doc.allocNode("PriceInfo");
    XMLUtils::addChild(doc, node, "CdsConventions", cdsConventionsId_);
    XMLUtils::addChild(doc, node, "RunningCoupon", runningCoupon_);
    if (indexFactors_) {
        auto factorsNode = XMLUtils::addChild(doc, node, "IndexFactors");
        XMLUtils::addChild(doc, factorsNode, "IndexFactor", indexFactors_->indexFactor);
        XMLUtils::addChild(doc, factorsNode, "IndexFactorStrike", indexFactors_->indexFactorStrike);
        XMLUtils::addChild(doc, factorsNode, "RealisedFep", indexFactors_->realisedFep);
    }
    if (!engineOverride_.empty())
        XMLUtils::addChild(doc, node, "EngineOverride", engineOverride_);
    if (solverConfig_) {
        XMLNode* solverConfigNode = solverConfig_->toXML(doc);
        XMLUtils::appendNode(node, solverConfigNode);
    }
    if (quoteDimension_) {
        string quoteDimStr = *quoteDimension_ == QuoteDimension::BpsPerOptionNtl ?
            "BpsPerOptionNtl" : "BpsPerOutstandingNtl";
        XMLUtils::addChild(doc, node, "QuoteDimension", quoteDimStr);
    }
    return node;
}

CDSVolatilityCurveConfig::CDSVolatilityCurveConfig()
    : dayCounter_("A365"), calendar_("NullCalendar"), strikeFactor_(1.0) {}

CDSVolatilityCurveConfig::CDSVolatilityCurveConfig(const string& curveId, const string& curveDescription,
                                                   const ext::shared_ptr<VolatilityConfig>& volatilityConfig,
                                                   const string& dayCounter, const string& calendar,
                                                   const string& strikeType, const string& quoteName, Real strikeFactor,
                                                   const vector<Period>& terms,
                                                   const vector<string>& termCurves,
                                                   const vector<Date>& termMaturities,
                                                   ext::optional<PriceInfo> priceInfo)
    : CurveConfig(curveId, curveDescription), volatilityConfig_(volatilityConfig), dayCounter_(dayCounter),
      calendar_(calendar), strikeType_(strikeType), quoteName_(quoteName), strikeFactor_(strikeFactor), terms_(terms),
      termCurves_(termCurves), termMaturities_(termMaturities), priceInfo_(std::move(priceInfo)) {
    validate("CDSVolatilityCurveConfig");
    populateQuotes();
}

const QuantLib::ext::shared_ptr<VolatilityConfig>& CDSVolatilityCurveConfig::volatilityConfig() const {
    return volatilityConfig_;
}

const string& CDSVolatilityCurveConfig::dayCounter() const { return dayCounter_; }

const string& CDSVolatilityCurveConfig::calendar() const { return calendar_; }

const string& CDSVolatilityCurveConfig::strikeType() const { return strikeType_; }

const string& CDSVolatilityCurveConfig::quoteName() const { return quoteName_; }

Real CDSVolatilityCurveConfig::strikeFactor() const { return strikeFactor_; }

const vector<Period>& CDSVolatilityCurveConfig::terms() const { return terms_; }

const vector<string>& CDSVolatilityCurveConfig::termCurves() const { return termCurves_; }

const vector<Date>& CDSVolatilityCurveConfig::termMaturities() const {
    return termMaturities_;
}

const ext::optional<CDSVolatilityCurveConfig::PriceInfo>& CDSVolatilityCurveConfig::priceInfo() const {
    return priceInfo_;
}

void CDSVolatilityCurveConfig::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CDSVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    terms_.clear();
    termCurves_.clear();
    termMaturities_.clear();
    if (auto n = XMLUtils::getChildNode(node, "Terms")) {
        for (auto c : XMLUtils::getChildrenNodes(n, "Term")) {
            terms_.push_back(XMLUtils::getChildValueAsPeriod(c, "Label", true));
            termCurves_.push_back(XMLUtils::getChildValue(c, "Curve", true));
            if (auto m = XMLUtils::getChildNode(c, "Maturity"))
                termMaturities_.push_back(parseDate(XMLUtils::getNodeValue(m)));
        }
    }

    if (auto n = XMLUtils::getChildNode(node, "QuoteName"))
        quoteName_ = XMLUtils::getNodeValue(n);

    if (XMLUtils::getChildNode(node, "Expiries")) {

        // Giving just an Expiries node is allowed to be backwards compatible but is discouraged.
        WLOG("Using an Expiries node only in CDSVolatilityCurveConfig is deprecated. "
             << "A volatility configuration node should be used instead.");

        // Get the expiries
        vector<string> expiries = XMLUtils::getChildrenValuesAsStrings(node, "Expiries", true);
        QL_REQUIRE(expiries.size() > 0, "Need at least one expiry in the Expiries node.");

        // Build the quotes by appending the expiries and terms to the quote stem.
        std::vector<std::string> quotes;
        string stem = quoteStem(MarketDatum::QuoteType::RATE_LNVOL);
        for (const string& exp : expiries) {
            for (auto const& p : terms_) {
                quotes.push_back(stem + exp + "/" + ore::data::to_string(p));
            }
        }

        // If we have at most 1 term specified, we add quotes without term as well
        for (const string& exp : expiries) {
            quotes.push_back(stem + exp);
        }

        // Create the relevant volatilityConfig_ object.
        if (quotes.size() == 1) {
            volatilityConfig_ = QuantLib::ext::make_shared<ConstantVolatilityConfig>(quotes[0]);
        } else {
            volatilityConfig_ = QuantLib::ext::make_shared<VolatilityCurveConfig>(quotes, "Linear", "Flat");
        }

    } else {
        XMLNode* n;
        if ((n = XMLUtils::getChildNode(node, "Constant"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<ConstantVolatilityConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "Curve"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<VolatilityCurveConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "StrikeSurface"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<VolatilityStrikeSurfaceConfig>();
        } else if ((n = XMLUtils::getChildNode(node, "DeltaSurface"))) {
            QL_FAIL("CDSVolatilityCurveConfig does not yet support a DeltaSurface.");
        } else if ((n = XMLUtils::getChildNode(node, "MoneynessSurface"))) {
            QL_FAIL("CDSVolatilityCurveConfig does not yet support a MoneynessSurface.");
        } else if ((n = XMLUtils::getChildNode(node, "ProxySurface"))) {
            volatilityConfig_ = QuantLib::ext::make_shared<CDSProxyVolatilityConfig>();
        } else {
            QL_FAIL("CDSVolatility node expects one child node with name in list: Constant,"
                    << " Curve, StrikeSurface, ProxySurface.");
        }
        volatilityConfig_->fromXML(n);
    }

    if (auto n = XMLUtils::getChildNode(node, "DayCounter"))
        dayCounter_ = XMLUtils::getNodeValue(n);

    if (auto n = XMLUtils::getChildNode(node, "Calendar"))
        calendar_ = XMLUtils::getNodeValue(n);

    if (auto n = XMLUtils::getChildNode(node, "StrikeType"))
        strikeType_ = XMLUtils::getNodeValue(n);

    if (auto n = XMLUtils::getChildNode(node, "StrikeFactor"))
        strikeFactor_ = parseReal(XMLUtils::getNodeValue(n));

    if (auto n = XMLUtils::getChildNode(node, "PriceInfo")) {
        priceInfo_ = PriceInfo{};
        priceInfo_->fromXML(n);
    }

    validate("CDSVolatilityCurveConfig::fromXML");
    populateQuotes();
}

XMLNode* CDSVolatilityCurveConfig::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CDSVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    if (!terms_.empty()) {
        auto termsNode = XMLUtils::addChild(doc, node, "Terms");
        for (Size i = 0; i < terms_.size(); ++i) {
            auto tmp = XMLUtils::addChild(doc, termsNode, "Term");
            XMLUtils::addChild(doc, tmp, "Label", ore::data::to_string(terms_[i]));
            XMLUtils::addChild(doc, tmp, "Curve", ore::data::to_string(termCurves_[i]));
            if (!termMaturities_.empty())
                XMLUtils::addChild(doc, tmp, "Maturity", ore::data::to_string(termMaturities_[i]));
        }
    }

    XMLNode* n = volatilityConfig_->toXML(doc);
    XMLUtils::appendNode(node, n);

    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!strikeType_.empty())
        XMLUtils::addChild(doc, node, "StrikeType", strikeType_);
    if (!quoteName_.empty())
        XMLUtils::addChild(doc, node, "QuoteName", quoteName_);
    XMLUtils::addChild(doc, node, "StrikeFactor", strikeFactor_);

    if (priceInfo_) {
        XMLNode* pxInfoNode = priceInfo_->toXML(doc);
        XMLUtils::appendNode(node, pxInfoNode);
    }

    return node;
}

void CDSVolatilityCurveConfig::populateQuotes() {

    // The quotes depend on the type of volatility structure that has been configured.
    if (auto vc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(volatilityConfig_)) {
        quotes_ = {vc->quote()};
    } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(volatilityConfig_)) {
        quotes_ = vc->quotes();
    } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<VolatilitySurfaceConfig>(volatilityConfig_)) {

        // Clear the quotes_ if necessary and populate with surface quotes
        quotes_.clear();

        string stem = quoteStem(vc->quoteType());
        for (const pair<string, string>& p : vc->quotes()) {
            // build quotes of the form .../TERM/EXPIRY/STRIKE
            for (auto const& t : terms_) {
                quotes_.push_back(stem + ore::data::to_string(t) + "/" + p.first + "/" + p.second);
            }
            // if only one or even no term is configured, also build quotes of the form .../EXPIRY/STRIKE
            // note: we require the term for price quotes.
            if (terms_.size() <= 1 && vc->quoteType() != MarketDatum::QuoteType::PRICE) {
                quotes_.push_back(stem + p.first + "/" + p.second);
            }
        }

    } else if (auto vc = QuantLib::ext::dynamic_pointer_cast<CDSProxyVolatilityConfig>(volatilityConfig_)) {
        // no quotes required in this case
    } else {
        QL_FAIL("CDSVolatilityCurveConfig expected a constant, curve or surface");
    }
}

void CDSVolatilityCurveConfig::populateRequiredIds() const {
    if (auto vc = QuantLib::ext::dynamic_pointer_cast<CDSProxyVolatilityConfig>(volatilityConfig_)) {
        requiredCurveIds_[CurveSpec::CurveType::CDSVolatility].insert(vc->cdsVolatilityCurve());
    }
    for (auto const& c : termCurves_) {
        auto spec = parseCurveSpec(c);
        requiredCurveIds_[CurveSpec::CurveType::Default].insert(spec->curveConfigID());
    }
}

string CDSVolatilityCurveConfig::quoteStem(MarketDatum::QuoteType quoteType) const
{
    string name = !quoteName_.empty() ? quoteName_ : curveID_;
    return "INDEX_CDS_OPTION/" + to_string(quoteType) + "/" + name + "/";
}

void CDSVolatilityCurveConfig::validate(const string& checkSite) const
{
    QL_REQUIRE(terms_.size() == termCurves_.size(), checkSite << ": for curve '" << curveID_ <<
        "', the number of terms (" << terms_.size() << ") should equal the number of term curves (" <<
        termCurves_.size() << ").");

    if (auto vc = ext::dynamic_pointer_cast<QuoteBasedVolatilityConfig>(volatilityConfig_);
        vc && vc->quoteType() == MarketDatum::QuoteType::PRICE)
    {
        QL_REQUIRE(terms_.size() == termMaturities_.size(), checkSite << ": for curve '"
            << curveID_ << "' with quote type PRICE, the number of terms (" << terms_.size() <<") should equal the "
            "number of term maturities (" << termMaturities_.size() << ").");
        QL_REQUIRE(priceInfo_, checkSite << ": for curve '" << curveID_ << "' with quote type PRICE, "
            "a 'PriceInfo' node must be provided.");
    }
}

} // namespace data
} // namespace ore
