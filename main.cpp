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
		std::size_t pos = umpire.find("'");
		if (pos != std::string::npos) {
			umpire.insert(pos, "'");
		}
		std::string dayNight = gameParts[3];

		int gameNumber = 1;
		if (gameParts.size() == 5) {
			gameNumber = 2;
		}

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

    	std::vector<std::tuple<int, std::string, char, char>> lineup;

    	for (int i = 0; i < 9; ++i) {
            query = "select distinct hitter.name from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where hitter.team like '%"+opponent+"' and hitter.position != 'P' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.gamenumber="+std::to_string(gameNumber)+";";
            std::vector<std::map<std::string, std::string>> hphitters = DBWrapper::queryDatabase(db, query);

            if (!hphitters.empty()) {
            	for (std::map<std::string, std::string> hphitter : hphitters) {
            		std::string hittername = hphitter["name"];
            		std::size_t appos = hittername.find("'");
            		if (appos != std::string::npos) {
            			hittername.insert(appos, "'");
            		}
            		query = "select distinct ph.gamedate,hitter.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where hitter.name='"+hittername+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event >= 0 and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.gamenumber="+std::to_string(gameNumber)+" order by ph.gamedate desc limit 1;";
            		std::vector<std::map<std::string, std::string>> hitterhpdates = DBWrapper::queryDatabase(db, query);

            		if (!hitterhpdates.empty()) {
            			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where hitter.name='"+hittername+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event > 0 and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.gamenumber="+std::to_string(gameNumber)+" and ph.gamedate='"+hitterhpdates[0]["gamedate"]+"';";
            			std::vector<std::map<std::string, std::string>> hitQuery = DBWrapper::queryDatabase(db, query);

           				int hitcount = std::stoi(hitQuery[0]["count(*)"]);
           				if (hitcount == 0) {
           					lineup.emplace_back(i+1,hitterhpdates[0]["gamedate"],hitterhpdates[0]["hits"][0],'N');
           				}
           				else {
           					lineup.emplace_back(i+1,hitterhpdates[0]["gamedate"],hitterhpdates[0]["hits"][0],'Y');
           				}
           			}
           		}
           	}

            query = "select distinct hitter.name from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where hitter.team like '%"+opponent+"' and hitter.position != 'P' and ph.umpire='"+umpire+"' and pd.throws='"+pitcherThrows+"' and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.gamenumber="+std::to_string(gameNumber)+";";
            std::vector<std::map<std::string, std::string>> huhitters = DBWrapper::queryDatabase(db, query);

            if (!huhitters.empty()) {
            	for (std::map<std::string, std::string> huhitter : huhitters) {
            		std::string hittername = huhitter["name"];
            		std::size_t appos = hittername.find("'");
            		if (appos != std::string::npos) {
            			hittername.insert(appos, "'");
            		}
            		query = "select distinct ph.gamedate,hitter.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where hitter.name='"+hittername+"' and ph.umpire='"+umpire+"' and pd.throws='"+pitcherThrows+"' and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event >= 0 and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.gamenumber="+std::to_string(gameNumber)+" order by ph.gamedate desc limit 1;";
            		std::vector<std::map<std::string, std::string>> hitterhudates = DBWrapper::queryDatabase(db, query);

            		if (!hitterhudates.empty()) {
            			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where hitter.name='"+hittername+"' and ph.umpire='"+umpire+"' and pd.throws='"+pitcherThrows+"' and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event > 0 and pd.isHitterStarter=1 and pd.isPitcherStarter=1 and ph.gamenumber="+std::to_string(gameNumber)+" and ph.gamedate='"+hitterhudates[0]["gamedate"]+"';";
            			std::vector<std::map<std::string, std::string>> hitQuery = DBWrapper::queryDatabase(db, query);

           				int hitcount = std::stoi(hitQuery[0]["count(*)"]);
           				if (hitcount == 0) {
           					lineup.emplace_back(i+1,hitterhudates[0]["gamedate"],hitterhudates[0]["hits"][0],'N');
           				}
           				else {
           					lineup.emplace_back(i+1,hitterhudates[0]["gamedate"],hitterhudates[0]["hits"][0],'Y');
           				}
           			}
           		}
           	}
    	}

    	if (!lineup.empty()) {
            std::sort(lineup.begin(),lineup.end(),
    		       [](const std::tuple<int,std::string,char,char>& a,
    		       const std::tuple<int,std::string,char,char>& b) -> bool
    		       {
            	     bool bpcompare = (std::get<0>(a) < std::get<0>(b));
            	     int yr1 = std::stoi(std::get<1>(a).substr(0,4));
            	     int yr2 = std::stoi(std::get<1>(b).substr(0,4));
            	     int mo1 = std::stoi(std::get<1>(a).substr(5,2));
            	     int mo2 = std::stoi(std::get<1>(b).substr(5,2));
            	     int d1 = std::stoi(std::get<1>(a).substr(8));
            	     int d2 = std::stoi(std::get<1>(b).substr(8));
            	     if (bpcompare) {
            	    	 return true;
            	     }
            	     else if (std::get<0>(a) == std::get<0>(b)) {
                	     if (yr1 < yr2) {
                	    	 return true;
                	     }
                	     else if (yr1 == yr2 && mo1 < mo2) {
                	    	 return true;
                	     }
                	     else if (yr1 == yr2 && mo1 == mo2 && d1 < d2) {
                	    	 return true;
                	     }
                	     else {
                	    	 return false;
                	     }
            	     }
            	     else {
            	    	 return false;
            	     }
    		       });

            for (std::tuple<int,std::string,char,char> tp : lineup) {
            	std::cout << std::get<0>(tp) << "," << std::get<1>(tp) << "," << std::get<2>(tp) << "," << std::get<3>(tp) << std::endl;
            }

    	}

		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
