/*
 * Utils.h
 *
 *  Created on: Feb 28, 2021
 *      Author: jeremy
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <vector>
#include <string>

class Utils {
public:
	Utils() = delete;
	Utils(const Utils &) = delete;
	Utils& operator=(const Utils &) = delete;
	static std::vector<std::string> split(const std::string & str, char delim = ',');
};



#endif /* UTILS_H_ */
