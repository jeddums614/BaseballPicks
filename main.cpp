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

std::pair<std::pair<double, std::string>, std::pair<double, std::string>> printBabipStats(sqlite3* db, int hitterId, double targetBabip) {
	std::pair<std::pair<double, std::string>, std::pair<double, std::string>> retVal;

	std::string firstDateQuery = "select distinct gamedate from PBP where hitterid="+std::to_string(hitterId)+" limit 1;";
	std::vector<std::map<std::string, std::string>> firstDateRes = DBWrapper::queryDatabase(db, firstDateQuery);
	std::string firstDate = "";
	if (!firstDateRes.empty()) {
		firstDate = firstDateRes[0]["gamedate"];
	}
	else {
		firstDate = "2021-04-01";
	}
	std::string query = "select distinct gamedate from PBP where hitterid="+std::to_string(hitterId)+" and gamedate > '"+firstDate+"'";
	std::vector<std::map<std::string, std::string>> dateColl = DBWrapper::queryDatabase(db, query);

	for (std::map<std::string, std::string> datemap : dateColl) {
		std::string numHitQuery = "select count(*) from PBP where gamedate < '"+datemap["gamedate"] + "' and gamedate >= (select date('"+datemap["gamedate"]+"', '-7 day')) and hitterid="+std::to_string(hitterId)+" and isPitcherStarter=1 and isHitterStarter=1 and event > 0";
		std::vector<std::map<std::string, std::string>> tmp = DBWrapper::queryDatabase(db, numHitQuery);
		int numHits = std::stoi(tmp[0]["count(*)"]);
		std::string numHrQuery = "select count(*) from PBP where gamedate < '"+datemap["gamedate"] + "' and gamedate >= (select date('"+datemap["gamedate"]+"', '-7 day')) and hitterid="+std::to_string(hitterId)+" and isPitcherStarter=1 and isHitterStarter=1 and event=4;";
		tmp = DBWrapper::queryDatabase(db, numHrQuery);
		int numHr = std::stoi(tmp[0]["count(*)"]);
		std::string numSfQuery = "select count(*) from PBP where gamedate < '"+datemap["gamedate"] + "' and gamedate >= (select date('"+datemap["gamedate"]+"', '-7 day')) and hitterid="+std::to_string(hitterId)+" and isPitcherStarter=1 and isHitterStarter=1 and event=-2;";
		tmp = DBWrapper::queryDatabase(db, numSfQuery);
		int numSf = std::stoi(tmp[0]["count(*)"]);
		std::string numStrikeoutQuery = "select count(*) from PBP where gamedate < '"+datemap["gamedate"] + "' and gamedate >= (select date('"+datemap["gamedate"]+"', '-7 day')) and hitterid="+std::to_string(hitterId)+" and isPitcherStarter=1 and isHitterStarter=1 and count like '%-3';";
		tmp = DBWrapper::queryDatabase(db, numStrikeoutQuery);
		int numStrikeouts = std::stoi(tmp[0]["count(*)"]);
		std::string numAtBatQuery = "select count(*) from PBP where gamedate < '"+datemap["gamedate"] + "' and gamedate >= (select date('"+datemap["gamedate"]+"', '-7 day')) and hitterid="+std::to_string(hitterId)+" and isPitcherStarter=1 and isHitterStarter=1 and event >= 0;";
		tmp = DBWrapper::queryDatabase(db, numAtBatQuery);
		int numAtBats = std::stoi(tmp[0]["count(*)"]);

		double denominator = numAtBats - numHr - numStrikeouts + numSf;
		double babip = 0;
		if (denominator > 0) {
			babip = (numHits-numHr)/denominator;
		}

		std::string hitQuery = "select count(*) from PBP where gamedate = '"+datemap["gamedate"] + "' and hitterid="+std::to_string(hitterId)+" and isPitcherStarter=1 and isHitterStarter=1 and event > 0;";
		std::vector<std::map<std::string, std::string>> dayHitRes = DBWrapper::queryDatabase(db, hitQuery);
		std::string hitResult = "";

		if (dayHitRes[0]["count(*)"][0] != '0') {
			hitResult = "Y";
		}
		else {
			hitResult = "N";
		}

		if (babip > targetBabip) {
			if (retVal.first.second.empty()) {
				retVal.first = std::make_pair(babip,hitResult);
			}
			else if (babip < retVal.first.first) {
			    retVal.first = std::make_pair(babip,hitResult);
			}
		}
		else if (babip < targetBabip) {
			if (retVal.second.second.empty()) {
				retVal.second = std::make_pair(babip,hitResult);
			}
			else if (babip > retVal.second.first) {
				retVal.second = std::make_pair(babip,hitResult);
			}
		}
	}

	return retVal;
}

int main() {
	sqlite3* db;

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

	sqlite3_open("baseball.db", &db);

	if (db == NULL)
	{
	    std::cout << "Failed to open DB" << std::endl;
		return (1);
	}

	std::ifstream ifs("todaymatchups.txt");
	std::string line = "";

	while (std::getline(ifs, line)) {
		std::cout << line << std::endl;
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
			continue;
		}

		std::string matchStr = "";

		std::string puDateQuery = "select distinct gamedate,inningtype from PBP where pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1 and event >= 0 and inningnum <= 7 order by gamedate desc;";
		std::vector<std::map<std::string, std::string>> puDateResults = DBWrapper::queryDatabase(db, puDateQuery);

		if (!puDateResults.empty()) {
			matchStr += "pu [" + puDateResults[0]["inningtype"] + "]";
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
			int hitterid = std::numeric_limits<int>::min();
			try {
				hitterid = std::stoi(hitter["id"]);
			}
			catch (...) {
				hitterid = std::numeric_limits<int>::min();
			}

			if (hitterid > 0) {
				query = "select distinct gamedate from PBP where hitterid="+hitter["id"]+" and umpire='"+umpire+"' and event >= 0 and inningnum <= 7 order by gamedate desc limit 1";
				std::vector<std::map<std::string, std::string>> huDateRes = DBWrapper::queryDatabase(db, query);

				if (!huDateRes.empty()) {
					std::string huDate = huDateRes[0]["gamedate"];

					query = "select inningtype,inningnum,batpos from PBP p inner join players pitch on pitch.id=p.pitcherid where p.gamedate='"+huDate+"' and p.hitterid="+hitter["id"]+" and p.isHitterStarter=1 and p.isPitcherStarter=1 and pitch.throws='"+pitcherThrows+"' and p.inningnum <= 7 and p.event > 0;";
					std::vector<std::map<std::string, std::string>> huHitRes = DBWrapper::queryDatabase(db, query);

					if (!huHitRes.empty()) {
						if (!matchStr.empty()) {
							matchStr += ",";
						}
						matchStr += "hu [" + huHitRes[0]["inningtype"] + "," + huHitRes[0]["batpos"]+"]";
					}
					else {
						continue;
					}
				}

				query = "select distinct gamedate from PBP where hitterid="+hitter["id"] + " and pitcherid="+std::to_string(pitcherId)+" and event >= 0 and isHitterStarter=1 and isPitcherStarter=1 order by gamedate desc limit 1;";
				std::vector<std::map<std::string, std::string>> hpDateRes = DBWrapper::queryDatabase(db, query);

				if (!hpDateRes.empty()) {
					std::string hpDate = hpDateRes[0]["gamedate"];
					query = "select distinct inningtype,inningnum,batpos from PBP where hitterid="+hitter["id"] + " and pitcherid="+std::to_string(pitcherId)+" and gamedate='"+hpDate+"' and event > 0 and inningnum <= 7;";
					std::vector<std::map<std::string, std::string>> hpHitRes = DBWrapper::queryDatabase(db, query);

					if (!hpHitRes.empty()) {
						if (!matchStr.empty()) {
							matchStr += ",";
						}
						matchStr += "hp [" + hpHitRes[0]["inningtype"] + "," + hpHitRes[0]["batpos"] + "]";
					}
					else {
						continue;
					}
				}

				std::string numHitQuery = "select count(*) from PBP where gamedate < '"+datestr + "' and gamedate >= (select date('"+datestr+"', '-7 day')) and hitterid="+std::to_string(hitterid)+" and isPitcherStarter=1 and isHitterStarter=1 and event > 0";
				std::vector<std::map<std::string, std::string>> tmp = DBWrapper::queryDatabase(db, numHitQuery);
				int numHits = std::stoi(tmp[0]["count(*)"]);
				std::string numHrQuery = "select count(*) from PBP where gamedate < '"+datestr + "' and gamedate >= (select date('"+datestr+"', '-7 day')) and hitterid="+std::to_string(hitterid)+" and isPitcherStarter=1 and isHitterStarter=1 and event=4;";
				tmp = DBWrapper::queryDatabase(db, numHrQuery);
				int numHr = std::stoi(tmp[0]["count(*)"]);
				std::string numSfQuery = "select count(*) from PBP where gamedate < '"+datestr + "' and gamedate >= (select date('"+datestr+"', '-7 day')) and hitterid="+std::to_string(hitterid)+" and isPitcherStarter=1 and isHitterStarter=1 and event=-2;";
				tmp = DBWrapper::queryDatabase(db, numSfQuery);
				int numSf = std::stoi(tmp[0]["count(*)"]);
				std::string numStrikeoutQuery = "select count(*) from PBP where gamedate < '"+datestr + "' and gamedate >= (select date('"+datestr+"', '-7 day')) and hitterid="+std::to_string(hitterid)+" and isPitcherStarter=1 and isHitterStarter=1 and count like '%-3';";
				tmp = DBWrapper::queryDatabase(db, numStrikeoutQuery);
				int numStrikeouts = std::stoi(tmp[0]["count(*)"]);
				std::string numAtBatQuery = "select count(*) from PBP where gamedate < '"+datestr + "' and gamedate >= (select date('"+datestr+"', '-7 day')) and hitterid="+std::to_string(hitterid)+" and isPitcherStarter=1 and isHitterStarter=1 and event >= 0;";
				tmp = DBWrapper::queryDatabase(db, numAtBatQuery);
				int numAtBats = std::stoi(tmp[0]["count(*)"]);

				double todaydenominator = numAtBats - numHr - numStrikeouts + numSf;
				double todaybabip = 0;
				if (todaydenominator > 0) {
					todaybabip = (numHits-numHr)/todaydenominator;
				}

			    std::pair<std::pair<double, std::string>, std::pair<double, std::string>> bounds = printBabipStats(db,hitterid,todaybabip);

			    if (!bounds.first.second.empty() && bounds.first.second.compare(bounds.second.second) == 0 && bounds.first.second[0] == 'Y' && !matchStr.empty()) {
			    	std::cout << hitter["name"];
				    if (!matchStr.empty()) {
					    std::cout << " (" << matchStr << ")";
				    }

				    std::cout << " - " << bounds.first.second << "," <<bounds.second.second << std::endl;
				    //std::cout << (std::round( bounds.first.first * 1000.0 ) / 1000.0) << "," << (std::round(todaybabip * 1000.0) / 1000.0) << "," << (std::round( bounds.second.first * 1000.0 ) / 1000.0) << std::endl;
				}
			}
		}
	}

	sqlite3_close(db);

	return (0);
}
