/**
 * @file   smtratSolverTool.h
 *         Created on April 10, 2013, 3:37 PM
 * @author: Sebastian Junges
 *
 *
 */

#pragma once

#include "Tool.h"

#include "../utils/Execute.h"
#include "../utils/regex.h"

namespace benchmax {

class SMTRAT: public Tool {
public:
	SMTRAT(const fs::path& binary, const std::string& arguments): Tool("SMTRAT", binary, arguments) {
		std::string output;
		callProgram(binary.native() + " --settings", output);
		auto options = parseOptions(output);
		mAttributes.insert(options.begin(), options.end());
	}

	virtual bool canHandle(const fs::path& path) const override {
		return isExtension(path, ".smt2");
	}
	
	std::map<std::string,std::string> parseOptions(const std::string& options) const {
		std::map<std::string,std::string> res;
		regex r("^(.+) = (.+)\n");
		auto begin = regex_iterator(options.begin(), options.end(), r);
		auto end = regex_iterator();
		for (auto i = begin; i != end; ++i) {
			res[(*i)[1]] = (*i)[2];
		}
		return res;
	}
};

}
