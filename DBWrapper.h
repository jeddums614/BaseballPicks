/*
 * DBWrapper.h
 *
 *  Created on: Feb 18, 2021
 *      Author: jeremy
 */

#ifndef DBWRAPPER_H_
#define DBWRAPPER_H_

#include <vector>
#include <string>
#include <map>
#include <sqlite3.h>

class DBWrapper {
public:
	DBWrapper() = delete;
	DBWrapper(const DBWrapper &) = delete;
	DBWrapper& operator=(const DBWrapper &) = delete;
	static std::vector<std::map<std::string, std::string>> queryDatabase(sqlite3* db, const std::string & query);
};



#endif /* DBWRAPPER_H_ */
