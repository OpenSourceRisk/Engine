#include <ored/portfolio/forwardvolatilityagreement.hpp>
#include <ored/scripting/utilities.hpp>

namespace ore {
namespace data {

void ForwardVolatilityAgreement::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    clear();
    initIndices();
    events_.emplace_back("FvaDate", fvaDate_);
    events_.emplace_back("OptionExpiry", optionExpiry_);
    events_.emplace_back("PremiumDate", premiumDate_);
    events_.emplace_back("SettlementDate", settlementDate_);
    Position::Type position = parsePositionType(longShort_);
    numbers_.emplace_back("Number", "LongShort", position == Position::Long ? "1" : "-1");
    numbers_.emplace_back("Number", "UnderlyingStrike", underlyingStrikeProvided_ ? underlyingStrike_ : "0.0");
    numbers_.emplace_back("Number", "HasUnderlyingStrike", underlyingStrikeProvided_ ? "1" : "0");
    numbers_.emplace_back("Number", "ImpliedVolStrike", impliedVolStrike_);
    numbers_.emplace_back("Number", "Quantity", quantity_);

    daycounters_.emplace_back("Daycounter", "DayCountFraction", dayCountFraction_.empty() ? "A360" : dayCountFraction_);

    const bool isStraddle = payoffType_ == "Straddle" || payoffType_.empty();
    const bool isPut = payoffType_ == "Put";
    QL_REQUIRE(isStraddle || isPut, "PayoffType (" << payoffType_ << ") must be Straddle or Put");

    if (isPut) {
        QL_REQUIRE(!dayCountFraction_.empty(), "DayCountFraction is required for Put payoff type");
        QL_REQUIRE(!dividendYield_.empty(), "DividendYield is required for Put payoff type");
        QL_REQUIRE(!fixedRate_.empty(), "FixedRate is required for Put payoff type");
    }

    numbers_.emplace_back("Number", "DividendYield", dividendYield_.empty() ? "0.0" : dividendYield_);
    numbers_.emplace_back("Number", "FixedRate", fixedRate_.empty() ? "0.0" : fixedRate_);

    currencies_.emplace_back("Currency", "PayCcy", payCcy_);

    productTag_ = "SingleAssetOption({AssetClass})";

    static const std::string straddleScript =
        "REQUIRE TODAY <= FvaDate;\n"
        "REQUIRE FvaDate <= PremiumDate;\n"
        "REQUIRE FvaDate < OptionExpiry;\n"
        "REQUIRE Quantity >= 0;\n"
        "REQUIRE ImpliedVolStrike > 0;\n"
        "\n"
        "NUMBER forwardStrike, effectiveStrike, fixedPremium, floatingPayoff;\n"
        "NUMBER Tau, D1, D2, P1, P2, S1;\n"
        "\n"
        "forwardStrike = Underlying(FvaDate, OptionExpiry);\n"
        "\n"
        "effectiveStrike = HasUnderlyingStrike * UnderlyingStrike + (1 - HasUnderlyingStrike) * forwardStrike;\n"
        "\n"

        "IF FixedRate == 0 AND DividendYield == 0 THEN\n" // ENSURE varibbles are defined in tghe Straddle, then run the straddle. Confirmed the regular tests pass, then move to replicating the Put one 
        "   fixedPremium = black(1, FvaDate, OptionExpiry, effectiveStrike, forwardStrike, ImpliedVolStrike) +\n"
        "                  black(-1, FvaDate, OptionExpiry, effectiveStrike, forwardStrike, ImpliedVolStrike);\n"
        "ELSE\n" 
        "Tau = dcf(DayCountFraction, FvaDate, OptionExpiry);\n"
        "   D1 = ln(S1 / UnderlyingStrike) +\n"
        "        (FixedRate - DividendYield + 0.5 * ImpliedVolStrike * ImpliedVolStrike) * Tau;\n"
        "   D1 = D1 / (ImpliedVolStrike * sqrt(Tau));\n"
        "   D2 = ln(S1 / UnderlyingStrike) +\n"
        "        (FixedRate - DividendYield - 0.5 * ImpliedVolStrike * ImpliedVolStrike) * Tau;\n"
        "   D2 = D2 / (ImpliedVolStrike * sqrt(Tau));\n"
        "   P1 = normalCdf(-D1) * S1 * exp(-DividendYield * Tau) -\n"
        "        UnderlyingStrike * normalCdf(D2) * exp(-FixedRate * Tau);\n"
        "   P2 = UnderlyingStrike * normalCdf(-D2) * exp(-FixedRate * Tau) -\n"
        "        normalCdf(-D1) * S1 * exp(-DividendYield * Tau);\n"    
        "   fixedPremium = P1 + P2;\n"
        "END;\n"
        "floatingPayoff = abs(effectiveStrike - Underlying(OptionExpiry));\n"
        "\n"
        "FVA = LongShort * Quantity * (PAY(floatingPayoff, OptionExpiry, SettlementDate, PayCcy) -\n"
        "       PAY(fixedPremium, FvaDate, PremiumDate, PayCcy));";

    static const std::string putScript =
        "NUMBER Tau, S1, D1, D2, P1;\n"
        "NUMBER Payoff, OptionLeg, PremiumLeg;\n"
        "\n"
        "REQUIRE TODAY <= FvaDate;\n"
        "REQUIRE FvaDate <= PremiumDate;\n"
        "REQUIRE FvaDate < OptionExpiry;\n"
        "REQUIRE Quantity > 0;\n"
        "REQUIRE UnderlyingStrike > 0;\n"
        "REQUIRE ImpliedVolStrike > 0;\n"
        "\n"
        "Tau = dcf(DayCountFraction, FvaDate, OptionExpiry);\n"
        "\n"
        "S1 = Underlying(FvaDate);\n"
        "\n"
        "D1 = ln(S1 / UnderlyingStrike) + (FixedRate - DividendYield + 0.5 * ImpliedVolStrike * ImpliedVolStrike) * Tau;\n"
        "D1 = D1 / (ImpliedVolStrike * sqrt(Tau));\n"
        "D2 = ln(S1 / UnderlyingStrike) + (FixedRate - DividendYield - 0.5 * ImpliedVolStrike * ImpliedVolStrike) * Tau;\n"
        "D2 = D2 / (ImpliedVolStrike * sqrt(Tau));\n"
        "\n"
        "P1 = (UnderlyingStrike * normalCdf(-D2) - normalCdf(-D1) * S1 * exp((FixedRate - DividendYield) * Tau)) *\n"
        "     exp(-DividendYield * Tau);\n"
        "PremiumLeg = -1 * LongShort * Quantity * LOGPAY(P1, FvaDate, PremiumDate, PayCcy);\n"
        "\n"
        "Payoff = max(UnderlyingStrike - Underlying(OptionExpiry), 0);\n"
        "OptionLeg = LongShort * Quantity * LOGPAY(Payoff, OptionExpiry, SettlementDate, PayCcy);\n"
        "\n"
        "FVA = OptionLeg + PremiumLeg;\n";

    script_ = {{"", ScriptedTradeScriptData(
                        isStraddle ? straddleScript : putScript,
                        "FVA",
                        {{"currentNotional", "Quantity"}, {"notionalCurrency", "PayCcy"}}, {})}};
    ScriptedTrade::build(factory);
}

void ForwardVolatilityAgreement::initIndices() {
    indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_));
}

void ForwardVolatilityAgreement::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(tradeDataNode, "ForwardVolatilityAgreementData node not found");

    fvaDate_ = XMLUtils::getChildValue(tradeDataNode, "FvaDate", true);
    optionExpiry_ = XMLUtils::getChildValue(tradeDataNode, "OptionExpiry", true);
    premiumDate_ = XMLUtils::getChildValue(tradeDataNode, "PremiumDate", true);
    XMLNode* tmp = XMLUtils::getChildNode(tradeDataNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();
    longShort_ = XMLUtils::getChildValue(tradeDataNode, "LongShort", true);
    underlyingStrike_ = XMLUtils::getChildValue(tradeDataNode, "UnderlyingStrike", false);
    underlyingStrikeProvided_ = underlyingStrike_ != "0" && !underlyingStrike_.empty();
    impliedVolStrike_ = XMLUtils::getChildValue(tradeDataNode, "ImpliedVolStrike", true);
    quantity_ = XMLUtils::getChildValue(tradeDataNode, "Quantity", true);
    payCcy_ = XMLUtils::getChildValue(tradeDataNode, "PayCcy", true);
    settlementDate_ = XMLUtils::getChildValue(tradeDataNode, "SettlementDate", true);
    payoffType_ = XMLUtils::getChildValue(tradeDataNode, "PayoffType", false, "Straddle");
    dayCountFraction_ = XMLUtils::getChildValue(tradeDataNode, "DayCountFraction", false);
    dividendYield_ = XMLUtils::getChildValue(tradeDataNode, "DividendYield", false);
    fixedRate_ = XMLUtils::getChildValue(tradeDataNode, "FixedRate", false);
    initIndices();
}

XMLNode* ForwardVolatilityAgreement::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, tradeNode);

    XMLUtils::addChild(doc, tradeNode, "FvaDate", fvaDate_);
    XMLUtils::addChild(doc, tradeNode, "OptionExpiry", optionExpiry_);
    XMLUtils::addChild(doc, tradeNode, "PremiumDate", premiumDate_);
    XMLUtils::appendNode(tradeNode, underlying_->toXML(doc));
    XMLUtils::addChild(doc, tradeNode, "LongShort", longShort_);
    if (underlyingStrikeProvided_)
        XMLUtils::addChild(doc, tradeNode, "UnderlyingStrike", underlyingStrike_);
    XMLUtils::addChild(doc, tradeNode, "ImpliedVolStrike", impliedVolStrike_);
    XMLUtils::addChild(doc, tradeNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, tradeNode, "PayCcy", payCcy_);
    XMLUtils::addChild(doc, tradeNode, "SettlementDate", settlementDate_);
    if (!payoffType_.empty() && payoffType_ != "Straddle")
        XMLUtils::addChild(doc, tradeNode, "PayoffType", payoffType_);
    if (!dayCountFraction_.empty())
        XMLUtils::addChild(doc, tradeNode, "DayCountFraction", dayCountFraction_);
    if (!dividendYield_.empty())
        XMLUtils::addChild(doc, tradeNode, "DividendYield", dividendYield_);
    if (!fixedRate_.empty())
        XMLUtils::addChild(doc, tradeNode, "FixedRate", fixedRate_);

    return node;
}

} // namespace data
} // namespace ore
