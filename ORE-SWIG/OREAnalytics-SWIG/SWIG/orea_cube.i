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

#ifndef orea_cube_i
#define orea_cube_i

%include stl.i
%include types.i

%shared_ptr(ore::analytics::NPVCube)
%shared_ptr(ore::analytics::SensitivityCube)
%nodefaultctor ore::analytics::SensitivityCube;
%shared_ptr(ore::analytics::CubeWriter)
%rename(CubeReader) ore::analytics::CubeCsvReader;
%shared_ptr(ore::analytics::CubeCsvReader)
%shared_ptr(ore::analytics::AggregationScenarioData)
%shared_ptr(ore::analytics::InMemoryCubeOpt<float>);
%shared_ptr(ore::analytics::InMemoryCubeOpt<double>);
%shared_ptr(ore::analytics::JointNPVCube)

namespace ore {
namespace analytics {
class NPVCube {
  public:
    //! Return the length of each dimension
    virtual Size numIds() const = 0;
    virtual Size numDates() const = 0;
    virtual Size samples() const = 0;
    virtual Size depth() const = 0;

    //! Get a set of all ids in the cube
    const std::set<std::string> ids() const;
    //! Get the vector of dates for this cube
    virtual const std::vector<QuantLib::Date>& dates() const = 0;

    //! Return the asof date (T0 date)
    virtual QuantLib::Date asof() const = 0;
    //! Get a T0 value from the cube using index
    virtual Real getT0(Size id, Size depth = 0) const = 0;
    //! Get a T0 value from the cube using trade id
    virtual Real getT0(const std::string& id, Size depth = 0) const;
    //! Get a value from the cube using index
    virtual Real get(Size id, Size date, Size sample, Size depth = 0) const = 0;
    //! Get a value from the cube using trade id and date
    virtual Real get(const std::string& id, const QuantLib::Date& date, Size sample, Size depth = 0) const;
};

class SensitivityCube {
    public:
        bool hasTrade(const std::string& tradeId) const;
        QuantLib::Real npv(const std::string& tradeId) const;
};

class CubeWriter {
    public:
        CubeWriter(const std::string& filename);
        const std::string& filename();
};

class CubeCsvReader {
    public:
        CubeCsvReader(const std::string& filename, const bool useDoublePrecision = false);
        const std::string& filename() const;
};

enum class AggregationScenarioDataType : unsigned int { IndexFixing = 0, FXSpot = 1, Numeraire = 2, Generic = 3 };

class AggregationScenarioData {
public:
    //! Return the length of each dimension
    virtual Size dimDates() const = 0;
    virtual Size dimSamples() const = 0;

    //! Check whether data is available for the given type
    virtual bool has(const AggregationScenarioDataType& type, const string& qualifier = "") const = 0;

    //! Get a value from the cube
    virtual Real get(Size dateIndex, Size sampleIndex, const AggregationScenarioDataType& type,
                     const string& qualifier = "") const = 0;

    // Get available keys (type, qualifier)
    virtual std::vector<std::pair<AggregationScenarioDataType, std::string>> keys() const = 0;

    //! Go to the next point on the cube
    /*! Go to the next point on the cube, assumes we do date, then samples
     */
    virtual void next() {
        if (++dIndex_ == dimDates()) {
            dIndex_ = 0;
            sIndex_++;
        }
    }
};

template <typename T> class InMemoryCubeOpt : public ore::analytics::NPVCube {
public:
    InMemoryCubeOpt(const QuantLib::Date& asof, const std::set<std::string>& ids,
                    const std::vector<QuantLib::Date>& dates, QuantLib::Size samples, const T& t = T());
    InMemoryCubeOpt(const QuantLib::Date& asof, const std::set<std::string>& ids,
                    const std::vector<QuantLib::Date>& dates, QuantLib::Size samples, QuantLib::Size depth,
                    const T& t = T());
    QuantLib::Size numIds() const override;
    QuantLib::Size numDates() const override;
    QuantLib::Size samples() const override;
    QuantLib::Size depth() const override;
    const std::map<std::string, QuantLib::Size>& idsAndIndexes() const override;
    const std::vector<QuantLib::Date>& dates() const override;
    QuantLib::Date asof() const override;
    QuantLib::Real getT0(QuantLib::Size i, QuantLib::Size d) const override;
    void setT0(QuantLib::Real value, QuantLib::Size i, QuantLib::Size d) override;
    QuantLib::Real get(QuantLib::Size i, QuantLib::Size j, QuantLib::Size k, QuantLib::Size d) const override;
    void set(QuantLib::Real value, QuantLib::Size i, QuantLib::Size j, QuantLib::Size k, QuantLib::Size d) override;
};

} // namespace analytics
} // namespace ore

%template(SinglePrecisionInMemoryCubeN) ore::analytics::InMemoryCubeOpt<float>;
%template(DoublePrecisionInMemoryCubeN) ore::analytics::InMemoryCubeOpt<double>;

%shared_ptr(SinglePrecisionInMemoryCubeN);
%shared_ptr(DoublePrecisionInMemoryCubeN);

%template(DoublePrecisionInMemoryCubeNVector) std::vector<QuantLib::ext::shared_ptr<ore::analytics::InMemoryCubeOpt<double>>>;

namespace ore {
namespace analytics {
class JointNPVCube : public ore::analytics::NPVCube {
public:
    %extend {
    JointNPVCube(
        const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube1, const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube2,
        const std::set<std::string>& ids = {}, const bool requireUniqueIds = true) {
            return new ore::analytics::JointNPVCube(cube1, cube2, ids, requireUniqueIds);
        }

    JointNPVCube(
        const std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>>& cubes, const std::set<std::string>& ids = {},
        const bool requireUniqueIds = true){
            return new ore::analytics::JointNPVCube(cubes, ids, requireUniqueIds);
        }

        JointNPVCube(const QuantLib::ext::shared_ptr<InMemoryCubeOpt<double>>& cube1,
                     const QuantLib::ext::shared_ptr<InMemoryCubeOpt<double>>& cube2, const std::set<std::string>& ids = {},
                     const bool requireUniqueIds = true) {
            return new ore::analytics::JointNPVCube(cube1, cube2, ids, requireUniqueIds);
        }

        JointNPVCube(const std::vector<QuantLib::ext::shared_ptr<InMemoryCubeOpt<double>>>& cubes,
                     const std::set<std::string>& ids = {}, const bool requireUniqueIds = true) {
            std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> npvCubes(cubes.begin(), cubes.end());
            return new ore::analytics::JointNPVCube(npvCubes, ids, requireUniqueIds);
        }

        JointNPVCube(const QuantLib::ext::shared_ptr<InMemoryCubeOpt<float>>& cube1,
                     const QuantLib::ext::shared_ptr<InMemoryCubeOpt<float>>& cube2, const std::set<std::string>& ids = {},
                     const bool requireUniqueIds = true) {
            return new ore::analytics::JointNPVCube(cube1, cube2, ids, requireUniqueIds);
        }

        JointNPVCube(const std::vector<QuantLib::ext::shared_ptr<InMemoryCubeOpt<float>>>& cubes,
                     const std::set<std::string>& ids = {}, const bool requireUniqueIds = true) {
            std::vector<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>> npvCubes(cubes.begin(), cubes.end());
            return new ore::analytics::JointNPVCube(npvCubes, ids, requireUniqueIds);
        }
    }
    QuantLib::Size numIds() const override;
    QuantLib::Size numDates() const override;
    QuantLib::Size samples() const override;
    QuantLib::Size depth() const override;

    const std::map<std::string, QuantLib::Size>& idsAndIndexes() const override;
    const std::vector<QuantLib::Date>& dates() const override;
    QuantLib::Date asof() const override;

    QuantLib::Real getT0(QuantLib::Size id, QuantLib::Size depth = 0) const override;
    void setT0(QuantLib::Real value, QuantLib::Size id, QuantLib::Size depth = 0) override;

    QuantLib::Real get(QuantLib::Size id, QuantLib::Size date, QuantLib::Size sample, QuantLib::Size depth = 0) const override;
    void set(QuantLib::Real value, QuantLib::Size id, QuantLib::Size date, QuantLib::Size sample, QuantLib::Size depth = 0) override;
};

} // namespace analytics
} // namespace ore

%shared_ptr(ore::analytics::CubeInterpretation)

namespace ore {
namespace analytics {

class CubeInterpretation {
public:
    CubeInterpretation(const bool storeFlows, const bool withCloseOutLag,
                       const bool withExerciseValue = false,
                       const ext::shared_ptr<ore::data::DateGrid>& dateGrid = ext::shared_ptr<ore::data::DateGrid>(),
                       const QuantLib::Size storeCreditStateNPVs = 0, const bool flipViewXVA = false);

    //! Inspectors
    bool storeFlows() const;
    bool withCloseOutLag() const;
    bool withExerciseValue() const;
    const ext::shared_ptr<ore::data::DateGrid>& dateGrid() const;
    QuantLib::Size storeCreditStateNPVs() const;
    bool flipViewXVA() const;

    //! Required npv cube depth for this interpretation
    QuantLib::Size requiredNpvCubeDepth() const;

    //! Indices in depth direction (may be Null<Size>() if not applicable)
    QuantLib::Size defaultDateNpvIndex() const;
    QuantLib::Size closeOutDateNpvIndex() const;
    QuantLib::Size exerciseValueIndex() const;
    QuantLib::Size mporFlowsIndex() const;
    QuantLib::Size creditStateNPVsIndex() const;

    //! Value retrieval from cube
    QuantLib::Real getGenericValue(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                   QuantLib::Size tradeIdx, QuantLib::Size dateIdx,
                                   QuantLib::Size sampleIdx, QuantLib::Size depth) const;
    QuantLib::Real getDefaultNpv(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                 QuantLib::Size tradeIdx, QuantLib::Size dateIdx, QuantLib::Size sampleIdx) const;
    QuantLib::Real getCloseOutNpv(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                  QuantLib::Size tradeIdx, QuantLib::Size dateIdx, QuantLib::Size sampleIdx,
                                  const ext::shared_ptr<ore::analytics::AggregationScenarioData>& data) const;
    QuantLib::Real getExerciseValue(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                    QuantLib::Size tradeIdx, QuantLib::Size dateIdx, QuantLib::Size sampleIdx) const;
    QuantLib::Real getMporPositiveFlows(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                        QuantLib::Size tradeIdx, QuantLib::Size dateIdx, QuantLib::Size sampleIdx) const;
    QuantLib::Real getMporNegativeFlows(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                        QuantLib::Size tradeIdx, QuantLib::Size dateIdx, QuantLib::Size sampleIdx) const;
    QuantLib::Real getMporFlows(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                QuantLib::Size tradeIdx, QuantLib::Size dateIdx, QuantLib::Size sampleIdx) const;
    QuantLib::Real getDefaultAggregationScenarioData(
        const ext::shared_ptr<ore::analytics::AggregationScenarioData>& data,
        const ore::analytics::AggregationScenarioDataType& dataType,
        QuantLib::Size dateIdx, QuantLib::Size sampleIdx, const std::string& qualifier = "") const;
    QuantLib::Real getCloseOutAggregationScenarioData(
        const ext::shared_ptr<ore::analytics::AggregationScenarioData>& data,
        const ore::analytics::AggregationScenarioDataType& dataType,
        QuantLib::Size dateIdx, QuantLib::Size sampleIdx, const std::string& qualifier = "") const;
    QuantLib::Size getMporCalendarDays(const ext::shared_ptr<ore::analytics::NPVCube>& cube,
                                       QuantLib::Size dateIdx) const;
};

} // namespace analytics
} // namespace ore

#endif
