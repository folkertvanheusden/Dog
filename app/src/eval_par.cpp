#include <assert.h>
#include <cmath>
#include <functional>
#include <numeric>

#include "eval_par.h"


eval_par::eval_par()
{
}

eval_par::eval_par(const std::vector<libchess::TunableParameter> & params)
{
	for(auto & p : params)
		*m.find(p.name())->second = p.value();

	if (*m.find("tune_psq_div")->second == 0)
		*m.find("tune_psq_div")->second = 1;
}

eval_par::~eval_par()
{
}

std::vector<libchess::TunableParameter> eval_par::get_tunable_parameters() const
{
	std::vector<libchess::TunableParameter> list;

	for(auto & it : m)
		list.push_back({ it.first, *it.second });

	return list;
}

void eval_par::set_eval(const std::string & name, int value)
{
	auto it = m.find(name);

	*it->second = value;
}

eval_par default_parameters;
