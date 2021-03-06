#pragma once

#include <carl/util/Singleton.h>

#include <any>
#include <cassert>
#include <map>
#include <string>

namespace smtrat {
namespace settings {

struct CoreSettings {
	bool show_help;
	bool show_info;
	bool show_version;
	bool show_settings;
	bool show_cmake_options;
	bool show_license;
};

struct SolverSettings {
	bool print_model;
	bool print_all_models;
	bool preprocess;
	std::string preprocess_output_file;
	bool print_statistics;
	bool print_strategy;
	bool print_timings;
};

struct ValidationSettings {
	bool log_lemmata;
	bool log_theory_calls;
	bool log_infeasible_subsets;
	std::string log_filename;
};

struct Settings: public carl::Singleton<Settings> {
	friend carl::Singleton<Settings>;
private:
	std::map<std::string,std::any> mSettings = {
		{"core", CoreSettings()},
		{"solver", SolverSettings{}},
		{"validation", ValidationSettings{}}
	};
	Settings() = default;
public:
	CoreSettings core;
	SolverSettings solver;
	ValidationSettings validation;

	template<typename T>
	T& get(const std::string& name) {
		auto it = mSettings.find(name);
		assert(it != mSettings.end());
		return std::any_cast<T&>(it->second);
	}
	template<typename T>
	T& add(const std::string& name) {
		auto res = mSettings.emplace(name, T{});
		assert(res.second);
		return std::any_cast<T&>(res.first->second);
	}
};

}

inline const settings::Settings& Settings() {
	return settings::Settings::getInstance();
}

inline const auto& settings_core() {
	static const auto& s = settings::Settings::getInstance().get<settings::CoreSettings>("core");
	return s;
}
inline const auto& settings_solver() {
	static const auto& s = settings::Settings::getInstance().get<settings::SolverSettings>("solver");
	return s;
}
inline const auto& settings_validation() {
	static const auto& s = settings::Settings::getInstance().get<settings::ValidationSettings>("validation");
	return s;
}

}