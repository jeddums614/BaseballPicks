/*
 * Utils.cpp
 *
 *  Created on: Feb 28, 2021
 *      Author: jeremy
 */


#include "Utils.h"
#include <sstream>

std::vector<std::string> Utils::split(const std::string & str, char delim)
{
	std::vector<std::string> parts;
	std::string part = "";
	std::stringstream iss{str};

	while (std::getline(iss, part, delim))
	{
		parts.push_back(part);
	}

	return (parts);
}
