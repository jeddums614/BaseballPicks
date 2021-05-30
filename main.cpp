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

	const std::string outputFile = "Results/"+datestr+".csv";
	std::ofstream ofs(outputFile);
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
		std::string ballpark = gameParts[3];
		std::string gametime = gameParts[4];

		if(umpire.empty() || pitcher.empty()) {
			continue;
		}

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

		std::string puDateQuery = "select distinct gamedate from PBP where pitcherid="+std::to_string(pitcherId)+" and umpire='"+umpire+"' and isPitcherStarter=1 and event >= 0 order by gamedate desc limit 1;";
		std::vector<std::map<std::string, std::string>> puDateResults = DBWrapper::queryDatabase(db, puDateQuery);

		if (!puDateResults.empty()) {
			for (std::map<std::string, std::string> puDres : puDateResults) {
				std::string batposQuery = "select distinct p.batpos,h.hits from PBP p inner join players h on p.hitterid=h.id where p.pitcherid="+std::to_string(pitcherId)+" and p.gamedate='"+puDres["gamedate"]+"' and p.event > 0 order by p.batpos;";
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

					ofs << puDres["gamedate"] << "," << pitcher << "," << umpire << "," << pitcherThrows << ",\"" << batposOutput << "\"" << std::endl;

					ofs << std::endl;
				}
			}
		}

		query = "select name,id from players where position != 'P' and (team='" + opponent +"' or team like '%" + opponent + "');";
		res = DBWrapper::queryDatabase(db, query);

		for (std::map<std::string, std::string> row : res)
		{
			int hitterId = 0;
			try
			{
				hitterId = std::stoi(row["id"]);
			}
			catch (std::invalid_argument &iae)
			{
				std::cout << "Error: " << iae.what() << std::endl;
				hitterId = std::numeric_limits<int>::min();
			}

			if (hitterId == std::numeric_limits<int>::min())
			{
				continue;
			}

			std::string hpDateQuery = "select distinct gamedate from PBP where hitterid="+std::to_string(hitterId)+" and pitcherId="+std::to_string(pitcherId)+" and isHitterStarter=1 and isPitcherStarter=1 and event >= 0 order by gamedate desc limit 1;";
			std::vector<std::map<std::string, std::string>> hpDateResults = DBWrapper::queryDatabase(db, hpDateQuery);

			if (hpDateResults.empty() && puDateResults.empty()) {
				continue;
			}

			bool showResults = false;
			std::string hpdate = "";
			std::vector<std::map<std::string, std::string>> hpHitResults;
			if (!hpDateResults.empty()) {
				hpdate = hpDateResults[0]["gamedate"];

				std::string hpHitQuery = "select distinct inningtype,inningnum,batpos,gamenumber,event,gametime,isHitterStarter,isPitcherStarter from PBP where hitterid="+std::to_string(hitterId)+" and pitcherId="+std::to_string(pitcherId)+" and gamedate='"+hpdate+"' and isPitcherStarter=1 and isHitterStarter=1;";
				hpHitResults = DBWrapper::queryDatabase(db, hpHitQuery);

				showResults = std::any_of(hpHitResults.begin(), hpHitResults.end(), [](const std::map<std::string, std::string> &hpRes) {
					try {
						int evVal = std::stoi(hpRes.at("event"));
						return (evVal > 0);
					}
					catch (...) {
						return (false);
					}
				});

				if (!showResults) {
					std::cout << "hpShowResults = false, skipping " << row["name"] << std::endl;
					continue;
				}
			}

			std::string huDateQuery = "select distinct gamedate from PBP where hitterid="+std::to_string(hitterId)+" and umpire='"+umpire+"' and isHitterStarter=1 and event >= 0 order by gamedate desc limit 1;";
			std::vector<std::map<std::string, std::string>> huDateResults = DBWrapper::queryDatabase(db, huDateQuery);

			if (huDateResults.empty()) {
				continue;
			}

			std::string hudate = huDateResults[0]["gamedate"];

			std::string huHitQuery = "select distinct inningtype,inningnum,batpos,pitcherid,gamenumber,event,gametime,isHitterStarter,isPitcherStarter from PBP where hitterid="+std::to_string(hitterId)+" and umpire='"+umpire+"' and gamedate='"+hudate+"' and isHitterStarter=1 and isPitcherStarter=1;";
			std::vector<std::map<std::string, std::string>> huHitResults = DBWrapper::queryDatabase(db, huHitQuery);

			showResults = std::any_of(huHitResults.begin(), huHitResults.end(), [](const std::map<std::string, std::string> &huRes) {
				try {
					int evVal = std::stoi(huRes.at("event"));
					return (evVal > 0);
				}
				catch (...) {
					return (false);
				}
			});

			if (!showResults) {
				std::cout << "huShowResults = false, skipping " << row["name"] << std::endl;
				continue;
			}

			for (std::map<std::string, std::string> hpRow : hpHitResults) {
				ofs << hpdate << "," << row["name"] << "," << pitcher << "," << pitcherThrows << "," << hpRow["gametime"] << "," << hpRow["isHitterStarter"] << "," << hpRow["isPitcherStarter"] << "," << hpRow["inningtype"] << "," << hpRow["inningnum"] << "," << hpRow["batpos"] << "," << hpRow["event"] << std::endl;
			}

			for (std::map<std::string, std::string> huRow : huHitResults) {
				std::string throwQuery = "select distinct throws from players where id="+huRow["pitcherid"]+";";
				std::vector<std::map<std::string, std::string>> throwResult = DBWrapper::queryDatabase(db, throwQuery);
				ofs << hudate << "," << row["name"] << "," << umpire << "," << throwResult[0]["throws"] << "," << huRow["gametime"] << "," << huRow["isHitterStarter"] << "," << huRow["isPitcherStarter"] << "," << huRow["inningtype"] << "," << huRow["inningnum"] << "," << huRow["batpos"] << "," << huRow["event"] << std::endl;
			}

			ofs << std::endl;

			std::cout << row["name"] << std::endl;
		}
	}

	sqlite3_close(db);

	ofs.close();
	if (std::filesystem::is_empty(outputFile)) {
		std::filesystem::remove(outputFile.c_str());
	}
	return (0);
}
