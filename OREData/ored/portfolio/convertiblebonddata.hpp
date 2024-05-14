/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file convertiblebonddata.hpp
    \brief convertible bond data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

using ore::data::ScheduleData;
    
class ConvertibleBondData : public ore::data::XMLSerializable {
public:
    // 1 Callability Data

    class CallabilityData : public ore::data::XMLSerializable {
    public:
        // 1.1 Make Whole Data

        class MakeWholeData : public ore::data::XMLSerializable {
        public:
            // 1.1.1 Conversion Ratio Increase Data

            class ConversionRatioIncreaseData : public ore::data::XMLSerializable {
            public:
                ConversionRatioIncreaseData() : initialised_(false) {}
                bool initialised() const { return initialised_; }

                const std::string& cap() const { return cap_; }
                const std::vector<double>& stockPrices() const { return stockPrices_; }
                const std::vector<std::vector<double>>& crIncrease() const { return crIncrease_; }
                const std::vector<std::string>& crIncreaseDates() const { return crIncreaseDates_; }

                void fromXML(ore::data::XMLNode* node) override;
                ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

            private:
                bool initialised_;
                std::string cap_;
                std::vector<double> stockPrices_;
                std::vector<std::vector<double>> crIncrease_;
                std::vector<std::string> crIncreaseDates_;
            };

            MakeWholeData() : initialised_(false) {}
            bool initialised() const { return initialised_; }

            const ConversionRatioIncreaseData& conversionRatioIncreaseData() const {
                return conversionRatioIncreaseData_;
            }

            void fromXML(ore::data::XMLNode* node) override;
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        private:
            bool initialised_;

            ConversionRatioIncreaseData conversionRatioIncreaseData_;
        };

        // Callability Data

        explicit CallabilityData(const std::string& nodeName) : initialised_(false), nodeName_(nodeName) {}

        bool initialised() const { return initialised_; }

        const ScheduleData& dates() const { return dates_; }

        const std::vector<std::string>& styles() const { return styles_; }
        const std::vector<std::string>& styleDates() const { return styleDates_; }
        const std::vector<double>& prices() const { return prices_; }
        const std::vector<std::string>& priceDates() const { return priceDates_; }
        const std::vector<std::string>& priceTypes() const { return priceTypes_; }
        const std::vector<std::string>& priceTypeDates() const { return priceTypeDates_; }
        const std::vector<bool>& includeAccrual() const { return includeAccrual_; }
        const std::vector<std::string>& includeAccrualDates() const { return includeAccrualDates_; }
        const std::vector<bool>& isSoft() const { return isSoft_; }
        const std::vector<std::string>& isSoftDates() const { return isSoftDates_; }
        const std::vector<double>& triggerRatios() const { return triggerRatios_; }
        const std::vector<std::string>& triggerRatioDates() const { return triggerRatioDates_; }
        const std::vector<std::string>& nOfMTriggers() const { return nOfMTriggers_; }
        const std::vector<std::string>& nOfMTriggerDates() const { return nOfMTriggerDates_; }
        const MakeWholeData& makeWholeData() const { return makeWholeData_; }

        void fromXML(ore::data::XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    private:
        bool initialised_;
        std::string nodeName_;

        ScheduleData dates_;
        std::vector<std::string> styles_;
        std::vector<std::string> styleDates_;
        std::vector<double> prices_;
        std::vector<std::string> priceDates_;
        std::vector<std::string> priceTypes_;
        std::vector<std::string> priceTypeDates_;
        std::vector<bool> includeAccrual_;
        std::vector<std::string> includeAccrualDates_;
        std::vector<bool> isSoft_;
        std::vector<std::string> isSoftDates_;
        std::vector<double> triggerRatios_;
        std::vector<std::string> triggerRatioDates_;
        std::vector<std::string> nOfMTriggers_;
        std::vector<std::string> nOfMTriggerDates_;
        MakeWholeData makeWholeData_;
    };

    // 2 Conversion Data

    class ConversionData : public ore::data::XMLSerializable {
    public:
        // 2.1 Contingent Conversion Data

        class ContingentConversionData : public ore::data::XMLSerializable {
        public:
            ContingentConversionData() : initialised_(false) {}

            bool initialised() const { return initialised_; }

            const std::vector<std::string>& observations() const { return observations_; }
            const std::vector<std::string>& observationDates() const { return observationDates_; }
            const std::vector<double>& barriers() const { return barriers_; }
            const std::vector<std::string>& barrierDates() const { return barrierDates_; }

            void fromXML(ore::data::XMLNode* node) override;
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        private:
            bool initialised_;

            std::vector<std::string> observations_;
            std::vector<std::string> observationDates_;
            std::vector<double> barriers_;
            std::vector<std::string> barrierDates_;
        };

        // 2.2 Mandatory Conversion Data

        class MandatoryConversionData : public ore::data::XMLSerializable {
        public:
            class PepsData : public ore::data::XMLSerializable {
            public:
                PepsData() : initialised_(false) {}

                bool initialised() const { return initialised_; }

                double upperBarrier() const { return upperBarrier_; }
                double lowerBarrier() const { return lowerBarrier_; }
                double upperConversionRatio() const { return upperConversionRatio_; }
                double lowerConversionRatio() const { return lowerConversionRatio_; }

                void fromXML(ore::data::XMLNode* node) override;
                ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

            private:
                bool initialised_;

                double upperBarrier_;
                double lowerBarrier_;
                double upperConversionRatio_;
                double lowerConversionRatio_;
            };

            MandatoryConversionData() : initialised_(false) {}

            bool initialised() const { return initialised_; }

            const std::string& date() const { return date_; }
            const std::string& type() const { return type_; }

            const PepsData& pepsData() const { return pepsData_; }

            void fromXML(ore::data::XMLNode* node) override;
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        private:
            bool initialised_;

            std::string date_;
            std::string type_;

            PepsData pepsData_;
        };

        // 2.3 Conversion Reset Data

        class ConversionResetData : public ore::data::XMLSerializable {
        public:
            ConversionResetData() : initialised_(false) {}

            bool initialised() const { return initialised_; }

            const ScheduleData& dates() const { return dates_; }
            const std::vector<std::string>& references() const { return references_; }
            const std::vector<std::string>& referenceDates() const { return referenceDates_; }
            const std::vector<double>& thresholds() const { return thresholds_; }
            const std::vector<std::string>& thresholdDates() const { return thresholdDates_; }
            const std::vector<double>& gearings() const { return gearings_; }
            const std::vector<std::string>& gearingDates() const { return gearingDates_; }
            const std::vector<double>& floors() const { return floors_; }
            const std::vector<std::string>& floorDates() const { return floorDates_; }
            const std::vector<double>& globalFloors() const { return globalFloors_; }
            const std::vector<std::string>& globalFloorDates() const { return globalFloorDates_; }

            void fromXML(ore::data::XMLNode* node) override;
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        private:
            bool initialised_;

            ScheduleData dates_;
            std::vector<std::string> references_;
            std::vector<std::string> referenceDates_;
            std::vector<double> thresholds_;
            std::vector<std::string> thresholdDates_;
            std::vector<double> gearings_;
            std::vector<std::string> gearingDates_;
            std::vector<double> floors_;
            std::vector<std::string> floorDates_;
            std::vector<double> globalFloors_;
            std::vector<std::string> globalFloorDates_;
        };

        // 2.4 Exchangeable Data

        class ExchangeableData : public ore::data::XMLSerializable {
        public:
            ExchangeableData() : initialised_(false) {}

            bool initialised() const { return initialised_; }

            bool isExchangeable() const { return isExchangeable_; }
            const std::string& equityCreditCurve() const { return equityCreditCurve_; }
            std::string& modifyEquityCreditCurve() { return equityCreditCurve_; }
            bool secured() const { return secured_; }

            void fromXML(ore::data::XMLNode* node) override;
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        private:
            bool initialised_;

            bool isExchangeable_;
            std::string equityCreditCurve_;
            bool secured_;
        };

        // 2.5 Fixed Amount Conversion Data

        class FixedAmountConversionData : public ore::data::XMLSerializable {
        public:
            FixedAmountConversionData() : initialised_(false) {}

            bool initialised() const { return initialised_; }

            const std::string& currency() const { return currency_; }

            const std::vector<double>& amounts() const { return amounts_; }
            const std::vector<std::string>& amountDates() const { return amountDates_; }

            void fromXML(ore::data::XMLNode* node) override;
            ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

        private:
            bool initialised_;

            std::string currency_;
            std::vector<double> amounts_;
            std::vector<std::string> amountDates_;
        };

        // Conversion Data

        ConversionData() : initialised_(false) {}

        bool initialised() const { return initialised_; }

        const ScheduleData& dates() const { return dates_; }
        const std::vector<std::string>& styles() const { return styles_; }
        const std::vector<std::string>& styleDates() const { return styleDates_; }
        const std::vector<double>& conversionRatios() const { return conversionRatios_; }
        const std::vector<std::string>& conversionRatioDates() const { return conversionRatioDates_; }

        const ContingentConversionData& contingentConversionData() const { return contingentConversionData_; }
        const MandatoryConversionData& mandatoryConversionData() const { return mandatoryConversionData_; }
        const ConversionResetData& conversionResetData() const { return conversionResetData_; }
        const ore::data::EquityUnderlying equityUnderlying() const { return equityUnderlying_; }
        const std::string fxIndex() const { return fxIndex_; }
        const ExchangeableData& exchangeableData() const { return exchangeableData_; }
        const FixedAmountConversionData& fixedAmountConversionData() const { return fixedAmountConversionData_; }
        ExchangeableData& modifyExchangeableData() { return exchangeableData_; }

        void fromXML(ore::data::XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    private:
        bool initialised_;

        ScheduleData dates_;
        std::vector<std::string> styles_;
        std::vector<std::string> styleDates_;
        std::vector<double> conversionRatios_;
        std::vector<std::string> conversionRatioDates_;

        ContingentConversionData contingentConversionData_;
        MandatoryConversionData mandatoryConversionData_;
        ConversionResetData conversionResetData_;
        ore::data::EquityUnderlying equityUnderlying_;
        std::string fxIndex_;
        ExchangeableData exchangeableData_;
        FixedAmountConversionData fixedAmountConversionData_;
    };

    // 3 Dividend Protection Data

    class DividendProtectionData : public ore::data::XMLSerializable {
    public:
        DividendProtectionData() : initialised_(false) {}

        bool initialised() const { return initialised_; }

        const ScheduleData& dates() const { return dates_; }
        const std::vector<std::string>& adjustmentStyles() const { return adjustmentStyles_; }
        const std::vector<std::string>& adjustmentStyleDates() const { return adjustmentStyleDates_; }
        const std::vector<std::string>& dividendTypes() const { return dividendTypes_; }
        const std::vector<std::string>& dividendTypeDates() const { return dividendTypeDates_; }
        const std::vector<double>& thresholds() const { return thresholds_; }
        const std::vector<std::string>& thresholdDates() const { return thresholdDates_; }

        void fromXML(ore::data::XMLNode* node) override;
        ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    private:
        bool initialised_;

        ScheduleData dates_;
        std::vector<std::string> adjustmentStyles_;
        std::vector<std::string> adjustmentStyleDates_;
        std::vector<std::string> dividendTypes_;
        std::vector<std::string> dividendTypeDates_;
        std::vector<double> thresholds_;
        std::vector<std::string> thresholdDates_;
    };

    // Convertible Bond Data

    ConvertibleBondData() {}
    explicit ConvertibleBondData(const ore::data::BondData& bondData) : bondData_(bondData) {}

    const ore::data::BondData& bondData() const { return bondData_; }

    const CallabilityData& callData() const { return callData_; }
    const CallabilityData& putData() const { return putData_; }
    const ConversionData& conversionData() const { return conversionData_; }
    ConversionData& modifyConversionData() { return conversionData_; }
    const DividendProtectionData& dividendProtectionData() const { return dividendProtectionData_; }
    std::string detachable() const { return detachable_; }

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    void populateFromBondReferenceData(const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData);

private:
    ore::data::BondData bondData_;
    CallabilityData callData_ = CallabilityData("CallData");
    CallabilityData putData_ = CallabilityData("PutData");
    ConversionData conversionData_;
    DividendProtectionData dividendProtectionData_;
    std::string detachable_;
};

} // namespace data
} // namespace ore
