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
 * @file UIEquality.h
 * @author Gereon Kremer <gereon.kremer@cs.rwth-aachen.de>
 * @author Florian Corzilius <corzilius@cs.rwth-aachen.de>
 * @since 2014-10-20
 * @version 2014-10-22
 */

#pragma once

#include <boost/variant.hpp>
#include "UIVariable.h"
#include "UFInstance.h"

namespace smtrat
{
    /**
     * Implements an uninterpreted equality, that is an equality of either 
     *     two uninterpreted function instances,
     *     two uninterpreted variables,
     *     or an uninterpreted function instance and an uninterpreted variable. 
     */
    class UIEquality
    {
        public:
            /// The type of the left and right-hand side of an uninterpreted equality.
            typedef boost::variant<UIVariable, UFInstance> Arg;

        private:

            /**
             * Checks whether the given argument is an uninterpreted variable.
             */
            struct IsUIVariable: public boost::static_visitor<bool> 
            {
                /**
                 * @param An uninterpreted variable.
                 * @return true
                 */
                bool operator()(const UIVariable&) const
                {
                    return true;
                }
                
                /**
                 * @param An uninterpreted function instance.
                 * @return false
                 */
                bool operator()(const UFInstance&) const 
                {
                    return false;
                }
            };

            /**
             * Checks whether the given argument is an uninterpreted function instance.
             */
            struct IsUFInstance: public boost::static_visitor<bool> 
            {
                /**
                 * @param An uninterpreted variable.
                 * @return false
                 */
                bool operator()(const UIVariable&) const
                {
                    return true;
                }
                
                /**
                 * @param An uninterpreted function instance.
                 * @return true
                 */
                bool operator()(const UFInstance&) const 
                {
                    return false;
                }
            };

            // Member.

            /// The left-hand side of this uninterpreted equality.
            Arg mLhs;
            /// The right-hand side of this uninterpreted equality.
            Arg mRhs;

        public:
            
            UIEquality(); // No default constructor.
            
            /**
             * Constructs an uninterpreted equality.
             * @param _uvarA An uninterpreted variable, which is going to be the left-hand side of this uninterpreted equality.
             * @param _uvarBAn uninterpreted variable, which is going to be the right-hand side of this uninterpreted equality.
             */
            UIEquality( const UIVariable& _uvarA, const UIVariable& _uvarB ):
                mLhs( _uvarA ),
                mRhs( _uvarB )
            {
                assert( _uvarA.domain() == _uvarB.domain() );
            }
            
            /**
             * Constructs an uninterpreted equality.
             * @param _uvar An uninterpreted variable, which is going to be the left-hand side of this uninterpreted equality.
             * @param _ufun An uninterpreted function instance, which is going to be the right-hand side of this uninterpreted equality.
             */
            UIEquality( const UIVariable& _uvar, const UFInstance& _ufun ):
                mLhs( _uvar ),
                mRhs( _ufun )
            {
                assert( _uvar.domain() == _ufun.codomain() );
            }
            
            /**
             * Constructs an uninterpreted equality.
             * @param _ufun An uninterpreted function instance, which is going to be the left-hand side of this uninterpreted equality.
             * @param _uvar An uninterpreted variable, which is going to be the right-hand side of this uninterpreted equality.
             */
            UIEquality( const UFInstance& _ufun, const UIVariable& _uvar ):
                mLhs( _ufun ),
                mRhs( _uvar )
            {
                assert( _uvar.domain() == _ufun.codomain() );
            }
            
            /**
             * Constructs an uninterpreted equality.
             * @param _ufunA An uninterpreted function instance, which is going to be the left-hand side of this uninterpreted equality.
             * @param _ufunB An uninterpreted function instance, which is going to be the right-hand side of this uninterpreted equality.
             */
            UIEquality( const UFInstance& _ufunA, const UFInstance& _ufunB ):
                mLhs( _ufunA ),
                mRhs( _ufunB )
            {
                assert( _ufunA.codomain() == _ufunB.codomain() );
            }

            /**
             * @return true, if the left-hand side is an uninterpreted variable.
             */
            bool lhsIsUV() const 
            {
                return boost::apply_visitor(IsUIVariable(), mLhs);
            }

            /**
             * @return true, if the right-hand side is an uninterpreted variable.
             */
            bool rhsIsUV() const 
            {
                return boost::apply_visitor(IsUIVariable(), mRhs);
            }

            /**
             * @return true, if the left-hand side is an uninterpreted function instance.
             */
            bool lhsIsUF() const 
            {
                return boost::apply_visitor(IsUFInstance(), mLhs);
            }

            /**
             * @return true, if the right-hand side is an uninterpreted function instance.
             */
            bool rhsIsUF() const 
            {
                return boost::apply_visitor(IsUFInstance(), mRhs);
            }

            /**
             * @return The left-hand side, which must be an uninterpreted variable.
             */
            const UIVariable& lhsAsUV() const 
            {
                return boost::get<UIVariable>(mLhs);
            }

            /**
             * @return The right-hand side, which must be an uninterpreted variable.
             */
            const UIVariable& rhsAsUV() const 
            {
                return boost::get<UIVariable>(mRhs);
            }

            /**
             * @return The left-hand side, which must be an uninterpreted function instance.
             */
            const UFInstance& lhsAsUF() const 
            {
                return boost::get<UFInstance>(mLhs);
            }

            /**
             * @return The right-hand side, which must be an uninterpreted function instance.
             */
            const UFInstance& rhsAsUF() const 
            {
                return boost::get<UFInstance>(mRhs);
            }
            
            /**
             * @param _ueq The uninterpreted equality to compare with.
             * @return true, if this and the given equality instance are equal.
             */
            bool operator==( const UIEquality& _ueq ) const
            {
                if( lhsIsUV() != _ueq.lhsIsUV() )
                {
                    return false;
                }
                if( rhsIsUV() != _ueq.rhsIsUV() )
                {
                    return false;
                }
                if( lhsIsUV() )
                {
                    if( rhsIsUV() )
                    {
                        return lhsAsUV() == _ueq.lhsAsUV() && rhsAsUV() == _ueq.rhsAsUV();
                    }
                    else
                    {
                        return lhsAsUV() == _ueq.lhsAsUV() && rhsAsUF() == _ueq.rhsAsUF();
                    }
                }
                else
                {
                    if( rhsIsUV() )
                    {
                        return lhsAsUF() == _ueq.lhsAsUF() && rhsAsUV() == _ueq.rhsAsUV();
                    }
                    else
                    {
                        return lhsAsUF() == _ueq.lhsAsUF() && rhsAsUF() == _ueq.rhsAsUF();
                    }
                }
            }
            
            /**
             * @param _ueq The uninterpreted equality to compare with.
             * @return true, if this uninterpreted equality is less than the given one.
             */
            bool operator<( const UIEquality& _ueq ) const
            {
                if( lhsIsUV() && !_ueq.lhsIsUV() )
                {
                    return true;
                }
                else if( !lhsIsUV() && _ueq.lhsIsUV() )
                {
                    return false;
                }
                if( rhsIsUV() && !_ueq.rhsIsUV() )
                {
                    return true;
                }
                else if( !rhsIsUV() && _ueq.rhsIsUV() )
                {
                    return false;
                }
                assert( lhsIsUV() == _ueq.lhsIsUV() );
                if( lhsIsUV() )
                {
                    if( lhsAsUV() < _ueq.lhsAsUV() )
                    {
                        return true;
                    }
                    else if( _ueq.lhsAsUV() < lhsAsUV() )
                    {
                        return false;
                    }
                }
                else
                {
                    if( lhsAsUF() < _ueq.lhsAsUF() )
                    {
                        return true;
                    }
                    else if( _ueq.lhsAsUF() < lhsAsUF() )
                    {
                        return false;
                    }
                }
                assert( (lhsIsUV() && lhsAsUV() == _ueq.lhsAsUV()) || (lhsIsUF() && lhsAsUF() == _ueq.lhsAsUF()) );
                if( rhsIsUV() )
                {
                    return rhsAsUV() < _ueq.rhsAsUV();
                }
                else
                {
                    return rhsAsUF() < _ueq.rhsAsUF();
                }
                return false;
            }
            
            /**
             * Prints the given uninterpreted equality on the given output stream.
             * @param _os The output stream to print on.
             * @param _uvar The uninterpreted equality to print.
             * @return The output stream after printing the given uninterpreted equality on it.
             */
            friend std::ostream& operator<<( std::ostream& _os, const UIEquality& _ueq )
            {
                _os << "(= ";
                if( _ueq.lhsIsUV() )
                {
                    _os << _ueq.lhsAsUV();
                }
                else
                {
                    _os << _ueq.lhsAsUF();
                }
                _os << " ";
                if( _ueq.rhsIsUV() )
                {
                    _os << _ueq.rhsAsUV();
                }
                else
                {
                    _os << _ueq.rhsAsUF();
                }
                _os << ")";
                return _os;
            }
    };
} // end namespace smtrat

namespace std
{
    /**
     * Implements std::hash for uninterpreted equalities.
     */
    template<>
    struct hash<smtrat::UIEquality>
    {
    public:
        /**
         * @param _ueq The uninterpreted equality to get the hash for.
         * @return The hash of the given uninterpreted equality.
         */
        size_t operator()( const smtrat::UIEquality& _ueq ) const 
        {
            size_t result = _ueq.lhsIsUV() ? std::hash<smtrat::UIVariable>()( _ueq.lhsAsUV() ) : std::hash<smtrat::UFInstance>()( _ueq.lhsAsUF() );
            result ^= _ueq.rhsIsUV() ? std::hash<smtrat::UIVariable>()( _ueq.rhsAsUV() ) : std::hash<smtrat::UFInstance>()( _ueq.rhsAsUF() );
            return result;
        }
    };
}
