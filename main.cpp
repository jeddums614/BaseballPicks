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

	while (std::getline(ifs, line)) {
		std::cout << line << std::endl;
		teamType tmType = (side % 2 == 0 ? teamType::AWAY : teamType::HOME);
		std::cout << tmType << std::endl;
		std::vector<std::string> gameParts = Utils::split(line);

		if (gameParts.size() < 4) {
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
		std::string dayNight = gameParts[3];

	    int gameNumber = 1;
	    if (gameParts.size() == 5) {
	    	gameNumber = 2;
	    }

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

    	std::vector<std::tuple<int, std::string, char, char>> lineup;

    	for (int i = 0; i < 9; ++i) {
            query = "select distinct pd.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and "+(tmType == teamType::AWAY ? "ph.hometeam='"+pitcherTeam+"' and ph.awayteam='"+opponent+"'" : "ph.awayteam='"+pitcherTeam+"' and ph.hometeam='"+opponent+"'")+" order by pd.hits;";
            std::vector<std::map<std::string, std::string>> hpsides = DBWrapper::queryDatabase(db, query);

            if (!hpsides.empty()) {
            	for (std::map<std::string, std::string> hpside : hpsides) {
            		query = "select distinct ph.gamedate,pd.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and pd.hits='"+hpside["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and "+(tmType == teamType::AWAY ? "ph.hometeam='"+pitcherTeam+"' and ph.awayteam='"+opponent+"'" : "ph.awayteam='"+pitcherTeam+"' and ph.hometeam='"+opponent+"'")+" and pd.isHitterStarter=1 and pd.isPitcherStarter=1;";
            		std::vector<std::map<std::string, std::string>> hpsidedates = DBWrapper::queryDatabase(db, query);

            		for (std::map<std::string, std::string> hpsidedate : hpsidedates) {
            			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where pd.hits='"+hpsidedate["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and pd.event > 0 and ph.gamedate='"+hpsidedate["gamedate"]+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.inningnum <= 7;";
            			std::vector<std::map<std::string, std::string>> hitQuery = DBWrapper::queryDatabase(db, query);

           				int hitcount = std::stoi(hitQuery[0]["count(*)"]);
           				if (hitcount == 0) {
                			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where pd.hits='"+hpsidedate["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and pd.event = 0 and ph.gamedate='"+hpsidedate["gamedate"]+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.inningnum <= 7;";
                			std::vector<std::map<std::string, std::string>> abQuery = DBWrapper::queryDatabase(db, query);
                			int abcount = std::stoi(abQuery[0]["count(*)"]);
                			if (abcount > 0) {
           					    lineup.emplace_back(i+1,hpsidedate["gamedate"],hpsidedate["hits"][0],'N');
                			}
                			else {
                				lineup.emplace_back(i+1,hpsidedate["gamedate"],hpsidedate["hits"][0],'?');
                			}
           				}
           				else {
           					lineup.emplace_back(i+1,hpsidedate["gamedate"],hpsidedate["hits"][0],'Y');
           				}
           			}
           		}
           	}

            query = "select distinct pd.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.umpire='"+umpire+"' order by pd.hits;";
            std::vector<std::map<std::string, std::string>> pusides = DBWrapper::queryDatabase(db, query);

            if (!pusides.empty()) {
            	for (std::map<std::string, std::string> puside : pusides) {
            		query = "select distinct ph.gamedate,pd.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and pd.hits='"+puside["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.umpire='"+umpire+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1;";
            		std::vector<std::map<std::string, std::string>> pusidedates = DBWrapper::queryDatabase(db, query);

            		for (std::map<std::string, std::string> pusidedate : pusidedates) {
            			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where pd.hits='"+pusidedate["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and pd.event > 0 and ph.gamedate='"+pusidedate["gamedate"]+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.inningnum <= 7;";
            			std::vector<std::map<std::string, std::string>> hitQuery = DBWrapper::queryDatabase(db, query);

           				int hitcount = std::stoi(hitQuery[0]["count(*)"]);
           				if (hitcount == 0) {
                			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where pd.hits='"+pusidedate["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and pd.event = 0 and ph.gamedate='"+pusidedate["gamedate"]+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and pd.inningnum <= 7;";
                			std::vector<std::map<std::string, std::string>> abQuery = DBWrapper::queryDatabase(db, query);
                			int abcount = std::stoi(abQuery[0]["count(*)"]);
                			if (abcount > 0) {
           					    lineup.emplace_back(i+1,pusidedate["gamedate"],pusidedate["hits"][0],'N');
                			}
                			else {
                				lineup.emplace_back(i+1,pusidedate["gamedate"],pusidedate["hits"][0],'?');
                			}
           				}
           				else {
           					lineup.emplace_back(i+1,pusidedate["gamedate"],pusidedate["hits"][0],'Y');
           				}
           			}
           		}
           	}
    	}

    	if (!lineup.empty()) {
            std::map<std::pair<int, char>, int> batposStats;
            std::vector<std::vector<char>> optimalLineup;
            optimalLineup.resize(9);
            int prevBatPos = 1;
            std::vector<char> lineupBatPos;

            for (std::tuple<int,std::string,char,char> tp : lineup) {
            	if (std::get<0>(tp) != prevBatPos) {
            		optimalLineup[prevBatPos-1] = lineupBatPos;
            		prevBatPos = std::get<0>(tp);
            		lineupBatPos = std::vector<char>();
            		batposStats = std::map<std::pair<int, char>, int>();
            	}

            	if (std::get<3>(tp) == 'Y') {
            		if (batposStats.find(std::make_pair(std::get<0>(tp), std::get<2>(tp))) == batposStats.end()) {
            		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == std::get<2>(tp);}) == lineupBatPos.end()) {
            			    lineupBatPos.push_back(std::get<2>(tp));
            		    }
            		}

            		if (std::get<2>(tp) != 'S') {
            			if (pitcherThrows[0] != std::get<2>(tp)) {
                    		if (batposStats.find(std::make_pair(std::get<0>(tp), 'S')) == batposStats.end()) {
                    		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == 'S';}) == lineupBatPos.end()) {
                    			    lineupBatPos.push_back('S');
                    		    }
                    		}
            			}
            		}
            		else {
            			if (pitcherThrows[0] == 'R') {
                    		if (batposStats.find(std::make_pair(std::get<0>(tp), 'L')) == batposStats.end()) {
                    		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == 'L';}) == lineupBatPos.end()) {
                    			    lineupBatPos.push_back('L');
                    		    }
                    		}
            			}
            			else {
                    		if (batposStats.find(std::make_pair(std::get<0>(tp), 'L')) == batposStats.end()) {
                    		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == 'L';}) == lineupBatPos.end()) {
                    			    lineupBatPos.push_back('L');
                    		    }
                    		}
            			}
            		}
            	}
            	else if (std::get<3>(tp) == 'N') {
            		lineupBatPos.erase(std::remove_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == std::get<2>(tp);}), lineupBatPos.end());

            		if (std::get<2>(tp) != 'S') {
            			if (std::get<2>(tp) == 'R') {
                    		if (batposStats.find(std::make_pair(std::get<0>(tp), 'L')) == batposStats.end()) {
                    		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == 'L';}) == lineupBatPos.end()) {
                    			    lineupBatPos.push_back('L');
                    		    }
                    		}
            			}
            			else {
                    		if (batposStats.find(std::make_pair(std::get<0>(tp), 'R')) == batposStats.end()) {
                    		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == 'R';}) == lineupBatPos.end()) {
                    			    lineupBatPos.push_back('R');
                    		    }
                    		}
            			}

            			if (pitcherThrows[0] == std::get<2>(tp)) {
                    		if (batposStats.find(std::make_pair(std::get<0>(tp), 'S')) == batposStats.end()) {
                    		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == 'S';}) == lineupBatPos.end()) {
                    			    lineupBatPos.push_back('S');
                    		    }
                    		}
            			}
            		}
            		else {
                		if (batposStats.find(std::make_pair(std::get<0>(tp), pitcherThrows[0])) == batposStats.end()) {
                		    if (std::find_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == pitcherThrows[0];}) == lineupBatPos.end()) {
                			    lineupBatPos.push_back(pitcherThrows[0]);
                		    }
                		}
            		}
            	}

            	++batposStats[std::make_pair(std::get<0>(tp), std::get<2>(tp))];

            	std::cout << std::get<0>(tp) << "," << std::get<1>(tp) << "," << std::get<2>(tp) << "," << std::get<3>(tp) << std::endl;
            }

            optimalLineup[prevBatPos-1] = lineupBatPos;

            std::cout << "\n";
            int printBatPos = 1;
            for (std::vector<char> sides : optimalLineup) {
            	if (!sides.empty()) {
            		std::sort(sides.begin(), sides.end());
            		if (argc > 1) {
            			std::cout << "*";
            		}
					std::cout << printBatPos << ": ";
					std::for_each(  sides.begin(),
									sides.end(),
									[](const char elem ) {
											std::cout<<elem;
									});
					std::cout << "\n";
            	}
                ++printBatPos;
            }

            if (!optimalLineup.empty()) {
            	std::cout << "\n";
            }

    	}

		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
