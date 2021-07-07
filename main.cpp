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
#include "Utils.h"
#include "Stats.h"

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

int main() {
	sqlite3* db = NULL;

	auto todaydate = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(todaydate);
	std::tm now_tm = *std::localtime(&now_c);
	int year = now_tm.tm_year+1900;
	std::string datestr = std::to_string(year) + "-";
	if (now_tm.tm_mon+1 < 10) {
		datestr += "0";
	}
	datestr += std::to_string(now_tm.tm_mon+1) + "-";
	if (now_tm.tm_mday < 10) {
		datestr += "0";
	}
	datestr += std::to_string(now_tm.tm_mday);
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

		if (gameParts.size() != 5) {
			std::cout << "Not all info filled out in todaymatchups.txt for this line" << std::endl;
			continue;
		}

		std::string pitcher = gameParts[0];
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

		std::string matchStr = "";

		std::string puDateQuery = "select distinct gamedate,inningtype from PBP where pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1 and event >= 0 and inningnum <= 7 order by gamedate desc;";
		std::vector<std::map<std::string, std::string>> puDateResults = DBWrapper::queryDatabase(db, puDateQuery);

		if (!puDateResults.empty()) {
			if (puDateResults.size() == 1 && ((tmType == teamType::AWAY && puDateResults[0]["inningtype"][0] == 't') ||
				(tmType == teamType::HOME && puDateResults[0]["inningtype"][0] == 'b'))) {
				matchStr += "pu [" + puDateResults[0]["inningtype"] + "]";
			}
			for (std::map<std::string, std::string> puDres : puDateResults) {
				std::string batposQuery = "select distinct p.batpos,h.hits from PBP p inner join players h on p.hitterid=h.id where p.pitcherid="+std::to_string(pitcherId)+" and p.gamedate='"+puDres["gamedate"]+"' and p.isHitterStarter=1 and p.event > 0 and p.inningnum <= 7 order by p.batpos;";
				std::vector<std::map<std::string, std::string>> batposResults = DBWrapper::queryDatabase(db, batposQuery);

				std::cout << puDres["gamedate"] << " [" << puDres["inningtype"] << "] - ";

				std::string batposOutput = "";
				for (std::map<std::string, std::string> bpres : batposResults) {
					if (!batposOutput.empty()) {
						batposOutput += ",";
					}
					batposOutput += bpres["batpos"] + " (" + bpres["hits"] + ")";
				}

				std::cout << batposOutput << std::endl;
			}
		}

		query = "select distinct id,name from players where team like '%"+opponent+"' and position != 'P';";
		std::vector<std::map<std::string, std::string>> playerList = DBWrapper::queryDatabase(db, query);

		std::string saveVal = matchStr;

		for (std::map<std::string, std::string> hitter : playerList) {
			matchStr = saveVal;
			Stats hitterStats = {0, 0, 0, 0, 0};
			int hitterid = std::numeric_limits<int>::min();
			try {
				hitterid = std::stoi(hitter["id"]);
			}
			catch (...) {
				hitterid = std::numeric_limits<int>::min();
			}

			if (hitterid > 0) {
				query = "select hits from players where id="+hitter["id"]+";";
				std::vector<std::map<std::string, std::string>> pHitQuery = DBWrapper::queryDatabase(db, query);

				std::string hitterHits = pHitQuery[0]["hits"];
				query = "select distinct gamedate,inningtype from PBP where hitterid="+hitter["id"]+" and umpire='"+umpire+"' and event >= 0 and inningnum <= 7 and isHitterStarter=1 and isPitcherStarter=1 order by gamedate desc limit 1";
				std::vector<std::map<std::string, std::string>> huDateRes = DBWrapper::queryDatabase(db, query);

				if (!huDateRes.empty()) {
					std::string huDate = huDateRes[0]["gamedate"];

					query = "select p.inningtype,p.batpos from PBP p inner join players pitch on pitch.id=p.pitcherid where p.hitterid="+hitter["id"]+" and p.gamedate='"+huDate+"' and p.event = -2 and p.isPitcherStarter=1 and p.isHitterStarter=1 and pitch.throws='"+pitcherThrows+"';";
					std::vector<std::map<std::string, std::string>> sfRes = DBWrapper::queryDatabase(db, query);

					hitterStats.sacflies += sfRes.size();

					query = "select p.inningtype,p.batpos from PBP p inner join players pitch on pitch.id=p.pitcherid where p.hitterid="+hitter["id"]+" and p.gamedate='"+huDate+"' and p.event=4 and p.isPitcherStarter=1 and p.isHitterStarter=1 and pitch.throws='"+pitcherThrows+"';";
					std::vector<std::map<std::string, std::string>> hrRes = DBWrapper::queryDatabase(db, query);

					hitterStats.homeruns += hrRes.size();

					query = "select p.inningtype,p.batpos from PBP p inner join players pitch on pitch.id=p.pitcherid where p.hitterid="+hitter["id"]+" and p.gamedate='"+huDate+"' and p.event >= 0 and p.isPitcherStarter=1 and p.isHitterStarter=1 and pitch.throws='"+pitcherThrows+"';";
					std::vector<std::map<std::string, std::string>> abRes = DBWrapper::queryDatabase(db, query);

					hitterStats.atbats += abRes.size();

					query = "select p.inningtype,p.batpos from PBP p inner join players pitch on pitch.id=p.pitcherid where p.hitterid="+hitter["id"]+" and p.gamedate='"+huDate+"' and p.count like '%-3' and p.isPitcherStarter=1 and p.isHitterStarter=1 and pitch.throws='"+pitcherThrows+"';";
					std::vector<std::map<std::string, std::string>> kRes = DBWrapper::queryDatabase(db, query);

					hitterStats.strikeouts += kRes.size();

					query = "select p.inningtype,p.batpos from PBP p inner join players pitch on pitch.id=p.pitcherid where p.hitterid="+hitter["id"]+" and p.gamedate='"+huDate+"' and p.event > 0 and p.isPitcherStarter=1 and p.isHitterStarter=1 and pitch.throws='"+pitcherThrows+"';";
					std::vector<std::map<std::string, std::string>> huHitRes = DBWrapper::queryDatabase(db, query);

					hitterStats.hits += huHitRes.size();

					if ((tmType == teamType::AWAY && huDateRes[0]["inningtype"][0] == 't')|| (tmType == teamType::HOME && huDateRes[0]["inningtype"][0] == 'b')) {
						if (!huHitRes.empty()) {
						    if (!matchStr.empty()) {
							    matchStr += ",";
						    }
						    matchStr += "hu [" + huHitRes[0]["inningtype"] + "," + huHitRes[0]["batpos"]+"," + hitterHits +"]";
					    }
					    else {
					        continue;
					    }
					}
				}

				query = "select distinct gamedate,inningtype from PBP where hitterid="+hitter["id"] + " and pitcherid="+std::to_string(pitcherId)+" and event >= 0 and isHitterStarter=1 and isPitcherStarter=1 order by gamedate desc limit 1;";
				std::vector<std::map<std::string, std::string>> hpDateRes = DBWrapper::queryDatabase(db, query);

				if (!hpDateRes.empty()) {
					std::string hpDate = hpDateRes[0]["gamedate"];

					query = "select inningtype,batpos from PBP where hitterid="+hitter["id"]+" and pitcherid="+std::to_string(pitcherId)+" and gamedate='"+hpDate+"' and event = -2;";
					std::vector<std::map<std::string, std::string>> sfRes = DBWrapper::queryDatabase(db, query);

					hitterStats.sacflies += sfRes.size();

					query = "select inningtype,batpos from PBP where hitterid="+hitter["id"]+" and pitcherid="+std::to_string(pitcherId)+" and gamedate='"+hpDate+"' and event=4;";
					std::vector<std::map<std::string, std::string>> hrRes = DBWrapper::queryDatabase(db, query);

					hitterStats.homeruns += hrRes.size();

					query = "select inningtype,batpos from PBP where hitterid="+hitter["id"]+" and pitcherid="+std::to_string(pitcherId)+" and gamedate='"+hpDate+"' and event >= 0;";
					std::vector<std::map<std::string, std::string>> abRes = DBWrapper::queryDatabase(db, query);

					hitterStats.atbats += abRes.size();

					query = "select inningtype,batpos from PBP where hitterid="+hitter["id"]+" and pitcherid="+std::to_string(pitcherId)+" and gamedate='"+hpDate+"' and count like '%-3';";
					std::vector<std::map<std::string, std::string>> kRes = DBWrapper::queryDatabase(db, query);

					hitterStats.strikeouts += kRes.size();

					query = "select inningtype,batpos from PBP where hitterid="+hitter["id"]+" and pitcherid="+std::to_string(pitcherId)+" and gamedate='"+hpDate+"' and event > 0;";
					std::vector<std::map<std::string, std::string>> hpHitRes = DBWrapper::queryDatabase(db, query);

					hitterStats.hits += hpHitRes.size();
					if ((tmType == teamType::AWAY && hpDateRes[0]["inningtype"][0] == 't')|| (tmType == teamType::HOME && hpDateRes[0]["inningtype"][0] == 'b')) {
					    if (!hpHitRes.empty()) {
						    if (!matchStr.empty()) {
							    matchStr += ",";
							}
							matchStr += "hp [" + hpHitRes[0]["inningtype"] + "," + hpHitRes[0]["batpos"] + "," + hitterHits + "]";
					    }
					    else {
					        continue;
					    }
				    }
				}

				double todaydenominator = hitterStats.atbats - hitterStats.homeruns - hitterStats.strikeouts + hitterStats.sacflies;
				double todaybabip = 0;
				if (todaydenominator > 0) {
					todaybabip = (hitterStats.hits - hitterStats.homeruns)/todaydenominator;
				}

			    if (!matchStr.empty() && todaybabip > 0) {
			    	std::cout << hitter["name"] << " (" << matchStr << ") " << todaybabip << std::endl;
				}
			}
		}
		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
