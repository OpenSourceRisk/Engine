/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;
using namespace std;

namespace {

string invertFx(const string& ccyPair) {
    QL_REQUIRE(ccyPair.size() == 6, "invertFx: Expected currency pair to be 6 characters but got: " << ccyPair);
    return ccyPair.substr(3, 3) + ccyPair.substr(0, 3);
}

}

namespace ore {
namespace data {

using QuantExt::CrossAssetModel;

bool operator<(const CorrelationFactor& lhs, const CorrelationFactor& rhs) {
    return tie(lhs.type, lhs.name, lhs.index) < tie(rhs.type, rhs.name, rhs.index);
}

bool operator==(const CorrelationFactor& lhs, const CorrelationFactor& rhs) {
    return lhs.type == rhs.type && lhs.name == rhs.name && lhs.index == rhs.index;
}

bool operator!=(const CorrelationFactor& lhs, const CorrelationFactor& rhs) {
    return !(lhs == rhs);
}

ostream& operator<<(ostream& out, const CorrelationFactor& f) {
    return out << f.type << ":" << f.name << ":" << f.index;
}

CorrelationFactor parseCorrelationFactor(const string& name, const char separator) {

    std::string sep(1, separator);
    vector<string> tokens;
    boost::split(tokens, name, boost::is_any_of(sep));

    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3,
               "parseCorrelationFactor(" << name << "): expected 2 or 3 tokens separated by separator ('" << sep
                                         << "'), e.g. 'IR" << sep << "USD' or 'INF" << sep << "UKRPI" << sep << "0'");

    return {parseCamAssetType(tokens[0]), tokens[1],
            static_cast<Size>(tokens.size() == 3 ? parseInteger(tokens[3]) : 0)};
}

void CorrelationMatrixBuilder::reset() {
    corrs_.clear();
}

void CorrelationMatrixBuilder::addCorrelation(const string& factor1, const string& factor2, Real correlation) {
    CorrelationFactor f_1 = parseCorrelationFactor(factor1);
    CorrelationFactor f_2 = parseCorrelationFactor(factor2);
    addCorrelation(f_1, f_2, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(correlation)));
}

void CorrelationMatrixBuilder::addCorrelation(const string& factor1, const string& factor2,
    const Handle<Quote>& correlation) {
    CorrelationFactor f_1 = parseCorrelationFactor(factor1);
    CorrelationFactor f_2 = parseCorrelationFactor(factor2);
    addCorrelation(f_1, f_2, correlation);
}

void CorrelationMatrixBuilder::addCorrelation(const CorrelationFactor& f_1,
    const CorrelationFactor& f_2, Real correlation) {
    addCorrelation(f_1, f_2, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(correlation)));
}

void CorrelationMatrixBuilder::addCorrelation(const CorrelationFactor& f_1, const CorrelationFactor& f_2,
    const Handle<Quote>& correlation) {

    // Check the factors
    checkFactor(f_1);
    checkFactor(f_2);

    // Store the correlation.
    CorrelationKey ck = createKey(f_1, f_2);
    QL_REQUIRE(correlation->value() >= -1.0 && correlation->value() <= 1.0, "Correlation value, " <<
        correlation->value() << ", for key [" << ck.first << "," << ck.second << "] should be in [-1.0,1.0]");
    corrs_[ck] = correlation;
    DLOG("Added correlation: (" << f_1 << "," << f_2 << ") = " << correlation->value() << ".");
}

Matrix CorrelationMatrixBuilder::correlationMatrix(const vector<string>& ccys) {
    ProcessInfo pi = createProcessInfo(ccys);
    return correlationMatrix(pi);
}

Matrix CorrelationMatrixBuilder::correlationMatrix(const vector<string>& ccys,
    const vector<string>& infIndices) {
    ProcessInfo pi = createProcessInfo(ccys, infIndices);
    return correlationMatrix(pi);
}

Matrix CorrelationMatrixBuilder::correlationMatrix(const vector<string>& ccys,
    const vector<string>& infIndices, const vector<string>& names) {
    ProcessInfo pi = createProcessInfo(ccys, infIndices, names);
    return correlationMatrix(pi);
}

Matrix CorrelationMatrixBuilder::correlationMatrix(const vector<string>& ccys,
    const vector<string>& infIndices, const vector<string>& names, const vector<string>& equities) {
    ProcessInfo pi = createProcessInfo(ccys, infIndices, names, equities);
    return correlationMatrix(pi);
}

Matrix CorrelationMatrixBuilder::correlationMatrix(const ProcessInfo& processInfo) {

    // Get the dimension of the matrix to create and create a list of factors.
    Size dim = 0;
    vector<CorrelationFactor> factors;
    for (const auto& kv : processInfo) {
        for (const auto& p : kv.second) {
            
            // Don't allow multiple factors for FX for now. Need to check later the FX inversion in the lookup below 
            // if we want to extend the builder to multiple factors for each FX process.
            if (kv.first == CrossAssetModel::AssetType::FX) {
                QL_REQUIRE(p.second == 1, "CorrelationMatrixBuilder does not support multiple factors for FX. " <<
                    p.first << " is set up with " << p.second << " factors.");
            }

            dim += p.second;
            for (Size i = 0; i < p.second; ++i) {
                factors.push_back({ kv.first, p.first, i });
            }
        }
    }
    
    // Start with the identity matrix
    Matrix corr(dim, dim, 0.0);
    for (Size i = 0; i < dim; i++)
        corr[i][i] = 1.0;
    
    // Populate all of the off-diagonal elements
    for (Size i = 0; i < dim; ++i) {
        for (Size j = 0; j < i; ++j) {
            corr[i][j] = corr[j][i] = getCorrelation(factors[i], factors[j])->value();
        }
    }

    return corr;
}

CorrelationMatrixBuilder::ProcessInfo CorrelationMatrixBuilder::createProcessInfo(
    const vector<string>& ccys,
    const std::vector<std::string>& inflationIndices,
    const std::vector<std::string>& creditNames,
    const std::vector<std::string>& equityNames) {
    
    // Check the currencies.
    QL_REQUIRE(!ccys.empty(), "At least one currency required to build correlation matrix");
    for (const string& ccy : ccys) {
        QL_REQUIRE(ccy.size() == 3, "Invalid currency code " << ccy);
    }

    // Hold the resulting process information.
    // Supporting a legacy method, assumed that there is 1 factor per process.
    ProcessInfo result;

    // Add process information for each currency.
    for (const string& ccy : ccys) {
        result[CrossAssetModel::AssetType::IR].emplace_back(ccy, 1);
    }

    // Add process information for each FX pair.
    for (Size i = 1; i < ccys.size(); ++i) {
        string ccyPair = ccys[i] + ccys[0];
        result[CrossAssetModel::AssetType::FX].emplace_back(ccyPair, 1);
    }

    // Add process information for inflation indices.
    for (const string& inflationIndex : inflationIndices) {
        result[CrossAssetModel::AssetType::INF].emplace_back(inflationIndex, 1);
    }

    // Add process information for credit names.
    for (const string& creditName : creditNames) {
        result[CrossAssetModel::AssetType::CR].emplace_back(creditName, 1);
    }

    // Add process information for equity names.
    for (const string& equityName : equityNames) {
        result[CrossAssetModel::AssetType::EQ].emplace_back(equityName, 1);
    }

    return result;
}

void CorrelationMatrixBuilder::checkFactor(const CorrelationFactor& f) const {
    switch (f.type) {
    case CrossAssetModel::AssetType::IR:
        QL_REQUIRE(f.name.size() == 3, "Expected IR factor name to be 3 character currency code but got: " << f.name);
        break;
    case CrossAssetModel::AssetType::FX:
        QL_REQUIRE(f.name.size() == 6, "Expected FX factor name to be 6 character currency pair but got: " << f.name);
        break;
    case CrossAssetModel::AssetType::INF:
    case CrossAssetModel::AssetType::CR:
    case CrossAssetModel::AssetType::EQ:
    case CrossAssetModel::AssetType::COM:
    case CrossAssetModel::AssetType::CrState:
        QL_REQUIRE(!f.name.empty(), "Expected non-empty factor name for factor type " << f.type);
        break;
    default:
        QL_FAIL("Did not recognise factor type " << static_cast<int>(f.type) << ".");
    }
}

CorrelationKey CorrelationMatrixBuilder::createKey(const CorrelationFactor& f_1,
    const CorrelationFactor& f_2) const {

    QL_REQUIRE(f_1 != f_2, "Correlation factors must be unique: " << f_1 << ".");
    
    if (f_1 < f_2)
        return make_pair(f_1, f_2);
    else
        return make_pair(f_2, f_1);
}

Handle<Quote> CorrelationMatrixBuilder::lookup(const string& f1, const string& f2) {
    CorrelationFactor f_1 = parseCorrelationFactor(f1);
    CorrelationFactor f_2 = parseCorrelationFactor(f2);
    return getCorrelation(f_1, f_2);
}

//! Get the correlation between the factor \p f_1 and \p f_2.
Handle<Quote> CorrelationMatrixBuilder::getCorrelation(const CorrelationFactor& f_1,
    const CorrelationFactor& f_2) const {
    
    // Create key with the correct order for lookup
    CorrelationKey ck = createKey(f_1, f_2);
    
    // If we have the correlation via direct lookup, return it.
    if (corrs_.find(ck) != corrs_.end())
        return corrs_.at(ck);

    // If one or both of the factors are FX, we may still be able to generate a correlation by using the inverse of 
    // a provided FX quote. Note that we have FX restricted to 1 factor in this class above.

    typedef DerivedQuote<negate<Real>> InvQuote;

    // If factor 1 is FX
    if (f_1.type == CrossAssetModel::AssetType::FX) {
        CorrelationFactor if_1{ CrossAssetModel::AssetType::FX, invertFx(f_1.name), f_1.index };
        ck = createKey(if_1, f_2);
        auto it = corrs_.find(ck);
        if (it != corrs_.end())
            return Handle<Quote>(QuantLib::ext::make_shared<InvQuote>(it->second, negate<Real>()));
    }

    // If factor 2 is FX
    if (f_2.type == CrossAssetModel::AssetType::FX) {
        CorrelationFactor if_2{ CrossAssetModel::AssetType::FX, invertFx(f_2.name), f_2.index };
        ck = createKey(f_1, if_2);
        auto it = corrs_.find(ck);
        if (it != corrs_.end())
            return Handle<Quote>(QuantLib::ext::make_shared<InvQuote>(it->second, negate<Real>()));
    }

    // If factor 1 and factor 2 are both FX
    if (f_1.type == CrossAssetModel::AssetType::FX && f_2.type == CrossAssetModel::AssetType::FX) {
        CorrelationFactor if_1{ CrossAssetModel::AssetType::FX, invertFx(f_1.name), f_1.index };
        CorrelationFactor if_2{ CrossAssetModel::AssetType::FX, invertFx(f_2.name), f_2.index };
        ck = createKey(if_1, if_2);
        auto it = corrs_.find(ck);
        if (it != corrs_.end())
            return it->second;
    }

    // If we still haven't found anything, return a correlation of 0.
    return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
}

const map<CorrelationKey, Handle<Quote>>& CorrelationMatrixBuilder::correlations() {
    return corrs_;
}


} // namespace data
} // namespace ore
