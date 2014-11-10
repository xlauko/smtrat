/*
 *  SMT-RAT - Satisfiability-Modulo-Theories Real Algebra Toolbox
 * Copyright (C) 2012 Florian Corzilius, Ulrich Loup, Erika Abraham, Sebastian Junges
 *
 * This file is part of SMT-RAT.
 *
 * SMT-RAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SMT-RAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SMT-RAT.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/**
 * @file UFInstancesManager.cpp
 * @author Gereon Kremer <gereon.kremer@cs.rwth-aachen.de>
 * @author Florian Corzilius <corzilius@cs.rwth-aachen.de>
 * @since 2014-10-22
 * @version 2014-11-10
 */

#include "UFInstancesManager.h"

using namespace std;

namespace smtrat
{   
    ostream& UFInstancesManager::print( ostream& _out, const UFInstance& _ufi, bool _infix, bool _friendlyNames ) const
    {
        assert( _ufi.id() != 0 );
        assert( _ufi.id() < mUFInstances.size() );
        const UFInstanceContent& ufic = *mUFInstances[_ufi.id()];
        if( _infix )
        {
            _out << _ufi.uninterpretedFunction().name() << "(";
        }
        else
        {
            _out << "(" << ufic.uninterpretedFunction().name();
        }
        for( auto iter = _ufi.args().begin(); iter != _ufi.args().end(); ++iter )
        {
            if( _infix )
            {
                if( iter != _ufi.args().begin() )
                {
                    _out << ", ";
                }
            }
            else
            {
                _out << " ";
            }
            _out << iter->toString( _friendlyNames );
        }
        _out << ")";
        return _out;
    }
    
    UFInstance UFInstancesManager::newUFInstance( const UFInstanceContent* _ufic )
    {
        auto iter = mUFInstanceIdMap.find( _ufic );
        // Check if this uninterpreted function content has already been created
        if( iter != mUFInstanceIdMap.end() )
        {
            delete _ufic;
            return UFInstance( iter->second );
        }
        // Create the uninterpreted function instance
        mUFInstanceIdMap.emplace( _ufic, mUFInstances.size() );
        UFInstance ufi( mUFInstances.size() );
        mUFInstances.push_back( _ufic );
        return ufi;
    }
    
    bool UFInstancesManager::argsCorrect( const UFInstanceContent& _ufic )
    {
        if( !(_ufic.uninterpretedFunction().domain().size() == _ufic.args().size()) )
        {
            return false;
        }
        for( size_t i = 0; i < _ufic.uninterpretedFunction().domain().size(); ++i )
        {
            if( !(_ufic.uninterpretedFunction().domain().at(i) == _ufic.args().at(i).domain()) )
            {
                return false;
            }
        }
        return true;
    }
}