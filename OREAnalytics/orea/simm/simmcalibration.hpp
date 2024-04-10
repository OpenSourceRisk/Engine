/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/simm/simmcalibration.hpp
    \brief SIMM class for defining SIMM risk weights, thresholds, buckets, and labels.
        Currently only supports the latest ISDA SIMM versions
        (apart from changes in the aforementioned four things).
*/

#pragma once

#include <boost/any.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <map>
#include <string>

namespace ore {
namespace analytics {

class SimmCalibration : public ore::data::XMLSerializable {
public:
    typedef std::map<std::tuple<std::string, std::string, std::string>, std::string> Amounts;
    typedef std::map<std::tuple<std::string, std::string, std::string>, std::set<std::string>> CurrencyLists;
    class Amount : public ore::data::XMLSerializable {
    public:
        Amount() {}
        Amount(const std::string& bucket, const std::string& label1, const std::string& label2,
               const std::string& value)
            : bucket_(bucket), label1_(label1), label2_(label2), value_(value) {}
        Amount(const std::tuple<std::string, std::string, std::string>& key, const std::string& value);
        Amount(ore::data::XMLNode* node) { fromXML(node); }

        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
        void fromXML(ore::data::XMLNode* node) override;

        const std::tuple<std::string, std::string, std::string> key() const;

        const std::string& bucket() const { return bucket_; }
        const std::string& label1() const { return label1_; }
        const std::string& label2() const { return label2_; }
        const std::string& value() const { return value_; }

    private:
        std::string bucket_, label1_, label2_, value_;
    };

    class RiskClassData : public ore::data::XMLSerializable {
    public:
        class RiskWeights : public ore::data::XMLSerializable {
        public:
            RiskWeights() {}
            RiskWeights(const SimmConfiguration::RiskClass& rc) : riskClass_(rc) {}
            RiskWeights(const SimmConfiguration::RiskClass& rc, ore::data::XMLNode* node);

            virtual const std::map<CrifRecord::RiskType, std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>>
            uniqueRiskWeights() const {
                return std::map<CrifRecord::RiskType, std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>>();
            }

            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}

            const std::map<QuantLib::Size, Amounts>& delta() const { return delta_; }
            const std::map<QuantLib::Size, Amounts>& vega() const { return vega_; }
            const std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>& historicalVolatilityRatio() const {
                return historicalVolatilityRatio_;
            }

        private:
            SimmConfiguration::RiskClass riskClass_;
            //       MPOR days
            std::map<QuantLib::Size, Amounts> delta_;
            std::map<QuantLib::Size, Amounts> vega_;
            std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>> historicalVolatilityRatio_;
        };
        class IRRiskWeights : public RiskWeights {
        public:
            IRRiskWeights(ore::data::XMLNode* node);
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            virtual const std::map<CrifRecord::RiskType, std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>>
            uniqueRiskWeights() const override;

            const std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>& inflation() const { return inflation_; }
            const std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>& xCcyBasis() const { return xCcyBasis_; }
            const CurrencyLists& currencyLists() const { return currencyLists_; }

        private:
            std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>> inflation_;
            std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>> xCcyBasis_;
            CurrencyLists currencyLists_;
        };
        class CreditQRiskWeights : public RiskWeights {
        public:
            CreditQRiskWeights(ore::data::XMLNode* node);
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            virtual const std::map<CrifRecord::RiskType, std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>>>
            uniqueRiskWeights() const override;

        private:
            std::map<QuantLib::Size, QuantLib::ext::shared_ptr<Amount>> baseCorrelation_;
        };
        class FXRiskWeights : public RiskWeights {
        public:
            FXRiskWeights(ore::data::XMLNode* node);
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            const CurrencyLists& currencyLists() const { return currencyLists_; }

        private:
            CurrencyLists currencyLists_;
        };

        class Correlations : public ore::data::XMLSerializable {
        public:
            Correlations() {}
            Correlations(ore::data::XMLNode* node) { fromXML(node); }
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            const Amounts& intraBucketCorrelations() const { return intraBucketCorrelations_; }
            const Amounts& interBucketCorrelations() const { return interBucketCorrelations_; }

        private:
            Amounts intraBucketCorrelations_;
            Amounts interBucketCorrelations_;
        };
        class IRCorrelations : public Correlations {
        public:
            IRCorrelations(ore::data::XMLNode* node) { fromXML(node); }
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            const QuantLib::ext::shared_ptr<Amount>& subCurves() const { return subCurves_; }
            const QuantLib::ext::shared_ptr<Amount>& inflation() const { return inflation_; }
            const QuantLib::ext::shared_ptr<Amount>& xCcyBasis() const { return xCcyBasis_; }
            const QuantLib::ext::shared_ptr<Amount>& outer() const { return outer_; }

        private:
            QuantLib::ext::shared_ptr<Amount> subCurves_;
            QuantLib::ext::shared_ptr<Amount> inflation_;
            QuantLib::ext::shared_ptr<Amount> xCcyBasis_;
            QuantLib::ext::shared_ptr<Amount> outer_;
        };
        class CreditQCorrelations : public Correlations {
        public:
            CreditQCorrelations(ore::data::XMLNode* node) { fromXML(node); }
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            const QuantLib::ext::shared_ptr<Amount>& baseCorrelation() const { return baseCorrelation_; }

        private:
            QuantLib::ext::shared_ptr<Amount> baseCorrelation_;
        };
        class FXCorrelations : public Correlations {
        public:
            FXCorrelations(ore::data::XMLNode* node) { fromXML(node); }
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}
            const QuantLib::ext::shared_ptr<Amount>& volatility() const { return volatility_; }

        private:
            QuantLib::ext::shared_ptr<Amount> volatility_;
        };

        class ConcentrationThresholds : public ore::data::XMLSerializable {
        public:
            ConcentrationThresholds() {}
            ConcentrationThresholds(ore::data::XMLNode* node) { fromXML(node); }
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}

            const Amounts& delta() const { return delta_; }
            const Amounts& vega() const { return vega_; }

        private:
            Amounts delta_;
            Amounts vega_;
        };
        class IRFXConcentrationThresholds : public ConcentrationThresholds {
        public:
            IRFXConcentrationThresholds() {}
            IRFXConcentrationThresholds(ore::data::XMLNode* node) { fromXML(node); }
            //! \name Serialisation
            //@{
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
            void fromXML(ore::data::XMLNode* node) override;
            //@}

            const CurrencyLists& currencyLists() const { return currencyLists_; }

        private:
            CurrencyLists currencyLists_;
        };

        RiskClassData() : riskClass_(SimmConfiguration::RiskClass::All) {}
        RiskClassData(const SimmConfiguration::RiskClass& riskClass) : riskClass_(riskClass) {}

        //! \name Serialisation
        //@{
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
        void fromXML(ore::data::XMLNode* node) override;
        //@}

        const QuantLib::ext::shared_ptr<RiskWeights>& riskWeights() const { return riskWeights_; }
        const QuantLib::ext::shared_ptr<Correlations>& correlations() const { return correlations_; }
        const QuantLib::ext::shared_ptr<ConcentrationThresholds>& concentrationThresholds() const {
            return concentrationThresholds_;
        }
        const std::map<CrifRecord::RiskType, std::vector<std::string>>& buckets() const { return buckets_; }
        const std::map<CrifRecord::RiskType, std::vector<std::string>>& labels1() const { return labels1_; }
        const std::map<CrifRecord::RiskType, std::vector<std::string>>& labels2() const { return labels2_; }

    private:
        SimmConfiguration::RiskClass riskClass_;
        QuantLib::ext::shared_ptr<RiskWeights> riskWeights_;
        QuantLib::ext::shared_ptr<Correlations> correlations_;
        QuantLib::ext::shared_ptr<ConcentrationThresholds> concentrationThresholds_;
        std::map<CrifRecord::RiskType, std::vector<std::string>> buckets_;
        std::map<CrifRecord::RiskType, std::vector<std::string>> labels1_;
        std::map<CrifRecord::RiskType, std::vector<std::string>> labels2_;
    };

    SimmCalibration() {}
    SimmCalibration(ore::data::XMLNode* node) { fromXML(node); }

    const std::string& version() const;
    const std::vector<std::string>& versionNames() const { return versionNames_; }
    const std::vector<std::pair<std::string, std::string>>& additionalFields() const { return additionalFields_; }

    //! \name Serialisation
    //@{
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    void fromXML(ore::data::XMLNode* node) override;
    //@}
    const std::map<SimmConfiguration::RiskClass, QuantLib::ext::shared_ptr<RiskClassData>>& riskClassData() const {
        return riskClassData_;
    }
    const Amounts& riskClassCorrelations() const { return riskClassCorrelations_; }

    // TODO: This would be useful for ensuring that the SimmCalibration obj is compatible with the data provided.
    void validate() const {}

    const std::string& id() const { return id_; }

private:
    std::string id_;
    std::vector<std::string> versionNames_;
    std::vector<std::pair<std::string, std::string>> additionalFields_;
    std::map<SimmConfiguration::RiskClass, QuantLib::ext::shared_ptr<RiskClassData>> riskClassData_;
    Amounts riskClassCorrelations_;
};

class SimmCalibrationData : public ore::data::XMLSerializable {
public:
    //!
    SimmCalibrationData() {}

    //! \name Serialisation
    //@{
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    void fromXML(ore::data::XMLNode* node) override;
    //@}

    void add(const QuantLib::ext::shared_ptr<SimmCalibration>&);
    bool hasId(const std::string& id) const { return data_.find(id) != data_.end(); }
    const QuantLib::ext::shared_ptr<SimmCalibration>& getById(const std::string& id) const;
    const QuantLib::ext::shared_ptr<SimmCalibration> getBySimmVersion(const std::string& id) const;

private:
    std::map<std::string, QuantLib::ext::shared_ptr<SimmCalibration>> data_;
};

} // namespace analytics
} // namespace ore
