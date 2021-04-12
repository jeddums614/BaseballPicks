/*
 * DBWrapper.cpp
 *
 *  Created on: Feb 18, 2021
 *      Author: jeremy
 */


#include "DBWrapper.h"
#include <iostream>

std::vector<std::map<std::string, std::string>> DBWrapper::queryDatabase(sqlite3* db, const std::string & query)
{
	std::vector<std::map<std::string, std::string>> results;

	sqlite3_stmt *stmt;

    int rc = 0;
    do
    {
    	rc = sqlite3_prepare_v2(db,query.c_str(),-1,&stmt,NULL);
    	//std::cout << "select rc = " << rc << std::endl;
    }
    while (rc != SQLITE_OK);

	while (sqlite3_step(stmt) != SQLITE_DONE) {
		int num_cols = sqlite3_column_count(stmt);
		std::map<std::string, std::string> row;

		for (int i = 0; i < num_cols; i++)
		{
			std::string val;
			std::string column = sqlite3_column_name(stmt, i);
			switch (sqlite3_column_type(stmt, i))
			{
			case SQLITE3_TEXT:
				val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
				break;
			case SQLITE_INTEGER:
				val = std::to_string(sqlite3_column_int(stmt, i));
				break;
			case SQLITE_FLOAT:
				val = std::to_string(sqlite3_column_double(stmt, i));
				break;
			default:
				break;
			}

			row[column] = val;
		}

		results.push_back(row);
	}

	sqlite3_finalize(stmt);

	return (results);
}
