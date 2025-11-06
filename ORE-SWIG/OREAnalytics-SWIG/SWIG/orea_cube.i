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

%{
using ore::analytics::NPVCube;
using ore::analytics::JointNPVCube;
using ore::analytics::InMemoryCubeOpt;
using ore::analytics::AggregationScenarioData;
using ore::analytics::AggregationScenarioDataType;
%}

%shared_ptr(NPVCube)
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

enum class AggregationScenarioDataType : unsigned int { IndexFixing = 0, FXSpot = 1, Numeraire = 2, Generic = 3 };

%shared_ptr(AggregationScenarioData)
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

%shared_ptr(InMemoryCubeOpt<float>);
%shared_ptr(InMemoryCubeOpt<double>);
template <typename T> class InMemoryCubeOpt : public NPVCube {
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

%template(SinglePrecisionInMemoryCubeN) InMemoryCubeOpt<float>;
%template(DoublePrecisionInMemoryCubeN) InMemoryCubeOpt<double>;

%shared_ptr(SinglePrecisionInMemoryCubeN);
%shared_ptr(DoublePrecisionInMemoryCubeN);

%template(DoublePrecisionInMemoryCubeNVector) std::vector<ext::shared_ptr<InMemoryCubeOpt<double>>>;

%shared_ptr(JointNPVCube)
class JointNPVCube : public NPVCube {
public:
    %extend {
    JointNPVCube(
        const ext::shared_ptr<NPVCube>& cube1, const ext::shared_ptr<NPVCube>& cube2,
        const std::set<std::string>& ids = {}, const bool requireUniqueIds = true) {
            return new JointNPVCube(cube1, cube2, ids, requireUniqueIds); 
        }

    JointNPVCube(
        const std::vector<ext::shared_ptr<NPVCube>>& cubes, const std::set<std::string>& ids = {},
        const bool requireUniqueIds = true){
            return new JointNPVCube(cubes, ids, requireUniqueIds);
        }

        JointNPVCube(const ext::shared_ptr<InMemoryCubeOpt<double>>& cube1,
                     const ext::shared_ptr<InMemoryCubeOpt<double>>& cube2, const std::set<std::string>& ids = {},
                     const bool requireUniqueIds = true) {
            return new JointNPVCube(cube1, cube2, ids, requireUniqueIds);
        }

        JointNPVCube(const std::vector<ext::shared_ptr<InMemoryCubeOpt<double>>>& cubes,
                     const std::set<std::string>& ids = {}, const bool requireUniqueIds = true) {
            std::vector<ext::shared_ptr<NPVCube>> npvCubes(cubes.begin(), cubes.end());
            return new JointNPVCube(npvCubes, ids, requireUniqueIds);
        }

        JointNPVCube(const ext::shared_ptr<InMemoryCubeOpt<float>>& cube1,
                     const ext::shared_ptr<InMemoryCubeOpt<float>>& cube2, const std::set<std::string>& ids = {},
                     const bool requireUniqueIds = true) {
            return new JointNPVCube(cube1, cube2, ids, requireUniqueIds);
        }

        JointNPVCube(const std::vector<ext::shared_ptr<InMemoryCubeOpt<float>>>& cubes,
                     const std::set<std::string>& ids = {}, const bool requireUniqueIds = true) {
            std::vector<ext::shared_ptr<NPVCube>> npvCubes(cubes.begin(), cubes.end());
            return new JointNPVCube(npvCubes, ids, requireUniqueIds);
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

#endif
