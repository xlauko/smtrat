/**
 * @file CADSettings.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2015-09-10
 * Created on 2015-09-10.
 */

#pragma once

#include <iostream>

#include <carl/cad/CADSettings.h>

#include "../../solver/ModuleSettings.h"

#include "../../utilities/SettingsManager.h"

namespace smtrat
{    
	enum class MISHeuristic {
		TRIVIAL, GREEDY
	};
    
    struct CADSettings1 : ModuleSettings
    {
		static constexpr auto moduleName = "CADModule<CADSettings1>";
        static constexpr carl::cad::IntegerHandling integerHandling = carl::cad::IntegerHandling::SPLIT_SOLUTION;
        
        static constexpr MISHeuristic mis_heuristic = MISHeuristic::GREEDY;
        static constexpr bool computeConflictGraph = (mis_heuristic != MISHeuristic::TRIVIAL);
        static constexpr bool checkMISForMinimality = false;
        
        static const bool dummy;
    };
    
    struct CADSettingsSplitLazy : CADSettings1
    {
		static constexpr auto moduleName = "CADModule<SplitLazy>";
        static constexpr carl::cad::IntegerHandling integerHandling = carl::cad::IntegerHandling::SPLIT_LAZY;
    };
	
	struct CADSettingsGuessAndSplit : CADSettings1
    {
		static constexpr auto moduleName = "CADModule<GuessAndSplit>";
        static constexpr carl::cad::IntegerHandling integerHandling = carl::cad::IntegerHandling::GUESS_AND_SPLIT;
    };
}