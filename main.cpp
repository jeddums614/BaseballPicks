/*
 * main.cpp
 *
 *  Created on: Feb 18, 2021
 *      Author: jeremy
 */

#include "DBWrapper.h"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <tuple>
#include <iterator>
#include "Utils.h"

enum class teamType { AWAY, HOME };

std::ostream& operator<< (std::ostream& os, const teamType& t) {
	switch (t) {
	case teamType::AWAY:
		os << "Away";
		break;

	case teamType::HOME:
		os << "Home";
		break;

	default:
		os << "Other";
		break;
	}

	return (os);
}

int main(int argc, char** argv) {
	sqlite3* db = NULL;

	std::string datestr = "";
	if (argc > 1) {
		datestr = argv[1];
	}

	if (datestr.empty()) {
	    auto todaydate = std::chrono::system_clock::now();
	    std::time_t now_c = std::chrono::system_clock::to_time_t(todaydate);
	    std::tm now_tm = *std::localtime(&now_c);
	    int year = now_tm.tm_year+1900;
	    datestr = std::to_string(year) + "-";
	    if (now_tm.tm_mon+1 < 10) {
		    datestr += "0";
	    }
	    datestr += std::to_string(now_tm.tm_mon+1) + "-";
	    if (now_tm.tm_mday < 10) {
		    datestr += "0";
	    }
	    datestr += std::to_string(now_tm.tm_mday);
	}
	const std::string startdate = "2021-04-01";

	const int dbflags = SQLITE_OPEN_READWRITE;
	int rc = sqlite3_open_v2("baseball.db", &db, dbflags, NULL);

	if (rc != SQLITE_OK)
	{
	    std::cout << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
	    sqlite3_close_v2(db);
		return (1);
	}

	std::ifstream ifs("todaymatchups.txt");
	std::string line = "";
	int side = 0;

	while (std::getline(ifs, line)) {
		std::cout << line << std::endl;
		teamType tmType = (side % 2 == 0 ? teamType::AWAY : teamType::HOME);
		std::cout << tmType << std::endl;
		std::vector<std::string> gameParts = Utils::split(line);

		if (gameParts.size() < 5) {
			std::cout << "Not all info filled out in todaymatchups.txt for this line" << std::endl;
			++side;
			continue;
		}

		std::string pitcher = gameParts[0];
		if (pitcher.empty()) {
			++side;
			continue;
		}
		std::string opponent = gameParts[1];
		std::string umpire = gameParts[2];
		std::size_t pos = umpire.find("'");
		if (pos != std::string::npos) {
			umpire.insert(pos, "'");
		}
		std::string ballpark = gameParts[3];
		std::string gametime = gameParts[4];

		std::string query = "select id,throws from players where position='P' and (name='" + pitcher +"' or alternatename like '%" + pitcher + "%');";
		std::vector<std::map<std::string, std::string>> res = DBWrapper::queryDatabase(db, query);
		if (res.empty()) {
			if (pitcher.compare("Shohei Ohtani") == 0) {
				query = "select id,throws from players where name='"+pitcher+"';";
				res = DBWrapper::queryDatabase(db, query);
			}
			else {
				++side;
				continue;
			}
		}

		int pitcherId = 0;
		std::string pitcherThrows = res[0]["throws"];
		try
		{
			pitcherId = std::stoi(res[0]["id"]);
		}
		catch (std::invalid_argument &iae)
		{
			std::cout << "Error: " << iae.what() << std::endl;
			pitcherId = std::numeric_limits<int>::min();
		}

		if (pitcherId == std::numeric_limits<int>::min())
		{
			++side;
			continue;
		}

		query = "select count(distinct gamedate) as numgames from PBP where gamedate like '2021-%' and gamedate < '"+datestr+"' and pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1";
		std::vector<std::map<std::string, std::string>> pUmpDateRes = DBWrapper::queryDatabase(db, query);

		int numGames = std::numeric_limits<int>::min();
		try {
			numGames = std::stoi(pUmpDateRes[0]["numgames"]);
		}
		catch (...) {
			numGames = std::numeric_limits<int>::min();
		}

        const int startYear = 2018;
        const int endYear = 2021;

        std::string querydatestr = "";
        for (int yr = startYear; yr <= endYear; ++yr) {
        	query = "select gamedate from (";
        	query += " select gamedate,row_number() over (order by gamedate) as gamenum from (";
        	query += " select distinct gamedate from PBP where gamedate like '"+std::to_string(yr)+"-%' and gamedate < '"+datestr+"' and pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1))";
        	query += " where gamenum="+std::to_string(numGames+1)+";";

        	std::vector<std::map<std::string, std::string>> gdRes = DBWrapper::queryDatabase(db, query);

        	if (!gdRes.empty()) {
        		if (!querydatestr.empty()) {
        			querydatestr += ",";
        		}

        		querydatestr += "'"+gdRes[0]["gamedate"]+"'";
        	}
        }

        if (!querydatestr.empty()) {
    		std::stringstream ss;
    		ss.str("");

    		query = "select p.gamedate,p.inningtype,p.inningnum,p.batpos,p.hitterid,p.event";
    		query += " from PBP p";
    		query += " where p.gamedate in ("+querydatestr+")";
    		query += " and p.pitcherid="+std::to_string(pitcherId)+" and p.isHitterStarter=1";
    		query += " order by p.batpos;";

    		std::vector<std::map<std::string, std::string>> gameRes = DBWrapper::queryDatabase(db, query);

   			for (std::map<std::string, std::string> gr : gameRes) {
   				std::string hits = " ";
   				if (!gr["hitterid"].empty()) {
   					query = "select hits from players where id="+gr["hitterid"]+";";
   					std::vector<std::map<std::string, std::string>> hitsRes = DBWrapper::queryDatabase(db, query);

   					hits = hitsRes[0]["hits"];
   				}
   				ss << gr["gamedate"] << "," << gr["batpos"] << "," << hits << ","
   				   << gr["inningtype"] << "," << gr["inningnum"] << "," << gr["event"] << "\n";
   			}

    		std::cout << ss.str() << std::endl;
		}

		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
