/**
 * @file Manager.cpp
 *
 * @author  Florian Corzilius
 * @author  Ulrich Loup
 * @author  Sebastian Junges
 * @author  Henrik Schmitz
 * @since   2012-01-18
 * @version 2013-01-11
 */

#include "Manager.h"
#include "StrategyGraph.h"
#include <functional>

#include <typeinfo>

using namespace std;

namespace smtrat
{
    
    // Constructor.
    
    Manager::Manager():
#ifdef __VS
        mPrimaryBackendFoundAnswer(smtrat::Conditionals(1, new std::atomic<bool>(false))),
#else
        mPrimaryBackendFoundAnswer(smtrat::Conditionals(1, new std::atomic_bool(false))),
#endif
        mpPassedFormula(new ModuleInput()),
        mBacktrackPoints(),
        mGeneratedModules(),
        mBackendsOfModules(),
        mpPrimaryBackend( new Module( mpPassedFormula, mPrimaryBackendFoundAnswer, this ) ),
        mStrategyGraph(),
        mDebugOutputChannel( cout.rdbuf() ),
        mLogic( Logic::UNDEFINED ),
        mInformationRelevantFormula(),
        mLemmaLevel(LemmaLevel::NONE),
        mObjectives()
        #ifdef SMTRAT_DEVOPTION_Statistics
        ,
        mpStatistics( new GeneralStatistics() )
        #endif
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        ,
        mpThreadPool( nullptr ),
        mNumberOfBranches( 0 ),
        mNumberOfCores( 1 ),
        mRunsParallel( false )
        #endif
    {
        mGeneratedModules.push_back( mpPrimaryBackend );
        // inform it about all constraints
        typedef void (*Func)( Module*, const FormulaT& );
        Func f = [] ( Module* _module, const FormulaT& _constraint ) { _module->inform( _constraint ); };
        carl::FormulaPool<Poly>::getInstance().forallDo<Module>( f, mpPrimaryBackend );
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        initialize();
        #endif
    }

    // Destructor.
    
    Manager::~Manager()
    {
        Module::storeAssumptionsToCheck( *this );
        #ifdef SMTRAT_DEVOPTION_Statistics
        delete mpStatistics;
        #endif
        while( !mGeneratedModules.empty() )
        {
            Module* ptsmodule = mGeneratedModules.back();
            mGeneratedModules.pop_back();
            delete ptsmodule;
        }
        while( !mPrimaryBackendFoundAnswer.empty() )
        {
#ifdef __VS
            std::atomic<bool>* toDelete = mPrimaryBackendFoundAnswer.back();
#else
            std::atomic_bool* toDelete = mPrimaryBackendFoundAnswer.back();
#endif
            mPrimaryBackendFoundAnswer.pop_back();
            delete toDelete;
        }
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        if( mpThreadPool != nullptr )
            delete mpThreadPool;
        #endif
        delete mpPassedFormula;
    }
    
    bool Manager::inform( const FormulaT& _constraint )
    {
        return mpPrimaryBackend->inform( _constraint );
    }
    
    bool Manager::add( const FormulaT& _subformula )
    {
        if( _subformula.getType() == carl::FormulaType::CONSTRAINT )
            mpPrimaryBackend->inform( _subformula );
        else if( _subformula.isNary() )
        {
            vector<FormulaT> constraints;
            _subformula.getConstraints( constraints );
            for( auto& c : constraints )
                mpPrimaryBackend->inform( c );
        }
        auto res = mpPassedFormula->add( _subformula );
        if( res.second )
        {
			bool r = true;
			for (auto it = res.first; it != mpPassedFormula->end(); it++) {
				r = r && mpPrimaryBackend->add( it );
			}
			return r;
        }
        return true;
    }
    
    Answer Manager::check( bool _full )
    {
        *mPrimaryBackendFoundAnswer.back() = false;
        mpPassedFormula->updateProperties();
        if( mObjectives.empty() )
            return mpPrimaryBackend->check( _full );
        push(); // In this level we collect the upper bounds for the minimum of each objective function.
        for( auto obVarIter = mObjectives.begin(); ; )
        {
            assert( obVarIter != mObjectives.end() );
            push(); // In this level we store the equation between the objective function and it's introduced variable.
            add( FormulaT( (obVarIter->second.second ? obVarIter->first : -(obVarIter->first)) - obVarIter->second.first, carl::Relation::EQ ) );
            mpPrimaryBackend->setObjective( obVarIter->second.first );
            Answer result = mpPrimaryBackend->check( _full, true );
            if( result != True )
            {
                pop( 2 );
                return result;
            }
            ++obVarIter;
            if( obVarIter != mObjectives.end() )
            {
                const Model& primModel = model();
                auto objModel = primModel.find( obVarIter->second.first );
                assert( objModel != primModel.end() );
                assert( objModel->second.isRational() ); // Non-linear optimization not yet supported.
                FormulaT minimumUpperBound( (obVarIter->second.second ? obVarIter->first : -(obVarIter->first)) - objModel->second.asRational(), carl::Relation::LESS );
                pop(); // Remove the equation between the objective function and it's introduced variable.
                add( minimumUpperBound );
            }
            else
            {
                pop( 2 );
                return result;
            }
        }
        
    }
    
    const std::vector<FormulaSetT>& Manager::infeasibleSubsets() const
    {
        return mpPrimaryBackend->infeasibleSubsets();
    }
    
    std::list<std::vector<carl::Variable>> Manager::getModelEqualities() const
    {
        return mpPrimaryBackend->getModelEqualities();
    }
    
    const Model& Manager::model() const
    {
        mpPrimaryBackend->updateModel();
        return mpPrimaryBackend->model();
    }
    
    ModelValue Manager::optimum( const Poly& _objFct ) const
    {
        if( mObjectives.size() == 1 )
        {
            assert( mObjectives.front().first == _objFct );
            const Model& curModel = model();
            auto modelIter = curModel.find( mObjectives.front().second.first );
            assert( modelIter != curModel.end() );
            if( modelIter->second.isMinusInfinity() )
                return (mObjectives.front().second.second ? modelIter->second.asInfinity() : InfinityValue(true));
            assert( modelIter->second.isRational() );
            return (mObjectives.front().second.second ? modelIter->second.asRational() : -(modelIter->second.asRational()));
        }
        for( auto& obj : mObjectives )
        {
            if( obj.first == _objFct )
            {
                const Model& curModel = model();
                auto modelIter = curModel.find( obj.second.first );
                assert( modelIter != curModel.end() );
                assert( modelIter->second.isRational() );
                return (obj.second.second ? modelIter->second.asRational() : -(modelIter->second.asRational()));
            }
        }
        assert( false );
        return ModelValue();
    }
    
    std::vector<FormulaT> Manager::lemmas()
    {
        std::vector<FormulaT> result;
        mpPrimaryBackend->updateDeductions();
        for( const auto& ded : mpPrimaryBackend->deductions() )
        {
            result.push_back( ded.first );
        }
        return result;
    }
    
    std::pair<bool,FormulaT> Manager::getInputSimplified()
    {
        return mpPrimaryBackend->getReceivedFormulaSimplified();
    }
    
    void Manager::printAssignment() const
    {
        if( mObjectives.empty() )
            mpPrimaryBackend->printModel();
        else
        {
            const Model& model = mpPrimaryBackend->model();
            auto objectivesIter = mObjectives.begin();
            cout << "(";
            for( Model::const_iterator ass = model.begin(); ass != model.end(); ++ass )
            {
                if (ass != model.begin()) cout << " ";
                if (ass->first.isVariable() || ass->first.isBVVariable())
                {
                    if( objectivesIter != mObjectives.end() && ass->first.asVariable() == objectivesIter->second.first )
                    {
                        if( !objectivesIter->first.isVariable() )
                        {
                            if( ass->second.isMinusInfinity() )
                            {
                                string opt = objectivesIter->second.second ? toString( ass->second.asInfinity(), false ) : toString( InfinityValue(true), false );
                                cout << "(" << objectivesIter->first.toString( false, true ) << " " << opt << ")" << endl;
                            }
                            else
                            {
                                assert( ass->second.isRational() );
                                Rational opt = (objectivesIter->second.second ? ass->second.asRational() : -(ass->second.asRational()));
                                cout << "(" << objectivesIter->first.toString( false, true ) << " " << opt << ")" << endl;
                            }
                        }
                        ++objectivesIter;
                    }
                    else
                    {
                        cout << "(" << ass->first << " " << ass->second << ")" << endl;
                    }
                }
                else if( ass->first.isUVariable() )
                    cout << "(define-fun " << ass->first << " () " << ass->first.asUVariable().domain() << " " << ass->second << ")" << endl;
                else
                {
                    assert(ass->first.isFunction());
                    cout << ass->second.asUFModel() << endl;
                }
            }
            cout << ")" << endl;
        }
    }
    
    ModuleInput::iterator Manager::remove( ModuleInput::iterator _subformula )
    {
        assert( _subformula != mpPassedFormula->end() );
        mpPrimaryBackend->remove( _subformula );
        return mpPassedFormula->erase( _subformula );
    }
    
    void Manager::reset()
    {
        while( pop() );
        assert( mpPassedFormula->empty() );
        mBackendsOfModules.clear();
        while( !mGeneratedModules.empty() )
        {
            Module* ptsmodule = mGeneratedModules.back();
            mGeneratedModules.pop_back();
            delete ptsmodule;
        }
        while( mPrimaryBackendFoundAnswer.size() > 1 )
        {
            std::atomic_bool* toDelete = mPrimaryBackendFoundAnswer.back();
            mPrimaryBackendFoundAnswer.pop_back();
            delete toDelete;
        }
        mLogic = Logic::UNDEFINED;
		assert(mpPrimaryBackend == nullptr);
        mpPrimaryBackend = new Module( mpPassedFormula, mPrimaryBackendFoundAnswer, this );
        mGeneratedModules.push_back( mpPrimaryBackend );
    }

    // Methods.

    #ifdef SMTRAT_STRAT_PARALLEL_MODE
    void Manager::initialize()
    {
#if 1
		if (mStrategyGraph.hasBranches()) {
			// TODO
		}
#else
        mNumberOfBranches = mStrategyGraph.numberOfBranches();
        if( mNumberOfBranches > 1 )
        {
            mNumberOfCores = std::thread::hardware_concurrency();
            if( mNumberOfCores > 1 )
            {
                mStrategyGraph.setThreadAndBranchIds();
//                mStrategyGraph.tmpPrint();
//                std::this_thread::sleep_for(std::chrono::seconds(29));
                mRunsParallel = true;
                mpThreadPool = new ThreadPool( mNumberOfBranches, mNumberOfCores );
            }
        }
#endif
    }
    #endif
    
    void Manager::printAssertions( ostream& _out ) const
    {
        _out << "(";
        if( mpPassedFormula->size() == 1 )
        {
            _out << mpPassedFormula->back().formula();
        }
        else
        {
            for( auto subFormula = mpPassedFormula->begin(); subFormula != mpPassedFormula->end(); ++subFormula )
            {
                _out << (*subFormula).formula() << endl;
            }
        }
        _out << ")" << endl;
    }

    void Manager::printInfeasibleSubset( ostream& _out ) const
    {
        _out << "(";
        if( !mpPrimaryBackend->infeasibleSubsets().empty() )
        {
            const FormulaSetT& infSubSet = *mpPrimaryBackend->infeasibleSubsets().begin();
            if( infSubSet.size() == 1 )
            {
                _out << *infSubSet.begin();
            }
            else
            {
                for( auto subFormula = infSubSet.begin(); subFormula != infSubSet.end(); ++subFormula )
                {
                    _out << *subFormula << endl;
                }
            }
        }
        _out << ")" << endl;
    }
            
    void Manager::printBackTrackStack( std::ostream& _out ) const
    {
		auto btlIter = mBacktrackPoints.begin();
		std::size_t btlCounter = 0;
		while (btlIter != mBacktrackPoints.end() && *btlIter == mpPassedFormula->end()) {
			_out << "btl_" << btlCounter << ": (and ) skip" << std::endl;;
			btlCounter++;
			btlIter++;
		}
		_out << "btl_" << btlCounter << ": (and";
		for (auto it = mpPassedFormula->begin(); it != mpPassedFormula->end(); it++) {
			_out << " " << it->formula().toString();
			if (btlIter != mBacktrackPoints.end() && *btlIter == it) {
				btlCounter++;
				btlIter++;
				_out << " )" << std::endl << "btl_" << btlCounter << ": (and";
			}
		}
		_out << " )" << std::endl << std::endl;;
    }
    
#ifdef __VS
    vector<Module*> Manager::getBackends( Module* _requiredBy, atomic<bool>* _foundAnswer )
#else
    vector<Module*> Manager::getBackends( Module* _requiredBy, atomic_bool* _foundAnswer )
#endif
    {
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        std::lock_guard<std::mutex> lock(mBackendsMutex);
        #endif
        std::vector<Module*> backends;
        std::vector<Module*>& allBackends = mBackendsOfModules[_requiredBy];
        _requiredBy->mpPassedFormula->updateProperties();
        // Obtain list of backends in the strategy
        std::set<std::pair<thread_priority,AbstractModuleFactory*>> factories = mStrategyGraph.getBackends(_requiredBy->threadPriority().second, _requiredBy->pPassedFormula()->properties());
        for (const auto& iter: factories) {
            // Check if the respective module has already been created
            bool moduleExists = false;
            for (const auto& candidate: allBackends) {
                if (candidate->threadPriority() == iter.first) {
                    backends.emplace_back(candidate);
                    moduleExists = true;
                    break;
                }
            }
            // Create a new module with the given factory
            if (!moduleExists) {
                auto factory = iter.second;
                assert(factory != nullptr);
                smtrat::Conditionals foundAnswers = smtrat::Conditionals( _requiredBy->answerFound() );
                foundAnswers.emplace_back(_foundAnswer);
                Module* newBackend = factory->create(_requiredBy->pPassedFormula(), foundAnswers, this);
                newBackend->setId(mGeneratedModules.size());
                newBackend->setThreadPriority(iter.first);
                mGeneratedModules.emplace_back(newBackend);
                allBackends.emplace_back(newBackend);
                backends.emplace_back(newBackend);
                for(const auto& cons: _requiredBy->informedConstraints()) {
                    newBackend->inform(cons);
                }
                for(auto form = _requiredBy->rPassedFormula().begin(); form != _requiredBy->firstSubformulaToPass(); form++) {
                    newBackend->add(form);
                }
                newBackend->setObjective( _requiredBy->objective() );
            }
        }
        return backends;
    }

    #ifdef SMTRAT_STRAT_PARALLEL_MODE
    std::future<Answer> Manager::submitBackend( Module* _pModule, bool _full )
    {
        assert( mRunsParallel );
        return mpThreadPool->submitBackend( _pModule, _full );
    }

    void Manager::checkBackendPriority( Module* _pModule )
    {
        assert( mRunsParallel );
        mpThreadPool->checkBackendPriority( _pModule );
    }
    #endif
}    // namespace smtrat
