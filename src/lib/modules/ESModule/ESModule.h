/**
 * A module, which iteratively finds boolean and arithmetic substitutions and applies them to all formulas
 * which are connected to this substitution by a conjunction.
 * 
 * @file ESModule.h
 * @author Florian Corzilius <corzilius@cs.rwth-aachen.de>
 *
 * @version 2015-09-09
 * Created on 2015-09-09.
 */

#pragma once

#include "../../solver/PModule.h"
#include "ESStatistics.h"
#include "ESSettings.h"
namespace smtrat
{
    template<typename Settings>
    class ESModule : public PModule
    {
        private:
            // Members.
            ///
            std::unordered_map<FormulaT, bool> mBoolSubs;
            ///
            std::map<carl::Variable,Poly> mArithSubs;

        public:
			typedef Settings SettingsType;
			std::string moduleName() const {
				return SettingsType::moduleName;
			}
            ESModule( const ModuleInput* _formula, RuntimeSettings* _settings, Conditionals& _conditionals, Manager* _manager = NULL );

            ~ESModule();

            // Main interfaces.

            /**
             * Updates the current assignment into the model.
             * Note, that this is a unique but possibly symbolic assignment maybe containing newly introduced variables.
             */
            void updateModel() const;

            /**
             * Checks the received formula for consistency.
             * @param _full false, if this module should avoid too expensive procedures and rather return unknown instead.
             * @return True,    if the received formula is satisfiable;
             *         False,   if the received formula is not satisfiable;
             *         Unknown, otherwise.
             */
            Answer checkCore( bool _full );
            
        private:
            /**
			 * Eliminates all equation forming a substitution of the form x = p with p not containing x.
			 */
			FormulaT elimSubstitutions( const FormulaT& _formula, bool _elimSubstitutions = false );

    };
}
