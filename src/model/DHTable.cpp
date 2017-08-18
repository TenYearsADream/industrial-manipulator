/*
 * DHTable.cpp
 *
 *  Created on: Aug 17, 2017
 *      Author: a1994846931931
 */

#include "DHTable.h"

namespace robot {
namespace model {

DHTable::DHTable() {
	// TODO Auto-generated constructor stub

}


int DHTable::size()
{
	return _dHParam.size();
}

const DHParameters& DHTable::operator()(int index) const
{
	return _dHParam[index];
}

const DHParameters& DHTable::operator[](int index) const
{
	return _dHParam[index];
}

void DHTable::append(const DHParameters& dHParam)
{
	_dHParam.push_back(dHParam);
}

DHTable::~DHTable() {
	// TODO Auto-generated destructor stub
}

} /* namespace model */
} /* namespace robot */
