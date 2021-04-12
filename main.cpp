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

	enum class teamType { HOME, AWAY };
	int  side = 0;
	teamType tmType;
	while (std::getline(ifs, line)) {
		std::cout << line << std::endl;
		std::vector<std::string> gameParts = Utils::split(line);

		++side;
		if (side % 2 == 0) {
			tmType = teamType::HOME;
		}
		else {
			tmType = teamType::AWAY;
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
			continue;
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

			if (hpDateResults.empty()) {
				continue;
			}

			std::string hpdate = hpDateResults[0]["gamedate"];

			std::string hpHitQuery = "select distinct inningtype,inningnum,batpos from PBP where hitterid="+std::to_string(hitterId)+" and pitcherId="+std::to_string(pitcherId)+" and gamedate='"+hpdate+"' and event > 0;";
			std::vector<std::map<std::string, std::string>> hpHitResults = DBWrapper::queryDatabase(db, hpHitQuery);

			if (hpHitResults.empty()) {
				continue;
			}

			std::string huDateQuery = "select distinct gamedate from PBP where hitterid="+std::to_string(hitterId)+" and umpire='"+umpire+"' and isHitterStarter=1 and event >= 0 order by gamedate desc limit 1;";
			std::vector<std::map<std::string, std::string>> huDateResults = DBWrapper::queryDatabase(db, huDateQuery);

			if (huDateResults.empty()) {
				continue;
			}

			std::string hudate = huDateResults[0]["gamedate"];

			std::string huHitQuery = "select distinct inningtype,inningnum,batpos,pitcherid from PBP where hitterid="+std::to_string(hitterId)+" and umpire='"+umpire+"' and gamedate='"+hudate+"' and event > 0;";
			std::vector<std::map<std::string, std::string>> huHitResults = DBWrapper::queryDatabase(db, huHitQuery);

			if (huHitResults.empty()) {
				continue;
			}

			for (std::map<std::string, std::string> hpRow : hpHitResults) {
				ofs << hpdate << "," << row["name"] << "," << pitcher << "," << pitcherThrows << "," << hpRow["inningtype"] << "," << hpRow["inningnum"] << "," << hpRow["batpos"] << std::endl;
			}

			for (std::map<std::string, std::string> huRow : huHitResults) {
				std::string throwQuery = "select distinct throws from players where id="+huRow["pitcherid"]+";";
				std::vector<std::map<std::string, std::string>> throwResult = DBWrapper::queryDatabase(db, throwQuery);
				ofs << hudate << "," << row["name"] << "," << umpire << "," << throwResult[0]["throws"] << "," << huRow["inningtype"] << "," << huRow["inningnum"] << "," << huRow["batpos"] << std::endl;
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
