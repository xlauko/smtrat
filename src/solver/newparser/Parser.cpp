#include "Parser.h"

#include <cassert>
#include <iostream>
#include <limits>

#include "../../lib/ConstraintPool.h"
#include "../../lib/Formula.h"
#include "lib/FormulaPool.h"

namespace smtrat {
namespace parser {

SMTLIBParser::SMTLIBParser(InstructionHandler* ih, bool queueInstructions):
	SMTLIBParser::base_type(main),
	handler(ih),
	queueInstructions(queueInstructions)
{
	boundary = &qi::no_skip[(qi::space | qi::char_(")"))];

	var = var_bool | var_theory;
	var.name("variable");

	key = ":" > symbol;
	key.name("key");
	value = qi::bool_ | symbol | decimal | integral;
	value.name("value");
	attribute = key > -value;
	attribute.name("attribute");

	varlist = *var;
	varlist.name("variable list");

	symlist = *symbol;
	symlist.name("symbol list");

	bindlist = +(lit("(") > binding > lit(")"));
	bindlist.name("binding list");
	binding = symbol[qi::_a = qi::_1] > (
			polynomial[px::bind(&SMTLIBParser::addTheoryBinding, px::ref(*this), qi::_a, qi::_1)]
		|	formula[px::bind(&SMTLIBParser::addBooleanBinding, px::ref(*this), qi::_a, qi::_1)]
	);
	binding.name("binding");
	
	cmd = "(" > (
			(lit("assert") > formula > ")")[px::bind(&SMTLIBParser::add, px::ref(*this), qi::_1)]
		|	(lit("check-sat") > ")")[px::bind(&SMTLIBParser::check, px::ref(*this))]
		|	(lit("declare-const") > symbol > domain > ")")[px::bind(&SMTLIBParser::declareConst, px::ref(*this), qi::_1, qi::_2)]
		|	(lit("declare-fun") > symbol > "(" > symlist > ")" > domain > ")")[px::bind(&SMTLIBParser::declareFun, px::ref(*this), qi::_1, qi::_2, qi::_3)]
		|	(lit("declare-sort") > symbol > integral > ")")[px::bind(&SMTLIBParser::declareSort, px::ref(*this), qi::_1, qi::_2)]
		|	(lit("define-fun") > symbol > "(" > symlist > ")" > domain > formula > ")")[px::bind(&SMTLIBParser::defineFun, px::ref(*this), qi::_1, qi::_2, qi::_3, qi::_4)]
		|	(lit("define-sort") > symbol > "(" > symlist > ")" > symbol > ")")[px::bind(&SMTLIBParser::defineSort, px::ref(*this), qi::_1, qi::_2, qi::_3)]
		|	(lit("exit") > ")")[px::bind(&SMTLIBParser::exit, px::ref(*this))]
		|	(lit("get-assertions") > ")")[px::bind(&SMTLIBParser::getAssertions, px::ref(*this))]
		|	(lit("get-assignment") > ")")[px::bind(&SMTLIBParser::getAssignment, px::ref(*this))]
		|	(lit("get-info") > key > ")")[px::bind(&SMTLIBParser::getInfo, px::ref(*this), qi::_1)]
		|	(lit("get-option") > key > ")")[px::bind(&SMTLIBParser::getOption, px::ref(*this), qi::_1)]
		|	(lit("get-proof") > ")")[px::bind(&SMTLIBParser::getProof, px::ref(*this))]
		|	(lit("get-unsat-core") > ")")[px::bind(&SMTLIBParser::getUnsatCore, px::ref(*this))]
		|	(lit("get-value") > varlist > ")")[px::bind(&SMTLIBParser::getValue, px::ref(*this), qi::_1)]
		|	(lit("pop") > integral > ")")[px::bind(&SMTLIBParser::pop, px::ref(*this), qi::_1)]
		|	(lit("push") > integral > ")")[px::bind(&SMTLIBParser::push, px::ref(*this), qi::_1)]
		|	(lit("set-info") > key > value > ")")[px::bind(&SMTLIBParser::setInfo, px::ref(*this), qi::_1, qi::_2)]
		|	(lit("set-logic") > logic > ")")[px::bind(&SMTLIBParser::setLogic, px::ref(*this), qi::_1)]
		|	(lit("set-option") > key > value > ")")[px::bind(&SMTLIBParser::setOption, px::ref(*this), qi::_1, qi::_2)]
	);
	cmd.name("command");

	formula = 
			(bind_bool >> boundary)[_val = qi::_1]
		|	(var_bool >> boundary)[_val = px::bind(&SMTLIBParser::mkBoolean, px::ref(*this), qi::_1)]
		|	lit("true")[_val = px::bind(&trueFormula)]
		|	lit("false")[_val = px::bind(&falseFormula)]
		|	("(" >> formula_op >> ")")[_val = qi::_1]
	;
	formula.name("formula");
	
	formula_list = +formula;
	formula_list.name("formula list");
	formula_op =
				((op_bool >> formula_list)[_val = px::bind(&SMTLIBParser::mkFormula, px::ref(*this), qi::_1, qi::_2)])
			|	(relation >> polynomial >> polynomial)[_val = px::bind(&SMTLIBParser::mkConstraint, px::ref(*this), qi::_2, qi::_3, qi::_1)]
			|	(lit("as")[qi::_pass = false] > symbol > symbol)
			|	(lit("not") > formula[_val = px::bind(&newNegation, qi::_1)])
			|	((lit("implies") | "=>") >> formula >> formula)[_val = px::bind(newImplication, qi::_1, qi::_2)]
			|	(lit("let")[px::bind(&SMTLIBParser::pushVariableStack, px::ref(*this))]
				> ("(" > bindlist > ")" > formula)[px::bind(&SMTLIBParser::popVariableStack, px::ref(*this)), _val = qi::_1])
			|	("exists" > bindlist > formula)
			|	("forall" > bindlist > formula)
			|	("ite" > (formula > formula > formula)[_val = px::bind(&SMTLIBParser::mkIteInFormula, px::ref(*this), qi::_1, qi::_2, qi::_3)])
			|	(("!" > formula > *attribute)[px::bind(&annotateFormula, qi::_1, qi::_2), _val = qi::_1])
	;
	formula_op.name("formula operation");

	polynomial_op = op_theory >> +polynomial;
	polynomial_op.name("polynomial operation");
	polynomial_ite = lit("ite") > (formula > polynomial > polynomial)[_val = px::construct<Polynomial>(px::bind(&SMTLIBParser::mkIteInExpr, px::ref(*this), qi::_1, qi::_2, qi::_3))];
	polynomial_ite.name("polynomial if-then-else");
	polynomial =
			(bind_theory >> boundary)
		|	(var_theory >> boundary)
		|	decimal
		|	integral
		|	("(" >> (
				polynomial_ite
			|	polynomial_op
		) >> ")")
	;
	polynomial.name("polynomial");

	main = *cmd > qi::eoi;
	main.name("SMTLib File");

	qi::on_error<qi::fail>(main, errorHandler(px::ref(*this), qi::_1, qi::_2, qi::_3, qi::_4));
/*
	qi::on_success(bindlist, successHandler(px::ref(*this), px::ref(bindlist), qi::_val, qi::_1, qi::_2));
	qi::on_success(polynomial, successHandler(px::ref(*this), px::ref(polynomial), qi::_val, qi::_1, qi::_2));
	qi::on_success(polynomial_op, successHandler(px::ref(*this), px::ref(polynomial_op), qi::_val, qi::_1, qi::_2));
	qi::on_success(formula, successHandlerPtr(px::ref(*this), px::ref(formula), qi::_val, qi::_1, qi::_2));
	qi::on_success(formula_op, successHandlerPtr(px::ref(*this), px::ref(formula_op), qi::_val, qi::_1, qi::_2));
	qi::on_success(cmd, successHandler(px::ref(*this), px::ref(cmd), qi::_val, qi::_1, qi::_2));
	qi::on_success(main, successHandler(px::ref(*this), px::ref(main), qi::_val, qi::_1, qi::_2));
*/
}

bool SMTLIBParser::parse(std::istream& in, const std::string& filename) {
	in.unsetf(std::ios::skipws);
	mInputStream = &in;
	BaseIteratorType basebegin(in);
	Iterator begin(basebegin);
	Iterator end;
	Skipper skipper;
	try {
		return qi::phrase_parse(begin, end, main, skipper);
	} catch (...) {
		std::cerr << "Unknown error during parser." << std::endl;
		return false;
	}
}

void SMTLIBParser::add(const Formula* f) {
	assert(f != nullptr);
	if (this->handler->printInstruction()) handler->regular() << "(assert " << *f << ")" << std::endl;
	callHandler(&InstructionHandler::add, f);
}
void SMTLIBParser::check() {
	if (this->handler->printInstruction()) handler->regular() << "(check-sat)" << std::endl;
	callHandler(&InstructionHandler::check);
}
void SMTLIBParser::declareConst(const std::string& name, const carl::VariableType& sort) {
	assert(this->isSymbolFree(name));
	switch (sort) {
	case carl::VariableType::VT_BOOL: {
			if (this->var_bool.find(name) != nullptr) handler->warn() << "a boolean variable with name '" << name << "' has already been defined.";
			carl::Variable var = newBooleanVariable(name, true);
			this->var_bool.add(name, var);
			std::cout << "Declared boolean variable " << var << std::endl;
			break;
		}
		break;
	case carl::VariableType::VT_INT:
	case carl::VariableType::VT_REAL: {
			if (this->var_theory.find(name) != nullptr) handler->warn() << "a theory variable with name '" << name << "' has already been defined.";
			carl::Variable var = newArithmeticVariable(name, sort, true);
			this->var_theory.add(name, var);
			std::cout << "Declared theory variable " << var << std::endl;
			break;
		}
	default:
		handler->error() << "Only variables of type \"Bool\", \"Int\" or \"Real\" are allowed!";
	}
	if (this->handler->printInstruction()) handler->regular() << "(declare-const " << name << " " << sort << ")" << std::endl;
	callHandler(&InstructionHandler::declareConst, name, sort);
}
void SMTLIBParser::declareFun(const std::string& name, const std::vector<std::string>& args, const carl::VariableType& sort) {
	assert(this->isSymbolFree(name));
	switch (sort) {
	case carl::VariableType::VT_BOOL: {
			if (this->var_bool.find(name) != nullptr) handler->warn() << "a boolean variable with name '" << name << "' has already been defined.";
			carl::Variable var = newBooleanVariable(name, true);
			this->var_bool.add(name, var);
			std::cout << "Declared boolean variable " << var << std::endl;
			break;
		}
		break;
	case carl::VariableType::VT_INT:
	case carl::VariableType::VT_REAL: {
			if (this->var_theory.find(name) != nullptr) handler->warn() << "a theory variable with name '" << name << "' has already been defined.";
			carl::Variable var = newArithmeticVariable(name, sort, true);
			this->var_theory.add(name, var);
			std::cout << "Declared theory variable " << var << std::endl;
			break;
		}
	default:
		handler->error() << "Only variables of type \"Bool\", \"Int\" or \"Real\" are allowed!";
	}
	if (this->handler->printInstruction()) handler->regular() << "(declare-fun " << name << " () " << sort << ")" << std::endl;
	callHandler(&InstructionHandler::declareFun, name, args, sort);
}
void SMTLIBParser::declareSort(const std::string& name, const Rational& arity) {
	if (this->handler->printInstruction()) handler->regular() << "(declare-sort " << name << " " << arity << ")" << std::endl;
	callHandler(&InstructionHandler::declareSort, name, carl::toInt<unsigned>(arity));
}
void SMTLIBParser::defineFun(const std::string& name, const std::vector<std::string>& args, const carl::VariableType& sort, const Formula* term) {
	if (this->handler->printInstruction()) handler->regular() << "(define-fun " << name << " () " << term << ")" << std::endl;
	callHandler(&InstructionHandler::defineFun, name, args, sort, term);
}
void SMTLIBParser::defineSort(const std::string& name, const std::vector<std::string>& args, const std::string& theory) {
	if (this->handler->printInstruction()) handler->regular() << "(define-sort " << name << " () " << theory << ")" << std::endl;
	callHandler(&InstructionHandler::defineSort, name, args, theory);
}
void SMTLIBParser::exit() {
	if (this->handler->printInstruction()) handler->regular() << "(exit)" << std::endl;
	callHandler(&InstructionHandler::exit);
}
void SMTLIBParser::getAssertions() {
	if (this->handler->printInstruction()) handler->regular() << "(get-assertions)" << std::endl;
	callHandler(&InstructionHandler::getAssertions);
}
void SMTLIBParser::getAssignment() {
	if (this->handler->printInstruction()) handler->regular() << "(get-assignment)" << std::endl;
	callHandler(&InstructionHandler::getAssignment);
}
void SMTLIBParser::getInfo(const std::string& key) {
	if (this->handler->printInstruction()) handler->regular() << "(get-info " << key << ")" << std::endl;
	callHandler(&InstructionHandler::getInfo, key);
}
void SMTLIBParser::getOption(const std::string& key) {
	if (this->handler->printInstruction()) handler->regular() << "(get-option " << key << ")" << std::endl;
	callHandler(&InstructionHandler::getOption, key);
}
void SMTLIBParser::getProof() {
	if (this->handler->printInstruction()) handler->regular() << "(get-proof)" << std::endl;
	callHandler(&InstructionHandler::getProof);
}
void SMTLIBParser::getUnsatCore() {
	if (this->handler->printInstruction()) handler->regular() << "(get-unsat-core)" << std::endl;
	callHandler(&InstructionHandler::getUnsatCore);
}
void SMTLIBParser::getValue(const std::vector<VariableWrapper>& vars) {
	std::vector<carl::Variable> carlVars;
	carlVars.reserve(vars.size());
	for (auto v: vars) carlVars.push_back(v);
	if (this->handler->printInstruction()) handler->regular() << "(get-value)" << std::endl;
	callHandler(&InstructionHandler::getValue, carlVars);
}
void SMTLIBParser::pop(const Rational& n) {
	if (this->handler->printInstruction()) handler->regular() << "(pop " << n << ")" << std::endl;
	callHandler(&InstructionHandler::pop, carl::toInt<unsigned>(n));
}
void SMTLIBParser::push(const Rational& n) {
	if (this->handler->printInstruction()) handler->regular() << "(push " << n << ")" << std::endl;
	callHandler(&InstructionHandler::push, carl::toInt<unsigned>(n));
}
void SMTLIBParser::setInfo(const std::string& key, const Value& val) {
	if (this->handler->printInstruction()) handler->regular() << "(set-info " << key << " " << val << ")" << std::endl;
	callHandler(&InstructionHandler::setInfo, key, val);
}
void SMTLIBParser::setLogic(const smtrat::Logic& l) {
	this->mLogic = l;
	if (this->handler->printInstruction()) handler->regular() << "(set-logic " << l << ")" << std::endl;
	callHandler(&InstructionHandler::setLogic, l);
}
void SMTLIBParser::setOption(const std::string& key, const Value& val) {
	if (this->handler->printInstruction()) handler->regular() << "(set-option " << key << " " << val << ")" << std::endl;
	callHandler(&InstructionHandler::setOption, key, val);
}

const Formula* SMTLIBParser::mkConstraint(const Polynomial& lhs, const Polynomial& rhs, Relation rel) {
	const Constraint* cons = newConstraint(lhs-rhs, rel);
	// Check if there have been ite expressions within this polynomial.
	// if so, collect ite formulas from mTheoryIteBindings and add them to the constraint
	PointerSet<Formula> varBindings;
	for (auto v: cons->variables()) {
		auto bindingVars = mTheoryIteBindings.find(v);
		if (bindingVars != mTheoryIteBindings.end()) {
			varBindings.insert(bindingVars->second);
			mTheoryIteBindings.erase(bindingVars);
		}
	}
	if (!varBindings.empty()) {
		varBindings.insert(newFormula(cons));
		return newFormula(smtrat::AND, std::move(varBindings));
	} else {
		return newFormula(cons);
	}
}

const smtrat::Formula* SMTLIBParser::mkFormula( smtrat::Type type, PointerSet<Formula>& _subformulas )
{
	assert(type == smtrat::AND || type == smtrat::OR || type == smtrat::XOR || type == smtrat::IFF);
	auto f =  newFormula(type, _subformulas);
	return f;
}

carl::Variable SMTLIBParser::mkIteInExpr(const Formula* _condition, Polynomial& _then, Polynomial& _else) {
	carl::Variable auxVar = (mLogic == Logic::QF_LIA || mLogic == Logic::QF_NIA) ? newAuxiliaryIntVariable() : newAuxiliaryRealVariable();

	const Formula* consThen = mkConstraint(Polynomial(auxVar), _then, Relation::EQ);
	const Formula* consElse = mkConstraint(Polynomial(auxVar), _else, Relation::EQ);

	assert(mTheoryIteBindings.find(auxVar) == mTheoryIteBindings.end());
	mTheoryIteBindings.emplace(auxVar, newFormula(smtrat::AND, newImplication(_condition, consThen), newImplication(newNegation(_condition), consElse)));
	return auxVar;
}

const Formula* SMTLIBParser::mkIteInFormula(const Formula* _condition, const Formula* _then, const Formula* _else) const {
	return newFormula(
			smtrat::AND,
			newImplication(_condition, _then),
			newImplication(newNegation(_condition), _else)
		);
}

void SMTLIBParser::addTheoryBinding( std::string& _varName, Polynomial& _polynomial )
{
	assert(this->isSymbolFree(_varName));
	mVariableStack.top().emplace_back(_varName, carl::VariableType::VT_REAL);
	bind_theory.add(_varName, _polynomial);
}

void SMTLIBParser::addBooleanBinding(std::string& _varName, const Formula* _formula) {
	assert(this->isSymbolFree(_varName));
	mVariableStack.top().emplace_back(_varName, carl::VariableType::VT_BOOL);
	bind_bool.add(_varName, _formula);
}

}
}