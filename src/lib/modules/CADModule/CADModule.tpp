/**
 * @file CADModule.cpp
 *
 * @author Ulrich Loup
 * @since 2012-01-19
 * @version 2013-07-10
 */

#include "../../solver/Manager.h"
#include "CADModule.h"

#include <memory>
#include <iostream>

#include "carl/core/logging.h"

#include "MISGeneration.h"

using carl::UnivariatePolynomial;
using carl::cad::EliminationSet;
using carl::cad::Constraint;
using carl::Polynomial;
using carl::CAD;
using carl::RealAlgebraicPoint;
using carl::cad::ConflictGraph;

using namespace std;

// CAD settings
//#define SMTRAT_CAD_GENERIC_SETTING
#define SMTRAT_CAD_DISABLE_PROJECTIONORDEROPTIMIZATION

namespace smtrat
{

	template<typename Settings>
	CADModule<Settings>::CADModule(ModuleType _type, const ModuleInput* _formula, RuntimeSettings*, Conditionals& _conditionals, Manager* const _manager):
		Module(_type, _formula, _conditionals, _manager),
		mCAD(_conditionals),
		mConstraints(),
		hasFalse(false),
		subformulaQueue(),
		mConstraintsMap(),
		mRealAlgebraicSolution(),
		mConflictGraph(),
		mVariableBounds()
#ifdef SMTRAT_DEVOPTION_Statistics
		,mStats(new CADStatistics())
#endif
	{
		mInfeasibleSubsets.clear();	// initially everything is satisfied
		// CAD setting
		carl::cad::CADSettings setting = mCAD.getSetting();
		// general setting set
		setting = carl::cad::CADSettings::getSettings(carl::cad::CADSettingsType::BOUNDED); // standard
		setting.simplifyByFactorization = true;
		setting.simplifyByRootcounting  = true;
		setting.splitInteger = false;
		setting.integerHandling = Settings::integerHandling;

		#ifdef SMTRAT_CAD_DISABLE_MIS
			setting.computeConflictGraph = false;
		#else
			setting.computeConflictGraph = true;
		#endif

		setting.trimVariables = false; // maintains the dimension important for the constraint checking
//		setting.autoSeparateEquations = false; // <- @TODO: find a correct implementation of the MIS for the only-strict or only-equations optimizations

		#ifndef SMTRAT_CAD_DISABLE_PROJECTIONORDEROPTIMIZATION
		// variable order optimization
		std::forward_list<symbol> variables = std::forward_list<symbol>( );
		GiNaC::symtab allVariables = mpReceivedFormula->constraintPool().realVariables();
		for( GiNaC::symtab::const_iterator i = allVariables.begin(); i != allVariables.end(); ++i )
			variables.push_front( GiNaC::ex_to<symbol>( i->second ) );
		std::forward_list<Poly> polynomials = std::forward_list<Poly>( );
		for( fcs_const_iterator i = mpReceivedFormula->constraintPool().begin(); i != mpReceivedFormula->constraintPool().end(); ++i )
			polynomials.push_front( (*i)->lhs() );
		mCAD = CAD( {}, CAD::orderVariablesGreeedily( variables.begin(), variables.end(), polynomials.begin(), polynomials.end() ), _conditionals, setting );
		#ifdef MODULE_VERBOSE
		cout << "Optimizing CAD variable order from ";
		for( forward_list<GiNaC::symbol>::const_iterator k = variables.begin(); k != variables.end(); ++k )
			cout << *k << " ";
		cout << "  to   ";
		for( vector<GiNaC::symbol>::const_iterator k = mCAD.variablesScheduled().begin(); k != mCAD.variablesScheduled().end(); ++k )
			cout << *k << " ";
		cout << endl;;
		#endif
		#else
		mCAD.alterSetting(setting);
		#endif

		SMTRAT_LOG_TRACE("smtrat.cad", "Initial CAD setting:" << std::endl << setting);
		#ifdef SMTRAT_CAD_GENERIC_SETTING
		SMTRAT_LOG_TRACE("smtrat.cad", "SMTRAT_CAD_GENERIC_SETTING set");
		#endif
		#ifdef SMTRAT_CAD_DISABLE_PROJECTIONORDEROPTIMIZATION
		SMTRAT_LOG_TRACE("smtrat.cad", "SMTRAT_CAD_DISABLE_PROJECTIONORDEROPTIMIZATION set");
		#endif
		#ifdef SMTRAT_CAD_DISABLE_MIS
		SMTRAT_LOG_TRACE("smtrat.cad", "SMTRAT_CAD_DISABLE_MIS set");
		#endif
	}

	template<typename Settings>
	CADModule<Settings>::~CADModule(){}

	/**
	 * This method just adds the respective constraint of the subformula, which ought to be one real constraint,
	 * to the local list of constraints. Moreover, the list of all variables is updated accordingly.
	 *
	 * Note that the CAD object is not touched here, the respective calls to CAD::addPolynomial and CAD::check happen in isConsistent.
	 * @param _subformula
	 * @return returns false if the current list of constraints was already found to be unsatisfiable (in this case, nothing is done), returns true previous result if the constraint was already checked for consistency before, otherwise true
	 */
	template<typename Settings>
	bool CADModule<Settings>::addCore(ModuleInput::const_iterator _subformula)
	{
		SMTRAT_LOG_FUNC("smtrat.cad", _subformula->formula());
		switch (_subformula->formula().getType()) {
        case carl::FormulaType::TRUE: 
			return true;
        case carl::FormulaType::FALSE: {
			this->hasFalse = true;
			FormulaSetT infSubSet;
			infSubSet.insert(_subformula->formula());
			mInfeasibleSubsets.push_back(infSubSet);
			return false;
        }
        case carl::FormulaType::CONSTRAINT: {
			SMTRAT_LOG_FUNC("smtrat.cad", _subformula->formula().constraint());
			if (this->hasFalse) {
				this->subformulaQueue.push_back(_subformula->formula());
				return false;
			} else {
				return this->addConstraintFormula(_subformula->formula());
			}
        }
        default:
			SMTRAT_LOG_ERROR("smtrat.cad", "Asserted " << _subformula->formula());
			assert(false);
			return true;
		}
	}

	/**
	 * All constraints asserted (and not removed)  so far are now added to the CAD object and checked for consistency.
	 * If the result is false, a minimal infeasible subset of the original constraint set is computed.
	 * Otherwise a sample value is available.
	 * @param false, if this module should avoid too expensive procedures and rather return unknown instead.
	 * @return True if consistent, False otherwise
	 */
	template<typename Settings>
	Answer CADModule<Settings>::checkCore( bool _full )
	{
		SMTRAT_LOG_FUNC("smtrat.cad", _full);
		if (!_full) {
			SMTRAT_LOG_WARN("smtrat.cad", "Unknown due to !_full");
			return Unknown;
		}
		
		assert(mConstraints.size() == mConstraintsMap.size());
#ifdef SMTRAT_DEVOPTION_Statistics
		mStats->addCall();
#endif
		if (this->hasFalse) return False;
		else {
			for (const auto& f: this->subformulaQueue) {
				this->addConstraintFormula(f);
			}
			this->subformulaQueue.clear();
		}
		if (!rReceivedFormula().isRealConstraintConjunction() && !rReceivedFormula().isIntegerConstraintConjunction()) {
			SMTRAT_LOG_WARN("smtrat.cad", "Unknown due to invalid constraints");
			return Unknown;
		}
		if (!mInfeasibleSubsets.empty())
			return False; // there was no constraint removed which was in a previously generated infeasible subset
		// check the extended constraints for satisfiability
		mCAD.prepareElimination();

		if (variableBounds().isConflicting()) {
			mInfeasibleSubsets.push_back(variableBounds().getConflict());
			mRealAlgebraicSolution = carl::RealAlgebraicPoint<smtrat::Rational>();
			return False;
		}
		carl::CAD<smtrat::Rational>::BoundMap boundMap;
		std::map<carl::Variable, carl::Interval<smtrat::Rational>> eiMap = mVariableBounds.getEvalIntervalMap();
		std::vector<carl::Variable> variables = mCAD.getVariables();
		for (unsigned v = 0; v < variables.size(); ++v)
		{
			auto vPos = eiMap.find(variables[v]);
			if (vPos != eiMap.end())
				boundMap[v] = vPos->second;
		}
		carl::cad::Answer status = mCAD.check(mConstraints, mRealAlgebraicSolution, mConflictGraph, boundMap, false, true);
		if (anAnswerFound()) return Unknown;
		if (status == carl::cad::Answer::False) {
			
			cad::MISGeneration<Settings::mis_heuristic> mis;
			mis(*this, mInfeasibleSubsets);
			//std::cout << "Infeasible Subset: " << *mInfeasibleSubsets.begin() << std::endl;
			//std::cout << "From " << constraints() << std::endl;

			if (Settings::checkMISForMinimality) {
				Module::checkInfSubsetForMinimality(mInfeasibleSubsets.begin());
			}
			mRealAlgebraicSolution = carl::RealAlgebraicPoint<smtrat::Rational>();
			return False;
		}
		if (status == carl::cad::Answer::Unknown) {
			// Pass on branch from CAD.
			const std::vector<carl::Variable>& vars = mCAD.getVariables();
			std::size_t rasid = mRealAlgebraicSolution.dim() - 1;
			std::size_t d = vars.size() - mRealAlgebraicSolution.dim();
			assert(vars[d].getType() == carl::VariableType::VT_INT);
			auto r = mRealAlgebraicSolution[rasid]->branchingPoint();
			assert(!carl::isInteger(r));
			SMTRAT_LOG_DEBUG("smtrat.cad", "Variables: " << vars);
			SMTRAT_LOG_DEBUG("smtrat.cad", "Branching at " << vars[d] << " = " << r);
			branchAt(vars[d], r);
			return Unknown;
		}
		SMTRAT_LOG_TRACE("smtrat.cad", "#Samples: " << mCAD.samples().size());
		SMTRAT_LOG_TRACE("smtrat.cad", "Elimination sets:");
		for (unsigned i = 0; i != mCAD.getEliminationSets().size(); ++i) {
			SMTRAT_LOG_TRACE("smtrat.cad", "\tLevel " << i << " (" << mCAD.getEliminationSet(i).size() << "): " << mCAD.getEliminationSet(i));
		}
		SMTRAT_LOG_TRACE("smtrat.cad", "Result: true");
		SMTRAT_LOG_TRACE("smtrat.cad", "CAD complete: " << mCAD.isComplete());
		SMTRAT_LOG_TRACE("smtrat.cad", "Solution point: " << mRealAlgebraicSolution);
		mInfeasibleSubsets.clear();
		if (Settings::integerHandling == carl::cad::IntegerHandling::SPLIT_SOLUTION) {
			// Check whether the found assignment is integer.
			const std::vector<carl::Variable>& vars = mCAD.getVariables();
			for (unsigned d = 0; d < this->mRealAlgebraicSolution.dim(); d++) {
				if (vars[d].getType() != carl::VariableType::VT_INT) continue;
				auto r = this->mRealAlgebraicSolution[d]->branchingPoint();
				if (!carl::isInteger(r)) {
					branchAt(vars[d], r);
					return Unknown;
				}
			}
		} else if (Settings::integerHandling == carl::cad::IntegerHandling::SPLIT_SOLUTION_INVERSE) {
			// Check whether the found assignment is integer.
			const std::vector<carl::Variable>& vars = mCAD.getVariables();
			for (std::size_t d = this->mRealAlgebraicSolution.dim(); d > 0; d--) {
				if (vars[d-1].getType() != carl::VariableType::VT_INT) continue;
				auto r = this->mRealAlgebraicSolution[d-1]->branchingPoint();
				if (!carl::isInteger(r)) {
					branchAt(vars[d-1], r);
					return Unknown;
				}
			}
		} else {
			const std::vector<carl::Variable>& vars = mCAD.getVariables();
			for (std::size_t d = 0; d < this->mRealAlgebraicSolution.dim(); d++) {
				if (vars[d].getType() != carl::VariableType::VT_INT) continue;
				auto r = this->mRealAlgebraicSolution[d]->branchingPoint();
				assert(carl::isInteger(r));
			}
		}
		return True;
	}

	template<typename Settings>
	void CADModule<Settings>::removeCore(ModuleInput::const_iterator _subformula)
	{
		SMTRAT_LOG_FUNC("smtrat.cad", _subformula->formula());
		switch (_subformula->formula().getType()) {
        case carl::FormulaType::TRUE:
			return;
        case carl::FormulaType::FALSE:
			this->hasFalse = false;
			return;
        case carl::FormulaType::CONSTRAINT: {
			SMTRAT_LOG_FUNC("smtrat.cad", _subformula->formula().constraint());
			auto it = std::find(this->subformulaQueue.begin(), this->subformulaQueue.end(), _subformula->formula());
			if (it != this->subformulaQueue.end()) {
				this->subformulaQueue.erase(it);
				return;
			}

			mVariableBounds.removeBound(_subformula->formula().constraint(), _subformula->formula());

			ConstraintIndexMap::iterator constraintIt = mConstraintsMap.find(_subformula->formula());
			if (constraintIt == mConstraintsMap.end())
				return; // there is nothing to remove
			const carl::cad::Constraint<smtrat::Rational>& constraint = mConstraints[constraintIt->second];

			SMTRAT_LOG_TRACE("smtrat.cad", "---- Constraint removal (before) ----");
			SMTRAT_LOG_TRACE("smtrat.cad", "Elimination sets:");
			for (unsigned i = 0; i != mCAD.getEliminationSets().size(); ++i) {
				SMTRAT_LOG_TRACE("smtrat.cad", "\tLevel " << i << " (" << mCAD.getEliminationSet(i).size() << "): " << mCAD.getEliminationSet(i));
			}
			SMTRAT_LOG_TRACE("smtrat.cad", "#Samples: " << mCAD.samples().size());
			SMTRAT_LOG_TRACE("smtrat.cad", "-----------------------------------------");
			SMTRAT_LOG_TRACE("smtrat.cad", "Removing " << constraint << "...");

			unsigned constraintIndex = constraintIt->second;
			// remove the constraint in mConstraintsMap
			mConstraintsMap.erase(constraintIt);
			// update the constraint / index map, i.e., decrement all indices above the removed one
			updateConstraintMap(constraintIndex, true);
			// remove the corresponding constraint node with index constraintIndex
			mConflictGraph.removeConstraint(constraint);

			// remove the corresponding polynomial from the CAD if it is not occurring in another constraint
			bool doDelete = true;
			for (const auto& c: mConstraints) {
				if (constraint.getPolynomial() == c.getPolynomial()) {
					doDelete = false;
					break;
				}
			}
			if (doDelete) {
				// no other constraint claims the polynomial, hence remove it from the list and the cad
				mCAD.removePolynomial(constraint.getPolynomial());
			}	

			// remove the constraint from the list of constraints
			assert(mConstraints.size() > constraintIndex); // the constraint to be removed should be stored in the local constraint list
			mConstraints.erase(mConstraints.begin() + constraintIndex);	// erase the (constraintIt->second)-th element
			
			SMTRAT_LOG_TRACE("smtrat.cad", "---- Constraint removal (afterwards) ----");
			SMTRAT_LOG_TRACE("smtrat.cad", "New constraint set: " << mConstraints);
			SMTRAT_LOG_TRACE("smtrat.cad", "Elimination sets:");
			for (unsigned i = 0; i != mCAD.getEliminationSets().size(); ++i) {
				SMTRAT_LOG_TRACE("smtrat.cad", "\tLevel " << i << " (" << mCAD.getEliminationSet(i).size() << "): " << mCAD.getEliminationSet(i));
			}
			SMTRAT_LOG_TRACE("smtrat.cad", "#Samples: " << mCAD.samples().size());
			SMTRAT_LOG_TRACE("smtrat.cad", "-----------------------------------------");
			return;
		}
		default:
			return;
		}
	}

	/**
	 * Updates the model.
	 */
	template<typename Settings>
	void CADModule<Settings>::updateModel() const
	{
		SMTRAT_LOG_FUNC("smtrat.cad", "");
		clearModel();
		if (this->solverState() == True) {
			// bound-independent part of the model
			std::vector<carl::Variable> vars(mCAD.getVariables());
			for (unsigned varID = 0; varID < vars.size(); ++varID) {
				ModelValue ass = mRealAlgebraicSolution[varID];
				mModel.insert(std::make_pair(vars[varID], ass));
			}
			// bounds for variables which were not handled in the solution point
			for (auto b: mVariableBounds.getIntervalMap()) {
				// add an assignment for every bound of a variable not in vars (Caution! Destroys vars!)
				std::vector<carl::Variable>::iterator v = std::find(vars.begin(), vars.end(), b.first);
				if (v != vars.end()) {
					vars.erase(v); // shall never be found again
				} else {
					// variable not handled by CAD, use the midpoint of the bounding interval for the assignment
					ModelValue ass = b.second.center();
					mModel.insert(std::make_pair(b.first, ass));
				}
			}
		}
	}

	///////////////////////
	// Auxiliary methods //
	///////////////////////
	template<typename Settings>
	bool CADModule<Settings>::addConstraintFormula(const FormulaT& f) {
		assert(f.getType() == carl::FormulaType::CONSTRAINT);
		mVariableBounds.addBound(f.constraint(), f);
		// add the constraint to the local list of constraints and memorize the index/constraint assignment if the constraint is not present already
		if (mConstraintsMap.find(f) != mConstraintsMap.end())
			return true;	// the exact constraint was already considered
		carl::cad::Constraint<smtrat::Rational> constraint = convertConstraint(f.constraint());
		mConstraints.push_back(constraint);
		mConstraintsMap[f] = (unsigned)(mConstraints.size() - 1);
		mCFMap[constraint] = f;
		mCAD.addPolynomial(typename Poly::PolyType(constraint.getPolynomial()), constraint.getVariables());

		return solverState() != False;
	}

	/**
	 * Converts the constraint types.
	 * @param c constraint of the SMT-RAT
	 * @return constraint of GiNaCRA
	 */
	template<typename Settings>
	inline const carl::cad::Constraint<smtrat::Rational> CADModule<Settings>::convertConstraint( const smtrat::ConstraintT& c )
	{
		// convert the constraints variable
		std::vector<carl::Variable> variables;
		for (auto i: c.variables()) {
			variables.push_back(i);
		}
		carl::Sign signForConstraint = carl::Sign::ZERO;
		bool cadConstraintNegated = false;
		switch (c.relation()) {
			case carl::Relation::EQ: 
				break;
			case carl::Relation::LEQ:
				signForConstraint	= carl::Sign::POSITIVE;
				cadConstraintNegated = true;
				break;
			case carl::Relation::GEQ:
				signForConstraint	= carl::Sign::NEGATIVE;
				cadConstraintNegated = true;
				break;
			case carl::Relation::LESS:
				signForConstraint = carl::Sign::NEGATIVE;
				break;
			case carl::Relation::GREATER:
				signForConstraint = carl::Sign::POSITIVE;
				break;
			case carl::Relation::NEQ:
				cadConstraintNegated = true;
				break;
			default: assert(false);
		}
		return carl::cad::Constraint<smtrat::Rational>((typename Poly::PolyType)c.lhs(), signForConstraint, variables, cadConstraintNegated);
	}

	/**
	 * Converts the constraint types.
	 * @param c constraint of the GiNaCRA
	 * @return constraint of SMT-RAT
	 */
	template<typename Settings>
	inline ConstraintT CADModule<Settings>::convertConstraint( const carl::cad::Constraint<smtrat::Rational>& c )
	{
		carl::Relation relation = carl::Relation::EQ;
		switch (c.getSign()) {
			case carl::Sign::POSITIVE:
				if (c.isNegated()) relation = carl::Relation::LEQ;
				else relation = carl::Relation::GREATER;
				break;
			case carl::Sign::ZERO:
				if (c.isNegated()) relation = carl::Relation::NEQ;
				else relation = carl::Relation::EQ;
				break;
			case carl::Sign::NEGATIVE:
				if (c.isNegated()) relation = carl::Relation::GEQ;
				else relation = carl::Relation::LESS;
				break;
			default: assert(false);
		}
		return ConstraintT(c.getPolynomial(), relation);
	}

	/**
	 *
	 * @param index
	 * @return
	 */
	template<typename Settings>
	inline const FormulaT& CADModule<Settings>::getConstraintAt(unsigned index) {
		SMTRAT_LOG_TRACE("smtrat.cad", "get " << index << " from " << mConstraintsMap);
                // @todo: Use some other map here.
		for (auto& i: mConstraintsMap) {
			if (i.second == index) // found the entry in the constraint map
				return i.first;
		}
		assert(false);	// The given index should match an input constraint!
		return mConstraintsMap.begin()->first;
	}

	/**
	 * Increment all indices stored in the constraint map being greater than the given index; decrement if decrement is true.
	 * @param index
	 * @param decrement
	 */
	template<typename Settings>
	inline void CADModule<Settings>::updateConstraintMap(unsigned index, bool decrement) {
		SMTRAT_LOG_TRACE("smtrat.cad", "updating " << index << " from " << mConstraintsMap);
		if (decrement) {
			for (auto& i: mConstraintsMap) {
				if (i.second > index) i.second--;
			}
		} else {
			for (auto& i: mConstraintsMap) {
				if (i.second > index) i.second++;
			}
		}
		SMTRAT_LOG_TRACE("smtrat.cad", "now: " << mConstraintsMap);
	}

}	// namespace smtrat
