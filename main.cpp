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

int main() {
    const std::array<char, 3> handArr = {'R','L','S'};
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

	int side = 0;

	std::ifstream ifs("todaymatchups.txt");
	std::string line = "";

	while (std::getline(ifs, line)) {
		std::cout << line << std::endl;
		std::vector<std::string> gameParts = Utils::split(line);

		if (gameParts.size() != 5) {
			std::cout << "Not all info filled out in todaymatchups.txt for this line" << std::endl;
			++side;
			continue;
		}

		std::string pitcher = gameParts[0];
		std::string opponent = gameParts[1];
		std::string umpire = gameParts[2];
		std::string ballpark = gameParts[3];
		std::string gametime = gameParts[4];

		/*if(umpire.empty() || pitcher.empty()) {
			++side;
			continue;
		}*/

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

		std::string puDateQuery = "select distinct gamedate,awayteam from PBP where pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1 and event >= 0 order by gamedate desc;";
		std::vector<std::map<std::string, std::string>> puDateResults = DBWrapper::queryDatabase(db, puDateQuery);

		if (!puDateResults.empty()) {
			for (std::map<std::string, std::string> puDres : puDateResults) {
				std::string batposQuery = "select distinct p.batpos,h.hits from PBP p inner join players h on p.hitterid=h.id where p.pitcherid="+std::to_string(pitcherId)+" and p.gamedate='"+puDres["gamedate"]+"' and p.isHitterStarter=1 and p.event > 0 order by p.batpos;";
				std::vector<std::map<std::string, std::string>> batposResults = DBWrapper::queryDatabase(db, batposQuery);

				if (!batposResults.empty()) {
					std::cout << puDres["gamedate"] << " - ";

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
		}

		teamType tType = (side % 2 == 0 ? teamType::AWAY : teamType::HOME);

		for (int n = 1; n <= 9; ++n) {
			std::map<char, int> numHands;
			for (char hand : handArr) {
				query = "select p.gamedate,p.inningtype,p.inningnum,p.batpos,h.hits,p.event from PBP p inner join players h on h.id=p.hitterid where p.pitcherid="+std::to_string(pitcherId)+" and p.batpos="+std::to_string(n)+" and p.inningtype='"+(tType == teamType::AWAY ? "t" : "b")+"' and h.hits='"+hand+"' and p.isHitterStarter=1 order by p.gamedate,p.batpos;";
				res = DBWrapper::queryDatabase(db, query);
				std::map<std::string, int> dates;
				std::map<std::string, int> numHits;

				for (std::map<std::string, std::string> data : res) {
					int val = std::numeric_limits<int>::min();
					try {
						val = std::stoi(data["event"]);
					}
					catch (...) {
						val = std::numeric_limits<int>::min();
					}

					if (val >= 0) {
						++dates[data["gamedate"]];
						if (val > 0) {
							++numHits[data["gamedate"]];
						}
					}
				}

				if (dates.size() > 0 && dates.size() == numHits.size()) {
					++numHands[hand];
				}
				else {
					numHands.erase(hand);
				}

			}

			if (numHands.size() == handArr.size()) {
				std::cout << n << std::endl;
			}
		}
	}

	sqlite3_close(db);

	return (0);
}
