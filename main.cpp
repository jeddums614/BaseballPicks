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

		std::vector<Lineup> lineups;

		query = "select gamedate,inningtype from PBP where pitcherid="+std::to_string(pitcherId)+" and (awayteam='"+opponent+"' or hometeam='"+opponent+"') and isPitcherStarter=1 order by gamedate desc limit 1;";
		std::vector<std::map<std::string, std::string>> pOppDateQuery = DBWrapper::queryDatabase(db, query);

		std::string pOppDate = !pOppDateQuery.empty() ? pOppDateQuery[0]["gamedate"] : "";

		if (!pOppDate.empty()) {
			std::string innType = pOppDateQuery[0]["inningtype"];
			Lineup hpLineup("hp: "+pOppDate, innType);

			for (int i = 1; i <= 9; ++i) {
				Hitter hitter;
				query = "select hitterid from PBP where gamedate='"+pOppDate+"' and pitcherid="+std::to_string(pitcherId)+" and isHitterStarter=1 and batpos="+std::to_string(i)+" and inningtype='"+pOppDateQuery[0]["inningtype"]+"';";
				std::vector<std::map<std::string, std::string>> hitterQuery = DBWrapper::queryDatabase(db, query);

				if (hitterQuery.empty()) {
					hitter.batpos = i;
					hitter.hits = "?";
					hitter.gotHit = '?';
					hpLineup.addHitter(hitter);
				    continue;
				}

				query = "select hits from players where id="+hitterQuery[0]["hitterid"]+";";
				std::vector<std::map<std::string, std::string>> sideQuery = DBWrapper::queryDatabase(db, query);

				query = "select count(*) from PBP where gamedate='"+pOppDate+"' and pitcherid="+std::to_string(pitcherId)+" and isHitterStarter=1 and batpos="+std::to_string(i)+" and event > 0 and inningtype='"+pOppDateQuery[0]["inningtype"]+"' and inningnum <= 7;";
				std::vector<std::map<std::string, std::string>> posHitQuery = DBWrapper::queryDatabase(db, query);
				std::string lineupHits = sideQuery[0]["hits"];
				if (lineupHits.compare("S") == 0) {
					if (pitcherThrows.compare("R") == 0) {
						lineupHits = "L";
					}
					else {
						lineupHits = "R";
					}
				}

				int numHits = std::stoi(posHitQuery[0]["count(*)"]);
				if (numHits > 0) {
					hitter.batpos = i;
					hitter.gotHit = 'Y';
					hitter.hits = lineupHits;
				}
				else {
					hitter.batpos = i;
					hitter.gotHit = 'N';
					hitter.hits = lineupHits;
			    }
				hpLineup.addHitter(hitter);
			}

			lineups.push_back(hpLineup);
		}

		query = "select gamedate,inningtype from PBP where pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1 order by gamedate desc limit 1;";
		std::vector<std::map<std::string, std::string>> pUmpDateQuery = DBWrapper::queryDatabase(db, query);

		std::string pUmpDate = !pUmpDateQuery.empty() ? pUmpDateQuery[0]["gamedate"] : "";

		if (!pUmpDate.empty()) {
			std::string innType = pUmpDateQuery[0]["inningtype"];

			Lineup puLineup("pu: "+pUmpDate,innType);
			for (int i = 1; i <= 9; ++i) {
				Hitter hitter;
				query = "select hitterid from PBP where gamedate='"+pUmpDate+"' and umpire='"+umpire+"' and isHitterStarter=1 and batpos="+std::to_string(i)+" and inningtype='"+pUmpDateQuery[0]["inningtype"]+"' and inningnum <= 7;";
				std::vector<std::map<std::string, std::string>> hitterQuery = DBWrapper::queryDatabase(db, query);
				if (hitterQuery.empty()) {
					hitter.batpos = i;
					hitter.gotHit = '?';
					hitter.hits = "?";
					puLineup.addHitter(hitter);
					continue;
				}

				query = "select hits from players where id="+hitterQuery[0]["hitterid"]+";";
				std::vector<std::map<std::string, std::string>> sideQuery = DBWrapper::queryDatabase(db, query);

				query = "select count(*) from PBP where gamedate='"+pUmpDate+"' and umpire='"+umpire+"' and isHitterStarter=1 and batpos="+std::to_string(i)+" and event > 0 and inningtype='"+pUmpDateQuery[0]["inningtype"]+"' and inningnum <= 7 and isPitcherStarter=1;";
				std::vector<std::map<std::string, std::string>> posHitQuery = DBWrapper::queryDatabase(db, query);
				std::string lineupHits = sideQuery[0]["hits"];
				if (lineupHits.compare("S") == 0) {
					if (pitcherThrows.compare("R") == 0) {
						lineupHits = "L";
					}
					else {
						lineupHits = "R";
					}
				}

				int numHits = std::stoi(posHitQuery[0]["count(*)"]);
				if (numHits > 0) {
					hitter.batpos = i;
					hitter.gotHit = 'Y';
					hitter.hits = lineupHits;
				}
				else {
				    hitter.batpos = i;
				    hitter.gotHit = 'N';
				    hitter.hits = lineupHits;
				}

				puLineup.addHitter(hitter);
			}

			lineups.push_back(puLineup);
		}

		query = "select p.gamedate,p.awayteam,p.hometeam,pi.throws from PBP p inner join players pi on pi.id=p.pitcherid where p.umpire='"+umpire+"' and (p.awayteam='"+opponent+"' or p.hometeam='"+opponent+"') and p.isPitcherStarter=1 order by p.gamedate desc limit 1;";
		std::vector<std::map<std::string, std::string>> uOppDateQuery = DBWrapper::queryDatabase(db, query);

		std::string uOppDate = !uOppDateQuery.empty() ? uOppDateQuery[0]["gamedate"] : "";
		std::string pThrows = !uOppDateQuery.empty() ? uOppDateQuery[0]["throws"] : "";

		if (!uOppDate.empty() && (!pThrows.empty() && pThrows[0] == pitcherThrows[0])) {
			std::string innType = uOppDateQuery[0]["awayteam"].compare(opponent) == 0 ? "t" : "b";

			Lineup huLineup("hu: "+uOppDate,innType);
			for (int i = 1; i <= 9; ++i) {
				Hitter hitter;
				query = "select hitterid from PBP where gamedate='"+uOppDate+"' and (awayteam='"+opponent+"' or hometeam='"+opponent+"') and isHitterStarter=1 and batpos="+std::to_string(i)+" and inningtype='"+innType+"';";

				std::vector<std::map<std::string, std::string>> hitterQuery = DBWrapper::queryDatabase(db, query);
				if (hitterQuery.empty()) {
					hitter.batpos = i;
					hitter.gotHit = '?';
					hitter.hits = "?";
					huLineup.addHitter(hitter);
					continue;
				}

				query = "select hits from players where id="+hitterQuery[0]["hitterid"]+";";
				std::vector<std::map<std::string, std::string>> sideQuery = DBWrapper::queryDatabase(db, query);

				query = "select count(*) from PBP where gamedate='"+uOppDate+"' and (awayteam='"+opponent+"' or hometeam='"+opponent+"') and isHitterStarter=1 and batpos="+std::to_string(i)+" and event > 0 and inningtype='"+innType+"' and inningnum <= 7;";
				std::vector<std::map<std::string, std::string>> posHitQuery = DBWrapper::queryDatabase(db, query);
				std::string lineupHits = sideQuery[0]["hits"];
				if (lineupHits.compare("S") == 0) {
					if (pitcherThrows.compare("R") == 0) {
						lineupHits = "L";
					}
					else {
						lineupHits = "R";
					}
				}

				int numHits = std::stoi(posHitQuery[0]["count(*)"]);
				if (numHits > 0) {
					hitter.batpos = i;
					hitter.gotHit = 'Y';
					hitter.hits = lineupHits;
				}
				else {
					hitter.batpos = i;
					hitter.gotHit = 'N';
					hitter.hits = lineupHits;
				}

				huLineup.addHitter(hitter);
			}

		    lineups.push_back(huLineup);
		}

		if (lineups.size() == 3) {
			for (Lineup l : lineups) {
				std::cout << l << std::endl;
			}
		}

		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
