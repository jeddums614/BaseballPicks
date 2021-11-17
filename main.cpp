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
#include <cstring>
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
	int side = 0;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-d") == 0) {
			datestr = argv[++i];
		}
		else if (strcmp(argv[i], "-s") == 0) {
			if (strcmp(argv[i+1], "home") == 0) {
				side = 1;
			}
			else if (strcmp(argv[i+1], "away") == 0) {
				side = 2;
			}
		}
	}

	int year = std::numeric_limits<int>::min();
	if (datestr.empty()) {
	    auto todaydate = std::chrono::system_clock::now();
	    std::time_t now_c = std::chrono::system_clock::to_time_t(todaydate);
	    std::tm now_tm = *std::localtime(&now_c);
	    year = now_tm.tm_year+1900;
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
	else {
		try {
		    year = std::stoi(datestr.substr(0,4));
		}
		catch (...) {
			year = std::numeric_limits<int>::min();
		}
	}

	if (year <= 0) {
		std::cout << "invalid year in date string" << std::endl;
		return -3;
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

	std::stringstream dirpath;
	dirpath.str(std::string());

	dirpath << "Results/" << year << "/";
	if (!std::filesystem::exists(dirpath.str()))
	{
		std::filesystem::create_directories(dirpath.str());
	}

	std::ofstream ofs(dirpath.str()+datestr+".csv");

	std::ifstream ifs("todaymatchups.txt");
	std::string line = "";

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
		std::size_t appos = pitcher.find("'");
		if (appos != std::string::npos) {
			pitcher.insert(appos, "'");
		}
		std::string opponent = gameParts[1];
		std::string umpire = gameParts[2];
		appos = umpire.find("'");
		if (appos != std::string::npos) {
		    umpire.insert(appos, "'");
		}
		std::string gameNumber = gameParts[3];
		std::string dayNight = gameParts[4];

		std::string query = "select id,throws,team from players where position='P' and (name='" + pitcher +"' or alternatename like '%" + pitcher + "%');";
		std::vector<std::map<std::string, std::string>> res = DBWrapper::queryDatabase(db, query);
		if (res.empty()) {
			if (pitcher.compare("Shohei Ohtani") == 0) {
				query = "select id,throws,team from players where name='"+pitcher+"';";
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

		std::string pitcherTeam = res[0]["team"];
		if (res[0]["team"].rfind(":") != std::string::npos) {
			pitcherTeam = res[0]["team"].substr(res[0]["team"].rfind(":")+1);
		}

		if (pitcherId == std::numeric_limits<int>::min())
		{
			++side;
			continue;
		}

    	query = "select id,name from players where team like '%"+opponent+"' and position != 'P';";
    	std::vector<std::map<std::string, std::string>> hitterList = DBWrapper::queryDatabase(db, query);

    	for (std::map<std::string, std::string> hitter : hitterList) {
    		query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and ph.gamenumber="+gameNumber+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.hitterid="+hitter["id"]+" and pd.pitcherid="+std::to_string(pitcherId)+" and pd.throws='"+pitcherThrows+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.event >= 0;";
    		std::vector<std::map<std::string, std::string>> queryRes = DBWrapper::queryDatabase(db, query);

    		double atbats = 0;
    		try {
    			atbats = std::stod(queryRes[0]["count(*)"]);
    		}
    		catch (...) {
    			continue;
    		}

    		query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and ph.gamenumber="+gameNumber+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.hitterid="+hitter["id"]+" and pd.pitcherid="+std::to_string(pitcherId)+" and pd.throws='"+pitcherThrows+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.event > 0;";
    		queryRes = DBWrapper::queryDatabase(db, query);

    		double hits = 0;
    		try {
    			hits = std::stod(queryRes[0]["count(*)"]);
    		}
    		catch (...) {
    			continue;
    		}

    		query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and ph.gamenumber="+gameNumber+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.hitterid="+hitter["id"]+" and pd.pitcherid="+std::to_string(pitcherId)+" and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.count like '%-3';";
    		queryRes = DBWrapper::queryDatabase(db, query);

    		double strikeouts = 0;
    		try {
    			strikeouts = std::stod(queryRes[0]["count(*)"]);
    		}
    		catch (...) {
    			continue;
    		}

    		query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and ph.gamenumber="+gameNumber+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.hitterid="+hitter["id"]+" and pd.pitcherid="+std::to_string(pitcherId)+" and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.event = 4;";
    		queryRes = DBWrapper::queryDatabase(db, query);

    		double homeruns = 0;
    		try {
    			homeruns = std::stod(queryRes[0]["count(*)"]);
    		}
    		catch (...) {
    			continue;
    		}

    		query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and ph.gamenumber="+gameNumber+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.hitterid="+hitter["id"]+" and pd.pitcherid="+std::to_string(pitcherId)+" and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.event = -2;";
    		queryRes = DBWrapper::queryDatabase(db, query);

    		double sacflies = 0;
    		try {
    			sacflies = std::stod(queryRes[0]["count(*)"]);
    		}
    		catch (...) {
    			continue;
    		}

    		double denominator = atbats - homeruns - strikeouts + sacflies;
    		double babip = std::numeric_limits<double>::min();

    		if (denominator > 0) {
    			babip = (hits - homeruns)/denominator;

    			ofs << datestr << "," << hitter["name"]
							   << "," << pitcher
							   << "," << opponent
							   << "," << pitcherTeam
							   << "," << tmType
							   << "," << dayNight
							   << "," << gameNumber
							   << "," << std::fixed << std::setprecision(3) << babip << "\n";
    		}
    		else {
    			ofs << datestr << "," << hitter["name"]
							   << "," << pitcher
							   << "," << opponent
							   << "," << pitcherTeam
							   << "," << tmType
							   << "," << dayNight
							   << "," << gameNumber
							   << "," << std::numeric_limits<double>::infinity() << "\n";
    		}
    	}

		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
