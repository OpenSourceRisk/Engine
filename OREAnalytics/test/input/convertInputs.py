# Intended to be used only for SIMM 2.5 and greater

import argparse
import pandas as pd
import os
import logging
from abc import ABC, abstractmethod
from pathlib import Path
from datetime import date
import enum

file_path = Path(__file__).parent

class SimmVersion(enum.Enum):
	V1_3 = '1.3'
	V1_3_38 = '1.3.38'
	V2_0 = '2.0'
	V2_1 = '2.1'
	V2_2 = '2.2'
	V2_3 = '2.3'
	V2_3_8 = '2.3.8'
	V2_5 = '2.5'
	V2_5A = '2.5A'

def get_simm_version_converter(output_dir, pp_dir, unit_test_path, calibration_dir, simm_version):
	version_specific_converters = {
		'1.3': SimmConverter_1_3,
		'1.3.38': SimmConverter_1_3_38,
		'2.0': SimmConverter_2_0,
		'2.1': SimmConverter_2_1,
		'2.2': SimmConverter_2_2,
		'2.3': SimmConverter_2_3,
		'2.3.8': SimmConverter_2_3_8,
		'2.5': SimmConverter_2_5,
		'2.5A': SimmConverter_2_5A
	}

	return version_specific_converters[simm_version](output_dir, pp_dir, unit_test_path, calibration_dir)

class IsdaSimmUnitTestsConverter(ABC):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir, simm_version):
		self._output_dir = output_dir
		self._pp_dir = pp_dir
		self._unit_test_path = unit_test_path
		self._calibration_dir = calibration_dir
		self._simm_version = simm_version
		self._delimiter = '|'
		self._outputs = dict()
		self._sensitivity_inputs = dict()
		self._sensitivity_combinations = dict()
		self._calibration = dict()

		self._output_dir.mkdir(parents=True, exist_ok=True)

	def read_excel_unit_tests_file(self):
		unit_test_file = pd.read_excel(self._unit_test_path, sheet_name=None)
		unit_test_file = {k.strip(): df for k, df in unit_test_file.items()}

		# Read sensitivity inputs
		self.read_sensitivity_inputs(unit_test_file)
		self._outputs.update(self._sensitivity_inputs)

		# Read sensitivity combinations
		self.read_sensitivity_combinations(unit_test_file)
		self._outputs.update(self._sensitivity_combinations)

	def write_out_test_inputs(self):
		for output_filename, df in self._outputs.items():
			output_path = self._output_dir / (output_filename + '.csv')
			logging.info(f'Writing out to file {output_path!r}')
			df.to_csv(output_path, sep=self._delimiter, index=False)

	def read_sensitivity_inputs(self, unit_test_file):
		sensitivity_inputs = unit_test_file['Sensitivity Inputs']
		crif_headers = self.sensitivity_inputs_headers()

		begin_reading = False
		sensitivity_inputs_df = pd.DataFrame(columns=crif_headers)
		for index, row in sensitivity_inputs.iterrows():
			if all(row.isna()):
				continue

			try:
				if all(row == crif_headers):
					begin_reading = True
			except:
				pass

			try:
				if any(row == crif_headers):
					continue
			except:
				pass

			if begin_reading:
				row_df = pd.DataFrame(row.values, index=crif_headers).transpose()
				sensitivity_inputs_df = pd.concat([sensitivity_inputs_df, row_df])

		sensitivity_inputs_df.reset_index(drop=True, inplace=True)

		self._sensitivity_inputs['sensitivity_inputs'] = sensitivity_inputs_df
		self.sensitivity_inputs_hardcoded_corrections()

	def sensitivity_combinations_headers(self):
		return ['Combination Id', 'Group', 'Risk Measure', 'Element of Calculation Tested', 'Sensitivity Ids',
						'Passes Required', 'SIMM Delta', 'SIMM Vega', 'SIMM Curvature', 'SIMM Base Corr', 'SIMM AddOn',
						'SIMM Benchmark']

	def ignore_sensitivity_combinations(self, mpor_days: list, combinations_to_ignore: list):
		for sc_string in ['sensitivity_combinations_' + d for d in mpor_days]:
			sc_df = self._sensitivity_combinations[sc_string]
			self._sensitivity_combinations[sc_string] = sc_df[~sc_df['Combination Id'].isin(combinations_to_ignore)]

	@abstractmethod
	def read_sensitivity_combinations(self, unit_test_file):
		pass

	@abstractmethod
	def sensitivity_inputs_headers(self):
		pass

	@abstractmethod
	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	@abstractmethod
	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def read_calibration_files(self):
		calibration_files = self._calibration_dir.iterdir()

		for pth in calibration_files:
			tokens = str(pth).split('-')
			if len(tokens) != 3:
				raise BaseException('Expected 3 tokens')

			mpor_days = tokens[1]
			calibration_df = pd.read_csv(pth, sep='\t')
			self._calibration[mpor_days] = calibration_df

	def write_calibration_files(self):
		self.write_simm_concentration_files()
		self.write_simm_configuration_files()

	def write_simm_concentration_files(self):
		calibration_df = self._calibration['10d']

		generic_intro = \
		"/*\n" \
		f" Copyright (C) {date.today().year} Quaternion Risk Management Ltd\n" \
		" All rights reserved.\n" \
		"*/\n"

		hpp_file_description =\
		"/*! \\file orepsimm/orea/simmconcentrationisdav" + self._simm_version.name[1:] + ".hpp\n"\
		"    \\brief SIMM concentration thresholds for SIMM version " + self._simm_version.value + "\n"\
		"*/\n"\
		"\n"

		hpp_includes =\
		"#pragma once\n"\
		"\n"\
		"#include <orepsimm/orea/simmconcentration.hpp>\n"\
		"\n"\
		"#include <map>\n"\
		"#include <orepsimm/orea/simmbucketmapper.hpp>\n"\
		"#include <set>\n"\
		"#include <string>\n"\
		"\n"

		hpp_body =\
		"namespace oreplus {\n"\
		"namespace analytics {\n"\
		"\n"\
		"/*! Class giving the SIMM concentration thresholds as outlined in the document\n"\
		f"    <em>ISDA SIMM Methodology, version {self._simm_version.value} .\n"\
		"        Effective Date: December 3, 2022.</em>\n"\
		"*/\n"\
		f"class SimmConcentration_ISDA_{self._simm_version.name} : public SimmConcentrationBase {{\n"\
		"public:\n"\
		"    //! Default constructor that adds fixed known mappings\n"\
		f"    SimmConcentration_ISDA_{self._simm_version.name}(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper);\n"\
		"\n"\
		"    /*! Return the SIMM <em>concentration threshold</em> for a given SIMM\n"\
		"        <em>RiskType</em> and SIMM <em>Qualifier</em>.\n"\
		"\n"\
		"        \\warning If the risk type is not covered <code>QL_MAX_REAL</code> is\n"\
		"                 returned i.e. no concentration threshold\n"\
		"     */\n"\
		"    QuantLib::Real threshold(const SimmConfiguration::RiskType& riskType, const std::string& qualifier) const override;\n"\
		"\n"\
		"private:\n"\
		"    //! Help getting SIMM buckets from SIMM qualifiers\n"\
		"    boost::shared_ptr<SimmBucketMapper> simmBucketMapper_;\n"\
		"};\n"\
		"\n"\
		"} // namespace analytics\n"\
		"} // namespace oreplus\n"

		cpp_includes =\
		f"#include <orepsimm/orea/simmconcentrationisdav{self._simm_version.name[1:]}.hpp>\n"\
		"#include <ored/utilities/log.hpp>\n"\
		"#include <ql/errors.hpp>\n"\
		"\n"

		cpp_body =\
		"using namespace QuantLib;\n"\
		"\n"\
		"using std::map;\n"\
		"using std::set;\n"\
		"using std::string;\n"\
		"\n"\
		"namespace oreplus {\n"\
		"namespace analytics {\n"\
		"\n"\
		"// Ease syntax\n"\
		"using RiskType = SimmConfiguration::RiskType;\n"\
		"\n"\
		f"SimmConcentration_ISDA_{self._simm_version.name}::SimmConcentration_ISDA_{self._simm_version.name}(const " \
		f"boost::shared_ptr<SimmBucketMapper>& simmBucketMapper)\n"\
		"    : simmBucketMapper_(simmBucketMapper) {\n"\
		"\n"

		# Currency categories for IR/FX concentration thresholds
		for pc in ['IR', 'FX']:
			cpp_body += f"    // Populate {pc} categories that are used for concentration thresholds\n"
			conc_ccy_df = calibration_df[calibration_df['RiskType']==f'Info_{pc}_Conc_CurrencyList']
			conc_ccy_dict = {}
			for data in conc_ccy_df.itertuples():
				if data.Bucket in conc_ccy_dict:
					conc_ccy_dict[data.Bucket].append(data.Parameter)
				else:
					if data.Parameter == 'Other':
						conc_ccy_dict[data.Bucket] = []
					else:
						conc_ccy_dict[data.Bucket] = [data.Parameter]

			cpp_body += f"    {pc.lower()}Categories_ = {{"
			for bucket, ccy_list in conc_ccy_dict.items():
				cpp_body += f'{{"{bucket}", ' + str(conc_ccy_dict[bucket]).replace('\'', '\"').replace('[', '{').replace(
					']', '}') + '},'
			cpp_body = cpp_body[:-1] + "};\n\n"

		cpp_body +=\
		"    // Initialise the data\n"\
		"    // clang-format off\n"\
		"\n"\
		"    // Populate flat thresholds\n"\
		f"    flatThresholds_[RiskType::CreditVol] = {calibration_df[calibration_df['RiskType'] == 'Calib_CreditVol_Conc']['Parameter'].values[0]};\n"\
		f"    flatThresholds_[RiskType::CreditVolNonQ] = " \
		f"{calibration_df[calibration_df['RiskType'] == 'Calib_CreditVolNonQ_Conc']['Parameter'].values[0]};\n"\
		"\n"

		# Bucketed thresholds
		cpp_body += "    // Populate bucketed thresholds\n"
		for rt in ['IRCurve', 'CreditQ', 'CreditNonQ', 'Equity', 'Commodity', 'FX', 'IRVol', 'EquityVol', 'CommodityVol',
							 'FXVol']:
			# Calibration doc has this listed differently
			def special_bucketing(label1: str, label2: str):
				if (label1, label2) == ('1','1'):
					th_bucket = '1'
				elif (label1, label2) == ('1','2'):
					th_bucket = '2'
				elif (label1, label2) == ('1', '3'):
					th_bucket = '3'
				elif (label1, label2) == ('2', '2'):
					th_bucket = '4'
				elif (label1, label2) == ('2', '3'):
					th_bucket = '5'
				elif (label1, label2) == ('3', '3'):
					th_bucket = '6'
				else:
					th_bucket = None

				return th_bucket

			if rt == 'FXVol':
				th_bucket = special_bucketing(data.Label1, data.Label2)
			else:
				th_bucket = data.Bucket

			thresholds_df = calibration_df[calibration_df['RiskType']==f'Calib_{rt}_Conc']
			thresholds_dict = {}
			for data in thresholds_df.itertuples():
				if rt == 'FXVol':
					th_bucket = special_bucketing(data.Label1, data.Label2)
					if th_bucket is None:
						continue
				else:
					th_bucket = data.Bucket

				thresholds_dict[th_bucket] = data.Parameter

			cpp_body += f"    bucketedThresholds_[RiskType::{rt}] = {{\n"
			for bucket, threshold in thresholds_dict.items():
				cpp_body += f'        {{ "{bucket}", {threshold} }},\n'
			cpp_body = cpp_body[:-2] + '\n'
			cpp_body +=\
			"    };\n"\
			"\n"

		cpp_body +=\
		"    // clang-format on\n"\
		"}\n"\
		"\n"\
		f"Real SimmConcentration_ISDA_{self._simm_version.name}::threshold(const RiskType& riskType, const string& qualifier) "\
		f"const {{\n"\
		"    return thresholdImpl(simmBucketMapper_, riskType, qualifier);\n"\
		"}\n"\
		"\n"\
		"} // namespace analytics\n"\
		"} // namespace oreplus\n"\

		simm_conc_path = self._pp_dir / f"simmconcentrationisda{self._simm_version.name}.hpp"
		logging.info(f'Writing out to file {simm_conc_path!r}')
		with open(simm_conc_path, 'w') as simm_conc_hpp:
			simm_conc_hpp.write(generic_intro)
			simm_conc_hpp.write(hpp_file_description)
			simm_conc_hpp.write(hpp_includes)
			simm_conc_hpp.write(hpp_body)

		with open(self._pp_dir / f"simmconcentrationisda{self._simm_version.name}.cpp", 'w') as simm_conc_cpp:
			simm_conc_cpp.write(generic_intro)
			simm_conc_cpp.write(cpp_includes)
			simm_conc_cpp.write(cpp_body)

	def write_simm_configuration_files(self):
		def df_get(df = None, rt = None, bkt = None, lbl1 = None, lbl2 = None):

			if 'Corr' in rt and lbl1 == lbl2 and lbl1 is not None and rt != 'Calib_FX_Corr':
				return '1.00'

			res_df = df
			if rt is not None:
				res_df = res_df[res_df['RiskType'] == rt]
			if bkt is not None:
				res_df = res_df[res_df['Bucket']==bkt]
			if lbl1 is not None:
				res_df = res_df[res_df['Label1'] == str(lbl1)]
			if lbl2 is not None:
				res_df = res_df[res_df['Label2'] == str(lbl2)]

			return res_df if len(res_df) > 1 else res_df.Parameter.values[0]

		generic_intro = \
			"/*\n" \
			f" Copyright (C) {date.today().year} Quaternion Risk Management Ltd.\n" \
			" All rights reserved.\n" \
			"*/\n" \
			"\n"

		hpp_file_description =\
		"/*! \\file orepsimm/orea/simmconfigurationisdav" + self._simm_version.name[1:] + ".hpp\n"\
		"    \\brief SIMM configuration for SIMM version " + self._simm_version.value + "\n"\
		"*/\n"\
		"\n"

		hpp_includes =\
		"#pragma once\n"\
		"\n"\
		"#include <orepsimm/orea/simmconfigurationbase.hpp>\n"\
		"\n"

		hpp_body =\
		f"namespace oreplus {{\n"\
		f"namespace analytics {{\n"\
		"\n"\
		"/*! Class giving the SIMM configuration as outlined in the document\n"\
		f"    <em>ISDA SIMM Methodology, version {self._simm_version.value}.\n"\
		"        Effective Date: <INSERT EFFECTIVE DATE>.</em>\n"\
		"*/\n"\
		f"class SimmConfiguration_ISDA_{self._simm_version.name} : public SimmConfigurationBase {{\n"\
		"public:\n"\
		f"    SimmConfiguration_ISDA_{self._simm_version.name}(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,\n"\
		"                                const QuantLib::Size& mporDays = 10,\n"\
		f"                                const std::string& name = \"SIMM ISDA {self._simm_version.value} (<INSERT PUBLISHING DATE HERE>)\",\n"\
		f"                                const std::string version = \"{self._simm_version.value}\");\n"\
		"\n"\
		"    //! Return the SIMM <em>Label2</em> value for the given interest rate index\n"\
		"    std::string labels2(const boost::shared_ptr<QuantLib::InterestRateIndex>& irIndex) const override;\n"\
		"\n"\
		"    //! Add SIMM <em>Label2</em> values under certain circumstances.\n"\
		"    void addLabels2(const RiskType& rt, const std::string& label_2) override;\n"\
		"\n"\
		"    QuantLib::Real curvatureMarginScaling() const override;\n"\
		"\n"\
		"    QuantLib::Real weight(const RiskType& rt, boost::optional<std::string> qualifier = boost::none,\n"\
		"                          boost::optional<std::string> label_1 = boost::none,\n"\
		"                          const std::string& calculationCurrency = \"\") const override;\n"\
		"\n"\
		"    QuantLib::Real correlation(const RiskType& firstRt, const std::string& firstQualifier,\n"\
		"                               const std::string& firstLabel_1, const std::string& firstLabel_2,\n"\
		"                               const RiskType& secondRt, const std::string& secondQualifier,\n"\
		"                               const std::string& secondLabel_1, const std::string& secondLabel_2,\n"\
		"                               const std::string& calculationCurrency = \"\") const override;\n"\
		"\n"\
		"private:\n"\
		"    //! Find the group of the \\p qualifier\n"\
		"    QuantLib::Size group(const std::string& qualifier,\n"\
		"                         const std::map<QuantLib::Size, std::set<std::string>>& groups) const;\n"\
		"\n"\
		"    /*! Map giving a currency's FX Volatility group (High or Regular). This concept\n"\
		"        was introduced in ISDA Simm 2.2\n"\
		"     */\n"\
		"    std::map<QuantLib::Size, std::set<std::string>> ccyGroups_;\n"\
		"\n"\
		"    //! FX risk weight matrix\n"\
		"    QuantLib::Matrix rwFX_;\n"\
		"\n"\
		"    //! FX Correlations when the calculation ccy is in the Regular Volatility group\n"\
		"    QuantLib::Matrix fxRegVolCorrelation_;\n"\
		"\n"\
		"    //! FX Correlations when the calculation ccy is in the High Volatility group\n"\
		"    QuantLib::Matrix fxHighVolCorrelation_;\n"\
		"\n"\
		"    //! IR Historical volatility ratio\n"\
		"    QuantLib::Real hvr_ir_;\n"\
		"};\n"\
		"\n"\
		"} // namespace analytics\n"\
		"} // namespace oreplus\n"

		cpp_includes =\
		f"#include <orepsimm/orea/simmconcentrationisda{self._simm_version.name}.hpp>\n" \
		f"#include <orepsimm/orea/simmconfigurationisda{self._simm_version.name}.hpp>\n" \
		"#include <ql/math/matrix.hpp>\n"\
		"\n"\
		"#include <boost/algorithm/string/predicate.hpp>\n" \
		"#include <boost/make_shared.hpp>\n" \
		"\n"

		cpp_body =\
		"using QuantLib::InterestRateIndex;\n"\
		"using QuantLib::Matrix;\n"\
		"using QuantLib::Real;\n"\
		"using std::string;\n"\
		"using std::vector;\n"\
		"\n"\
		"namespace oreplus {\n"\
		"namespace analytics {\n"\
		"\n"\
		f"QuantLib::Size SimmConfiguration_ISDA_{self._simm_version.name}::group(const string& qualifier, const std::map<QuantLib::Size,\n"\
		"                                                  std::set<string>>& categories) const {\n"\
		"    QuantLib::Size result = 0;\n"\
		"    for (const auto& kv : categories) {\n"\
		"        if (kv.second.empty()) {\n"\
		"            result = kv.first;\n"\
		"        } else {\n"\
		"            if (kv.second.count(qualifier) > 0) {\n"\
		"                // Found qualifier in category so return it\n"\
		"                return kv.first;\n"\
		"            }\n"\
		"        }\n"\
		"    }\n"\
		"\n"\
		"    // If we get here, result should hold category with empty set\n"\
		"    return result;\n"\
		"}\n"\
		"\n"\
		f"QuantLib::Real SimmConfiguration_ISDA_{self._simm_version.name}::weight(const RiskType& rt, boost::optional<string> qualifier,\n"\
		"                                                     boost::optional<std::string> label_1,\n"\
		"                                                     const std::string& calculationCurrency) const {\n"\
		"\n"\
		"    if (rt == RiskType::FX) {\n"\
		"        QL_REQUIRE(calculationCurrency != \"\", \"no calculation currency provided weight\");\n"\
		"        QL_REQUIRE(qualifier, \"need a qualifier to return a risk weight for the risk type FX\");\n"\
		"\n"\
		"        QuantLib::Size g1 = group(calculationCurrency, ccyGroups_);\n"\
		"        QuantLib::Size g2 = group(*qualifier, ccyGroups_);\n"\
		"        return rwFX_[g1][g2];\n"\
		"    }\n"\
		"\n"\
		"    return SimmConfigurationBase::weight(rt, qualifier, label_1);\n"\
		"}\n"\
		"\n"\
		f"QuantLib::Real SimmConfiguration_ISDA_{self._simm_version.name}::correlation(const RiskType& firstRt, const string& firstQualifier,\n"\
		"                                                        const string& firstLabel_1, const string& firstLabel_2,\n"\
		"                                                        const RiskType& secondRt, const string& secondQualifier,\n"\
		"                                                        const string& secondLabel_1, const string& secondLabel_2,\n"\
		"                                                        const std::string& calculationCurrency) const {\n"\
		"\n"\
		"    if (firstRt == RiskType::FX && secondRt == RiskType::FX) {\n"\
		"        QL_REQUIRE(calculationCurrency != \"\", \"no calculation currency provided corr\");\n"\
		"        QuantLib::Size g = group(calculationCurrency, ccyGroups_);\n"\
		"        QuantLib::Size g1 = group(firstQualifier, ccyGroups_);\n"\
		"        QuantLib::Size g2 = group(secondQualifier, ccyGroups_);\n"\
		"        if (g == 0) {\n"\
		"            return fxRegVolCorrelation_[g1][g2];\n"\
		"        } else if (g == 1) {\n"\
		"            return fxHighVolCorrelation_[g1][g2];\n"\
		"        } else {\n"\
		"            QL_FAIL(\"FX Volatility group \" << g << \" not recognized\");\n"\
		"        }\n"\
		"    }\n"\
		"\n"\
		"    return SimmConfigurationBase::correlation(firstRt, firstQualifier, firstLabel_1, firstLabel_2, secondRt,\n"\
		"                                              secondQualifier, secondLabel_1, secondLabel_2);\n"\
		"}\n"\
		"\n"\
		f"SimmConfiguration_ISDA_{self._simm_version.name}::SimmConfiguration_ISDA_{self._simm_version.name}(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,\n"\
		"                                                         const QuantLib::Size& mporDays, const std::string& name,\n"\
		"                                                         const std::string version)\n"\
		"     : SimmConfigurationBase(simmBucketMapper, name, version, mporDays) {\n"\
		"\n"\
		"    // The differences in methodology for 1 Day horizon is described in\n"\
		"    // Standard Initial Margin Model: Technical Paper, ISDA SIMM Governance Forum, Version 10:\n"\
		"    // Section I - Calibration with one-day horizon\n"\
		"    QL_REQUIRE(mporDays_ == 10 || mporDays_ == 1, \"SIMM only supports MPOR 10-day or 1-day\");\n"\
		"\n"\
		"    // Set up the correct concentration threshold getter\n"\
		"    if (mporDays == 10) {\n"\
		f"        simmConcentration_ = boost::make_shared<SimmConcentration_ISDA_{self._simm_version.name}>(simmBucketMapper_);\n"\
		"    } else {\n"\
		"        // SIMM:Technical Paper, Section I.4: \"The Concentration Risk feature is disabled\"\n"\
		"        simmConcentration_ = boost::make_shared<SimmConcentrationBase>();\n"\
		"    }\n"\
		"\n"\
		"    // clang-format off\n"\
		"\n"\
		"    // Set up the members for this configuration\n"\
		"    // Explanations of all these members are given in the hpp file\n"\
		"\n"\
		"    mapBuckets_ = {\n"\
		"        { RiskType::IRCurve, { \"1\", \"2\", \"3\" } },\n"\
		"        { RiskType::CreditQ, { \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"Residual\" } },\n"\
		"        { RiskType::CreditVol, { \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"Residual\" } },\n"\
		"        { RiskType::CreditNonQ, { \"1\", \"2\", \"Residual\" } },\n"\
		"        { RiskType::CreditVolNonQ, { \"1\", \"2\", \"Residual\" } },\n"\
		"        { RiskType::Equity, { \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"Residual\" } },\n"\
		"        { RiskType::EquityVol, { \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"Residual\" } },\n"\
		"        { RiskType::Commodity, { \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"13\", \"14\", \"15\", \"16\", \"17\" } },\n"\
		"        { RiskType::CommodityVol, { \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"13\", \"14\", \"15\", \"16\", \"17\" } }\n"\
		"    };\n"\
		"\n"\
		"    mapLabels_1_ = {\n"\
		"        { RiskType::IRCurve, { \"2w\", \"1m\", \"3m\", \"6m\", \"1y\", \"2y\", \"3y\", \"5y\", \"10y\", \"15y\", \"20y\", \"30y\" } },\n"\
		"        { RiskType::CreditQ, { \"1y\", \"2y\", \"3y\", \"5y\", \"10y\" } },\n"\
		"        { RiskType::CreditNonQ, { \"1y\", \"2y\", \"3y\", \"5y\", \"10y\" } },\n"\
		"        { RiskType::IRVol, { \"2w\", \"1m\", \"3m\", \"6m\", \"1y\", \"2y\", \"3y\", \"5y\", \"10y\", \"15y\", \"20y\", \"30y\" } },\n"\
		"        { RiskType::InflationVol, { \"2w\", \"1m\", \"3m\", \"6m\", \"1y\", \"2y\", \"3y\", \"5y\", \"10y\", \"15y\", \"20y\", \"30y\" } },\n"\
		"        { RiskType::CreditVol, { \"1y\", \"2y\", \"3y\", \"5y\", \"10y\" } },\n"\
		"        { RiskType::CreditVolNonQ, { \"1y\", \"2y\", \"3y\", \"5y\", \"10y\" } },\n"\
		"        { RiskType::EquityVol, { \"2w\", \"1m\", \"3m\", \"6m\", \"1y\", \"2y\", \"3y\", \"5y\", \"10y\", \"15y\", \"20y\", \"30y\" } },\n"\
		"        { RiskType::CommodityVol, { \"2w\", \"1m\", \"3m\", \"6m\", \"1y\", \"2y\", \"3y\", \"5y\", \"10y\", \"15y\", \"20y\", \"30y\" } },\n"\
		"        { RiskType::FXVol, { \"2w\", \"1m\", \"3m\", \"6m\", \"1y\", \"2y\", \"3y\", \"5y\", \"10y\", \"15y\", \"20y\", \"30y\" } }\n"\
		"    };\n"\
		"\n"\
		"    mapLabels_2_ = {\n"\
		"        { RiskType::IRCurve, { \"OIS\", \"Libor1m\", \"Libor3m\", \"Libor6m\", \"Libor12m\", \"Prime\", \"Municipal\" } },\n"\
		"        { RiskType::CreditQ, { \"\", \"Sec\" } }\n"\
		"    };\n"\
		"\n"\
		"    // Populate CCY groups that are used for FX correlations and risk weights\n"\
		"    // The groups consists of High Vol Currencies & regular vol currencies\n"\

		fx_groups_df = df_get(self._calibration['10d'], 'Info_FX_CurrencyList')
		fx_groups_dict = {}
		for data in fx_groups_df.itertuples():
			bucket = '0' if data.Bucket == '2' else data.Bucket
			if bucket in fx_groups_dict:
				fx_groups_dict[bucket].append(data.Parameter)
			else:
				if data.Parameter=='Other':
					fx_groups_dict[bucket] = []
				else:
					fx_groups_dict[bucket] = [data.Parameter]

		fx_groups_dict = {k: sorted(v) for k, v in fx_groups_dict.items()}
		fx_groups_dict = dict(sorted(fx_groups_dict.items(), reverse=True))
		cpp_body += "    ccyGroups_ = {\n"
		for bucket, ccy_list in fx_groups_dict.items():
			cpp_body += f'        {{ {bucket}, ' + str(fx_groups_dict[bucket]).replace('\'', '\"').replace('[', '{ ').replace(
				']', ' }') + ' },\n'
		cpp_body = cpp_body[:-2] + "\n    };\n\n"

		config_mpor_str = '10d'
		cpp_body +=\
		"    vector<Real> temp;\n"\
		"\n"\
		"    if (mporDays_ == 10) {\n"\
		"        // Risk weights\n"\
		"        temp = {\n"\
		f"           {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 2, 2)},  {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 1, 2)},\n"\
		f"           {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 2, 1)}, {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 1, 1)}\n"\
		"        };\n"\
		"        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());\n"\
		"\n"\
		"        rwRiskType_ = {\n"

		for rt in ['Inflation', 'XCcyBasis', 'IRVol', 'InflationVol', 'CreditVol', 'CreditVolNonQ', 'CommodityVol', 'FXVol', 'BaseCorr']:
			if rt == 'InflationVol':
				val = df_get(self._calibration[config_mpor_str], 'Calib_IRVol_Weight')
			else:
				val = df_get(self._calibration[config_mpor_str], f'Calib_{rt}_Weight')
			cpp_body += f"            {{ RiskType::{rt}, {val} }},\n"
		cpp_body = cpp_body[:-2] + "\n        };\n\n"

		cpp_body +=\
		"        rwBucket_ = {\n"
		for rt in ['CreditQ', 'CreditNonQ', 'Equity', 'Commodity', 'EquityVol']:
			rw_bucket_df = df_get(self._calibration[config_mpor_str], f'Calib_{rt}_Weight').Parameter
			rw_bucket_list = str([v for v in rw_bucket_df.values]).replace('\'', '').replace('[', '{ ').replace(']', ' }')
			cpp_body +=\
			f"            {{ RiskType::{rt}, {rw_bucket_list} }},\n"
		cpp_body += "        };\n\n"
		cpp_body += "        rwLabel_1_ = {\n"
		rw_label1_df = df_get(self._calibration[config_mpor_str], 'Calib_IRCurve_Weight')
		rw_label1_dict = {}
		for data in rw_label1_df.itertuples():
			if data.Bucket in rw_label1_dict:
				rw_label1_dict[data.Bucket].append(data.Parameter)
			else:
				rw_label1_dict[data.Bucket] = [data.Parameter]
		for bucket, lst in rw_label1_dict.items():
			cpp_body +=\
			"            { { RiskType::IRCurve, \"" + bucket + "\" }, " + str(lst).replace('\'', '').replace('[', '{ ').replace(']', ' }') + " },\n"
		cpp_body += "        };\n\n"\
		"        // Historical volatility ratios\n"\
		f"        historicalVolatilityRatios_[RiskType::EquityVol] = {df_get(self._calibration[config_mpor_str], 'Calib_EquityVol_HVR')};\n"\
		f"        historicalVolatilityRatios_[RiskType::CommodityVol] = {df_get(self._calibration[config_mpor_str], 'Calib_CommodityVol_HVR')};\n"\
		f"        historicalVolatilityRatios_[RiskType::FXVol] = {df_get(self._calibration[config_mpor_str], 'Calib_FXVol_HVR')};\n"\
		f"        hvr_ir_ = {df_get(self._calibration[config_mpor_str], 'Calib_IRVol_HVR')};\n"\
		"\n"\
		"        // Curvature weights\n"\
		"        curvatureWeights_ = {\n"\
		"            { RiskType::IRVol, { 0.5,\n"\
		"                                 0.5 * 14.0 / (365.0 / 12.0),\n"\
		"                                 0.5 * 14.0 / (3.0 * 365.0 / 12.0),\n"\
		"                                 0.5 * 14.0 / (6.0 * 365.0 / 12.0),\n"\
		"                                 0.5 * 14.0 / 365.0,\n"\
		"                                 0.5 * 14.0 / (2.0 * 365.0),\n"\
		"                                 0.5 * 14.0 / (3.0 * 365.0),\n"\
		"                                 0.5 * 14.0 / (5.0 * 365.0),\n"\
		"                                 0.5 * 14.0 / (10.0 * 365.0),\n"\
		"                                 0.5 * 14.0 / (15.0 * 365.0),\n"\
		"                                 0.5 * 14.0 / (20.0 * 365.0),\n"\
		"                                 0.5 * 14.0 / (30.0 * 365.0) }\n"\
		"            },\n"\
		"            { RiskType::CreditVol, { 0.5 * 14.0 / 365.0,\n"\
		"                                     0.5 * 14.0 / (2.0 * 365.0),\n"\
		"                                     0.5 * 14.0 / (3.0 * 365.0),\n"\
		"                                     0.5 * 14.0 / (5.0 * 365.0),\n"\
		"                                     0.5 * 14.0 / (10.0 * 365.0) }\n"\
		"            }\n"\
		"        };\n"\
		"        curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];\n"\
		"\n"\
		"    } else {\n"\
		"       // SIMM:Technical Paper, Section I.1: \"All delta and vega risk weights should be replaced with the values for\n"\
		"       // one-day calibration given in the Calibration Results document.\"\n"\
		"\n"

		config_mpor_str = '1d'
		cpp_body +=\
		"        // Risk weights\n"\
		"        temp = {\n"\
		f"           {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 2, 2)},  {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 1, 2)},\n"\
		f"           {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 2, 1)}, {df_get(self._calibration[config_mpor_str], 'Calib_FX_Weight', None, 1, 1)}\n"\
		"        };\n"\
		"        rwFX_ = Matrix(2, 2, temp.begin(), temp.end());\n"\
		"\n"\
		"        rwRiskType_ = {\n"

		for rt in ['Inflation', 'XCcyBasis', 'IRVol', 'InflationVol', 'CreditVol', 'CreditVolNonQ', 'CommodityVol', 'FXVol', 'BaseCorr']:
			if rt == 'InflationVol':
				val = df_get(self._calibration[config_mpor_str], 'Calib_IRVol_Weight')
			else:
				val = df_get(self._calibration[config_mpor_str], f'Calib_{rt}_Weight')
			cpp_body += f"            {{ RiskType::{rt}, {val} }},\n"
		cpp_body = cpp_body[:-2] + "\n        };\n\n"

		cpp_body +=\
		"        rwBucket_ = {\n"
		for rt in ['CreditQ', 'CreditNonQ', 'Equity', 'Commodity', 'EquityVol']:
			rw_bucket_df = df_get(self._calibration[config_mpor_str], f'Calib_{rt}_Weight').Parameter
			rw_bucket_list = str([v for v in rw_bucket_df.values]).replace('\'', '').replace('[', '{ ').replace(']', ' }')
			cpp_body +=\
			f"            {{ RiskType::{rt}, {rw_bucket_list} }},\n"
		cpp_body += "        };\n\n"
		cpp_body += "        rwLabel_1_ = {\n"
		rw_label1_df = df_get(self._calibration[config_mpor_str], 'Calib_IRCurve_Weight')
		rw_label1_dict = {}
		for data in rw_label1_df.itertuples():
			if data.Bucket in rw_label1_dict:
				rw_label1_dict[data.Bucket].append(data.Parameter)
			else:
				rw_label1_dict[data.Bucket] = [data.Parameter]
		for bucket, lst in rw_label1_dict.items():
			cpp_body +=\
			"            { { RiskType::IRCurve, \"" + bucket + "\" }, " + str(lst).replace('\'', '').replace('[', '{ ').replace(']', ' }') + " },\n"
		cpp_body += "        };\n\n"\
		"        // Historical volatility ratios\n"\
		f"        historicalVolatilityRatios_[RiskType::EquityVol] = {df_get(self._calibration[config_mpor_str], 'Calib_EquityVol_HVR')};\n"\
		f"        historicalVolatilityRatios_[RiskType::CommodityVol] = {df_get(self._calibration[config_mpor_str], 'Calib_CommodityVol_HVR')};\n"\
		f"        historicalVolatilityRatios_[RiskType::FXVol] = {df_get(self._calibration[config_mpor_str], 'Calib_FXVol_HVR')};\n"\
		f"        hvr_ir_ = {df_get(self._calibration[config_mpor_str], 'Calib_IRVol_HVR')};\n"\
		"\n"\
		"        // Curvature weights\n"\
		"        //SIMM:Technical Paper, Section I.3, this 10-day formula for curvature weights is modified\n"\
		"        curvatureWeights_ = {\n"\
		"            { RiskType::IRVol, { 0.5 / 10.0,\n"\
		"                                 0.5 * 1.40 / (365.0 / 12.0),\n"\
		"                                 0.5 * 1.40 / (3.0 * 365.0 / 12.0),\n"\
		"                                 0.5 * 1.40 / (6.0 * 365.0 / 12.0),\n"\
		"                                 0.5 * 1.40 / 365.0,\n"\
		"                                 0.5 * 1.40 / (2.0 * 365.0),\n"\
		"                                 0.5 * 1.40 / (3.0 * 365.0),\n"\
		"                                 0.5 * 1.40 / (5.0 * 365.0),\n"\
		"                                 0.5 * 1.40 / (10.0 * 365.0),\n"\
		"                                 0.5 * 1.40 / (15.0 * 365.0),\n"\
		"                                 0.5 * 1.40 / (20.0 * 365.0),\n"\
		"                                 0.5 * 1.40 / (30.0 * 365.0) }\n"\
		"            },\n"\
		"            { RiskType::CreditVol, { 0.5 * 1.40 / 365.0,\n"\
		"                                     0.5 * 1.40 / (2.0 * 365.0),\n"\
		"                                     0.5 * 1.40 / (3.0 * 365.0),\n"\
		"                                     0.5 * 1.40 / (5.0 * 365.0),\n"\
		"                                     0.5 * 1.40 / (10.0 * 365.0) }\n"\
		"            }\n"\
		"        };\n"\
		"        curvatureWeights_[RiskType::InflationVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::EquityVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::CommodityVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::FXVol] = curvatureWeights_[RiskType::IRVol];\n"\
		"        curvatureWeights_[RiskType::CreditVolNonQ] = curvatureWeights_[RiskType::CreditVol];\n"\
		"    }\n"\
		"\n"\
		"\n"\
		"    // Valid risk types\n"\
		"    validRiskTypes_ = {\n"\
		"        RiskType::Commodity,\n"\
		"        RiskType::CommodityVol,\n"\
		"        RiskType::CreditNonQ,\n"\
		"        RiskType::CreditQ,\n"\
		"        RiskType::CreditVol,\n"\
		"        RiskType::CreditVolNonQ,\n"\
		"        RiskType::Equity,\n"\
		"        RiskType::EquityVol,\n"\
		"        RiskType::FX,\n"\
		"        RiskType::FXVol,\n"\
		"        RiskType::Inflation,\n"\
		"        RiskType::IRCurve,\n"\
		"        RiskType::IRVol,\n"\
		"        RiskType::InflationVol,\n"\
		"        RiskType::BaseCorr,\n"\
		"        RiskType::XCcyBasis,\n"\
		"        RiskType::ProductClassMultiplier,\n"\
		"        RiskType::AddOnNotionalFactor,\n"\
		"        RiskType::PV,\n"\
		"        RiskType::Notional,\n"\
		"        RiskType::AddOnFixedAmount\n"\
		"    };\n"\
		"\n"\
		"    // Risk class correlation matrix\n"

		cpp_body += "    temp = {\n"
		risk_class_list = ['IR', 'CreditQ', 'CreditNonQ', 'Equity', 'Commodity', 'FX']
		for rc1 in risk_class_list:
			cpp_body += "       "
			for rc2 in risk_class_list:
				cpp_body += " " + df_get(self._calibration['10d'], 'Calib_RiskClass_Corr', None, rc1, rc2) + ","
			cpp_body += '\n'

		cpp_body = cpp_body[:-2] + '\n'
		cpp_body +=\
		"    };\n"\
		"    riskClassCorrelation_ = Matrix(6, 6, temp.begin(), temp.end());\n"\
		"\n"\
		"    // FX correlations\n"\
		"    temp = {\n"\
		f"        {df_get(self._calibration['10d'], 'Calib_FX_Corr', '2', '2', '2')}, {df_get(self._calibration['10d'], 'Calib_FX_Corr', '2', '1', '2')},\n"\
		f"        {df_get(self._calibration['10d'], 'Calib_FX_Corr', '2', '1', '2')}, {df_get(self._calibration['10d'], 'Calib_FX_Corr', '2', '1', '1')}\n"\
		"    };\n"\
		"    fxRegVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());\n"\
		"\n"\
		"    temp = {\n"\
		f"        {df_get(self._calibration['10d'], 'Calib_FX_Corr', '1', '2', '2')}, {df_get(self._calibration['10d'], 'Calib_FX_Corr', '1', '2', '1')},\n"\
		f"        {df_get(self._calibration['10d'], 'Calib_FX_Corr', '1', '1', '2')}, {df_get(self._calibration['10d'], 'Calib_FX_Corr', '1', '1', '1')}\n"\
		"    };\n"\
		"    fxHighVolCorrelation_ = Matrix(2, 2, temp.begin(), temp.end());\n"\
		"\n"\
		"    // Interest rate tenor correlations (i.e. Label1 level correlations)\n"\
		"    temp = {\n"
		ir_tenor_list = ['2W', '1M', '3M', '6M', '1Y', '2Y', '3Y', '5Y', '10Y', '15Y', '20Y', '30Y']
		for t1 in ir_tenor_list:
			cpp_body += "       "
			for t2 in ir_tenor_list:
				cpp_body += " " + str(round(float(df_get(self._calibration['10d'], 'Calib_IRCurve_Corr', None, t1, t2)), 2)) + ","
			cpp_body += '\n'

		cpp_body = cpp_body[:-2] + '\n'
		cpp_body += \
		"    };\n" \
		"    irTenorCorrelation_ = Matrix(12, 12, temp.begin(), temp.end());\n"\
		"\n"\
		"    // CreditQ inter-bucket correlations\n"\
		"     temp = {\n"
		credit_bucket_list = [str(i) for i in range(1, 13)]
		for t1 in credit_bucket_list:
			cpp_body += "       "
			for t2 in credit_bucket_list:
				cpp_body += " " + str(round(float(df_get(self._calibration['10d'], 'Calib_CreditQ_CorrOuter', None, t1, t2)), 2)) + ","
			cpp_body += '\n'

		cpp_body = cpp_body[:-2] + '\n'
		cpp_body += \
			"    };\n" \
		"    interBucketCorrelation_[RiskType::CreditQ] = Matrix(12, 12, temp.begin(), temp.end());\n"\
		"\n"\
		"     // Equity inter-bucket correlations\n"\
		"     temp = {\n"
		equity_bucket_list = [str(i) for i in range(1, 13)]
		for t1 in equity_bucket_list:
			cpp_body += "       "
			for t2 in equity_bucket_list:
				cpp_body += " " + str(round(float(df_get(self._calibration['10d'], 'Calib_Equity_CorrOuter', None, t1, t2)), 2)) + ","
			cpp_body += '\n'

		cpp_body = cpp_body[:-2] + '\n'
		cpp_body += \
			"    };\n" \
		"    interBucketCorrelation_[RiskType::Equity] = Matrix(12, 12, temp.begin(), temp.end());\n"\
		"\n"\
		"    // Commodity inter-bucket correlations\n"\
		"    temp = {\n"
		commodity_bucket_list = [str(i) for i in range(1, 18)]
		for t1 in commodity_bucket_list:
			cpp_body += "       "
			for t2 in commodity_bucket_list:
				cpp_body += " " + str(round(float(df_get(self._calibration['10d'], 'Calib_Commodity_CorrOuter', None, t1, t2)), 2)) + ","
			cpp_body += '\n'

		cpp_body = cpp_body[:-2] + '\n'
		cpp_body += \
			"    };\n" \
		"    interBucketCorrelation_[RiskType::Commodity] = Matrix(17, 17, temp.begin(), temp.end());\n"\
		"\n"\
		"    // Equity intra-bucket correlations (exclude Residual and deal with it in the method - it is 0%) - changed\n"
		equity_intrabucket_corrs = [df_get(self._calibration['10d'], 'Calib_Equity_Corr', str(i)) for i in range(1, 13)]
		equity_intrabucket_corrs = str(equity_intrabucket_corrs).replace('[', '{ ').replace(']', ' }').replace('\'', '')
		cpp_body += f"   intraBucketCorrelation_[RiskType::Equity] = {equity_intrabucket_corrs};\n"\
		"\n"\
		"    // Commodity intra-bucket correlations\n"
		commodity_intrabucket_corrs = [df_get(self._calibration['10d'], 'Calib_Commodity_Corr', str(i)) for i in range(1, 18)]
		commodity_intrabucket_corrs = str(commodity_intrabucket_corrs).replace('[', '{ ').replace(']', ' }').replace('\'', '')
		cpp_body += f"   intraBucketCorrelation_[RiskType::Commodity] = {commodity_intrabucket_corrs};\n"\
		"\n"\
		"    // Initialise the single, ad-hoc type, correlations\n"\
		f"    xccyCorr_ = {df_get(self._calibration['10d'], 'Calib_XCcyBasis_Corr')};\n"\
		f"    infCorr_ = {df_get(self._calibration['10d'], 'Calib_Inflation_Corr')};\n"\
		f"    infVolCorr_ = {df_get(self._calibration['10d'], 'Calib_Inflation_Corr')};\n"\
		f"    irSubCurveCorr_ = {df_get(self._calibration['10d'], 'Calib_IRSubCurve_Corr')};\n"\
		f"    irInterCurrencyCorr_ = {df_get(self._calibration['10d'], 'Calib_IR_CorrOuter')};\n"\
		f"    crqResidualIntraCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditQ_Corr', None, 'Residual', 'Same')};\n"\
		f"    crqSameIntraCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditQ_Corr', None, 'Aggregate', 'Same')};\n"\
		f"    crqDiffIntraCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditQ_Corr', None, 'Aggregate', 'Different')};\n"\
		f"    crnqResidualIntraCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditQ_Corr', None, 'Residual', 'Same')};\n"\
		f"    crnqSameIntraCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditNonQ_Corr', None, 'Aggregate', 'Same')};\n"\
		f"    crnqDiffIntraCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditNonQ_Corr', None, 'Aggregate', 'Different')};\n"\
		f"    crnqInterCorr_ = {df_get(self._calibration['10d'], 'Calib_CreditNonQ_CorrOuter')};\n"\
		f"    fxCorr_ = {df_get(self._calibration['10d'], 'Calib_FXVol_Corr')};\n"\
		f"    basecorrCorr_ = {df_get(self._calibration['10d'], 'Calib_BaseCorr_Corr')};\n"\
		"\n"\
		"    // clang-format on\n"\
		"}\n"\
		"\n"\
		"/* The CurvatureMargin must be multiplied by a scale factor of HVR(IR)^{-2}, where HVR(IR)\n"\
		"is the historical volatility ratio for the interest-rate risk class (see page 8 section 11(d)\n"\
		f"of the ISDA-SIMM-v{self._simm_version.value} documentation).\n"\
		"*/\n"\
		f"QuantLib::Real SimmConfiguration_ISDA_{self._simm_version.name}::curvatureMarginScaling() const {{ return pow(hvr_ir_, -2.0); }}\n"\
		"\n"\
		f"void SimmConfiguration_ISDA_{self._simm_version.name}::addLabels2(const RiskType& rt, const string& label_2) {{\n"\
		"    // Call the shared implementation\n"\
		"    SimmConfigurationBase::addLabels2Impl(rt, label_2);\n"\
		"}\n"\
		"\n"\
		f"string SimmConfiguration_ISDA_{self._simm_version.name}::labels2(const boost::shared_ptr<InterestRateIndex>& irIndex) const {{\n"\
		"    // Special for BMA\n"\
		"    if (boost::algorithm::starts_with(irIndex->name(), \"BMA\")) {\n"\
		"        return \"Municipal\";\n"\
		"    }\n"\
		"\n"\
		"    // Otherwise pass off to base class\n"\
		"    return SimmConfigurationBase::labels2(irIndex);\n"\
		"}\n"\
		"\n"\
		"} // namespace analytics\n"\
		"} // namespace oreplus\n"\


		simm_config_path = self._pp_dir / f"simmconfigurationisda{self._simm_version.name}.hpp"
		logging.info(f'Writing out to file {simm_config_path!r}')
		with open(simm_config_path, 'w') as simm_config_hpp:
			simm_config_hpp.write(generic_intro)
			simm_config_hpp.write(hpp_file_description)
			simm_config_hpp.write(hpp_includes)
			simm_config_hpp.write(hpp_body)

		with open(self._pp_dir / f"simmconfigurationisda{self._simm_version.name}.cpp", 'w') as simm_config_cpp:
			simm_config_cpp.write(generic_intro)
			simm_config_cpp.write(cpp_includes)
			simm_config_cpp.write(cpp_body)

class SimmConverter_1_3(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V1_3)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
		 				'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		self._sensitivity_inputs['sensitivity_inputs'].loc[46, 'Qualifier'] = 'ISIN:AU000000GMG3'
		self._sensitivity_inputs['sensitivity_inputs'].loc[65, 'Qualifier'] = 'Freight Wet'
		self._sensitivity_inputs['sensitivity_inputs'].loc[66, 'Qualifier'] = 'Livestock Live Cattle'
		self._sensitivity_inputs['sensitivity_inputs'].loc[67, 'Qualifier'] = 'Livestock Feeder Cattle'
		self._sensitivity_inputs['sensitivity_inputs'].loc[120, 'Qualifier'] = 'Livestock Lean Hogs'

	def sensitivity_combinations_hardcoded_corrections(self):
		## C127
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[126, 'SIMM AddOn'] = 45814977.7470377
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[126, 'SIMM Benchmark'] = 700314659.847575

		## C128
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[127, 'SIMM AddOn'] = 438314073.526591
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[127, 'SIMM Benchmark'] = 6699943695.33503

		## C156
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[155, 'SIMM AddOn'] = 5941913411.82865
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[155, 'SIMM Benchmark'] = 96338806411.3147

		## C242
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[241, 'Passes Required'] = 'C231-C241'

		## C247
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[246, 'SIMM AddOn'] = 11973465522.9302
		self._sensitivity_combinations['sensitivity_combinations_10'].loc[246, 'SIMM Benchmark'] = 179437221617.488

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Sensitivity Combinations']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d
		self.sensitivity_combinations_hardcoded_corrections()

class SimmConverter_1_3_38(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V1_3_38)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
		 				'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

class SimmConverter_2_0(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_0)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
		 				'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_combinations(self):
		return ['Combination Id', 'Group', 'Risk Measure', 'Element of Calculation Tested', 'Sensitivity Ids',
						'Passes Required', 'SIMM Delta', 'SIMM Vega', 'SIMM Curvature', 'SIMM Base Corr', 'SIMM AddOn',
						'SIMM Benchmark']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Sensitivity Combinations 10d']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Sensitivity Combinations 1d']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.sensitivity_combinations_hardcoded_corrections()


class SimmConverter_2_1(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_1)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
		 				'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		self._sensitivity_inputs['sensitivity_inputs'].loc[181, 'Label1'] = '1y'

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.sensitivity_combinations_hardcoded_corrections()


class SimmConverter_2_2(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_2)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
		 				'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		self._sensitivity_inputs['sensitivity_inputs'].loc[181, 'Label1'] = '1y'

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.sensitivity_combinations_hardcoded_corrections()


class SimmConverter_2_3(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_3)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'CollectRegulations', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
						'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.sensitivity_combinations_hardcoded_corrections()


class SimmConverter_2_3_8(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_3_8)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'CollectRegulations', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
						'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.sensitivity_combinations_hardcoded_corrections()

class SimmConverter_2_5(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_5)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'CollectRegulations', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
						'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_headers(self):
		return ['Combination Id', 'Group', 'Risk Measure', 'Element of Calculation Tested', 'Sensitivity Ids',
						'Passes Required', 'Calculation Mode', 'SIMM Delta', 'SIMM Vega', 'SIMM Curvature', 'SIMM Base Corr',
						'SIMM AddOn', 'SIMM Benchmark']

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.ignore_sensitivity_combinations(mpor_days=['1', '10'], combinations_to_ignore=['JS1', 'JS2'])
		self.sensitivity_combinations_hardcoded_corrections()

class SimmConverter_2_5(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_5)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'CollectRegulations', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
						'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_headers(self):
		return ['Combination Id', 'Group', 'Risk Measure', 'Element of Calculation Tested', 'Sensitivity Ids',
						'Passes Required', 'Calculation Mode', 'SIMM Delta', 'SIMM Vega', 'SIMM Curvature', 'SIMM Base Corr',
						'SIMM AddOn', 'SIMM Benchmark']

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.ignore_sensitivity_combinations(mpor_days=['1', '10'], combinations_to_ignore=['JS1', 'JS2'])
		self.sensitivity_combinations_hardcoded_corrections()

class SimmConverter_2_5A(IsdaSimmUnitTestsConverter):
	def __init__(self, output_dir, pp_dir, unit_test_path, calibration_dir):
		super().__init__(output_dir, pp_dir, unit_test_path, calibration_dir, SimmVersion.V2_5A)

	def sensitivity_inputs_headers(self):
		return ['ProductClass', 'Sensitivity_Id', 'CollectRegulations', 'RiskType', 'Qualifier', 'Bucket', 'Label1',
						'Label2', 'Amount', 'AmountCurrency', 'AmountUSD']

	def sensitivity_inputs_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_hardcoded_corrections(self):
		pass

	def sensitivity_combinations_headers(self):
		return ['Combination Id', 'Group', 'Risk Measure', 'Element of Calculation Tested', 'Sensitivity Ids',
						'Passes Required', 'Calculation Mode', 'SIMM Delta', 'SIMM Vega', 'SIMM Curvature', 'SIMM Base Corr',
						'SIMM AddOn', 'SIMM Benchmark']

	def read_sensitivity_combinations(self, unit_test_file):
		sensitivity_combinations_10d = unit_test_file['Combinations (10-day)']
		sensitivity_combinations_10d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_10'] = sensitivity_combinations_10d

		sensitivity_combinations_1d = unit_test_file['Combinations (1-day)']
		sensitivity_combinations_1d.columns = self.sensitivity_combinations_headers()
		self._sensitivity_combinations['sensitivity_combinations_1'] = sensitivity_combinations_1d
		self.ignore_sensitivity_combinations(mpor_days=['1', '10'], combinations_to_ignore=['JS1', 'JS2'])
		self.sensitivity_combinations_hardcoded_corrections()

def main(args):
	output_dir = args.output_path / args.simm_version

	converter = get_simm_version_converter(
								simm_version=args.simm_version,
								output_dir=args.output_path,
								pp_dir=args.pp_path,
								calibration_dir=args.calibration_dir,
								unit_test_path=args.unit_test
							)
	converter.read_excel_unit_tests_file()
	converter.write_out_test_inputs()
	converter.read_calibration_files()
	converter.write_calibration_files()

if __name__ == '__main__':
	parser = argparse.ArgumentParser()

	parser.add_argument('--unit_test', type=lambda p: Path(p).resolve(), help='Filepath of ISDA unit test Excel file', required=True)
	parser.add_argument('--calibration_dir', type=lambda p: Path(p).resolve(), help='Folder containing calibration files', required=True)
	parser.add_argument('--simm_version', '-v', type=str, help='SIMM version', required=True)

	parser.add_argument('--output_path', '-o', type=lambda p: Path(p).resolve(), default=file_path / 'simmisdaunittests',
											help='File dir to save the unit test files that will be used as inputs for the pricer tests')
	parser.add_argument('--pp_path', type=lambda p:Path(p).resolve(), default=file_path,
											help='File dir to save the resulting cpp files to')
	
	args = parser.parse_args()
	logging.basicConfig(level='INFO')
	main(args)
