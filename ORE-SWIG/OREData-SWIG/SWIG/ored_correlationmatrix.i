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

#ifndef ored_correlationmatrix_i
#define ored_correlationmatrix_i

%{
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/utilities/correlationmatrix.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/quotes/simplequote.hpp>

#include <sstream>

typedef ore::data::CorrelationFactor CorrelationFactor;
typedef ore::data::CorrelationKey CorrelationKey;
typedef ore::data::CorrelationMatrixBuilder CorrelationMatrixBuilder;
typedef ore::data::InstantaneousCorrelations InstantaneousCorrelations;

static ore::data::CorrelationFactor oreplusMakeCorrelationFactor(
    const std::string& type, const std::string& name, QuantLib::Size index = 0) {
    ore::data::CorrelationFactor factor;
    factor.type = ore::data::parseCamAssetType(type);
    factor.name = name;
    factor.index = index;
    return factor;
}

static ore::data::CorrelationMatrixBuilder oreplusCorrelationBuilderFromMap(
    const std::map<ore::data::CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& corrs) {
    ore::data::CorrelationMatrixBuilder builder;
    for (const auto& entry : corrs) {
        builder.addCorrelation(entry.first.first, entry.first.second, entry.second);
    }
    return builder;
}
%}

%include <std_map.i>
%include <std_pair.i>
%include <std_string.i>
%include <std_vector.i>
%include ored_xmlutils.i

%shared_ptr(ore::data::InstantaneousCorrelations)
%template(CorrelationFactorVector) std::vector<ore::data::CorrelationFactor>;

namespace ore {
namespace data {

struct CorrelationFactor {
    QuantExt::CrossAssetModel::AssetType type;
    std::string name;
    QuantLib::Size index;
};

class CorrelationMatrixBuilder {
public:
    CorrelationMatrixBuilder();
    void reset();
    void addCorrelation(const std::string& factor1, const std::string& factor2, QuantLib::Real correlation);
    void addCorrelation(const std::string& factor1, const std::string& factor2,
                        const QuantLib::Handle<QuantLib::Quote>& correlation);
    void addCorrelation(const CorrelationFactor& f_1, const CorrelationFactor& f_2, QuantLib::Real correlation);
    void addCorrelation(const CorrelationFactor& f_1, const CorrelationFactor& f_2,
                        const QuantLib::Handle<QuantLib::Quote>& correlation);
    QuantLib::Handle<QuantLib::Quote> lookup(const std::string& f1, const std::string& f2);
    QuantLib::Handle<QuantLib::Quote> getCorrelation(const CorrelationFactor& f_1,
                                                     const CorrelationFactor& f_2) const;
    const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations();

    %extend {
        void addCorrelationValue(const std::string& type1, const std::string& name1, QuantLib::Size index1,
                                 const std::string& type2, const std::string& name2, QuantLib::Size index2,
                                 QuantLib::Real correlation) {
            self->addCorrelation(oreplusMakeCorrelationFactor(type1, name1, index1),
                                 oreplusMakeCorrelationFactor(type2, name2, index2), correlation);
        }

        QuantLib::Real correlationValue(const ore::data::CorrelationFactor& factor1,
                                        const ore::data::CorrelationFactor& factor2) const {
            return self->getCorrelation(factor1, factor2)->value();
        }

        QuantLib::Real correlationValue(const std::string& type1, const std::string& name1,
                                        QuantLib::Size index1, const std::string& type2,
                                        const std::string& name2, QuantLib::Size index2) const {
            return self->getCorrelation(oreplusMakeCorrelationFactor(type1, name1, index1),
                                        oreplusMakeCorrelationFactor(type2, name2, index2))
                ->value();
        }
    }
};

class InstantaneousCorrelations : public ore::data::XMLSerializable {
public:
    InstantaneousCorrelations();
    InstantaneousCorrelations(const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations);
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& correlations() const;
    void clear();
    void correlations(const std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>>& corrs);
    bool operator==(const InstantaneousCorrelations& rhs);
    bool operator!=(const InstantaneousCorrelations& rhs);

    %extend {
        InstantaneousCorrelations(ore::data::CorrelationMatrixBuilder& builder) {
            return new ore::data::InstantaneousCorrelations(builder.correlations());
        }

        void setCorrelation(const ore::data::CorrelationFactor& factor1,
                            const ore::data::CorrelationFactor& factor2,
                            QuantLib::Real correlation) {
            ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
            builder.addCorrelation(factor1, factor2, correlation);
            self->correlations(builder.correlations());
        }

        void setCorrelationValue(const std::string& type1, const std::string& name1, QuantLib::Size index1,
                                 const std::string& type2, const std::string& name2, QuantLib::Size index2,
                                 QuantLib::Real correlation) {
            ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
            builder.addCorrelation(oreplusMakeCorrelationFactor(type1, name1, index1),
                                   oreplusMakeCorrelationFactor(type2, name2, index2), correlation);
            self->correlations(builder.correlations());
        }

        QuantLib::Real correlationValue(const ore::data::CorrelationFactor& factor1,
                                        const ore::data::CorrelationFactor& factor2) const {
            ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
            return builder.getCorrelation(factor1, factor2)->value();
        }

        QuantLib::Real correlationValue(const std::string& type1, const std::string& name1,
                                        QuantLib::Size index1, const std::string& type2,
                                        const std::string& name2, QuantLib::Size index2) const {
            ore::data::CorrelationMatrixBuilder builder = oreplusCorrelationBuilderFromMap(self->correlations());
            return builder.getCorrelation(oreplusMakeCorrelationFactor(type1, name1, index1),
                                          oreplusMakeCorrelationFactor(type2, name2, index2))
                ->value();
        }
    }
};

} // namespace data
} // namespace ore

%extend ore::data::CorrelationFactor {
    CorrelationFactor(const std::string& type, const std::string& name, QuantLib::Size index = 0) {
        return new ore::data::CorrelationFactor(oreplusMakeCorrelationFactor(type, name, index));
    }

    std::string typeName() const {
        std::ostringstream out;
        out << self->type;
        return out.str();
    }

    void setTypeName(const std::string& type) {
        self->type = ore::data::parseCamAssetType(type);
    }
}

#endif
