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
            query = "select distinct pd.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and ph.gamenumber="+std::to_string(gameNumber);
            if (!umpire.empty()) {
            	query += " and ph.umpire='"+umpire+"'";
            }
            query += ";";
            std::vector<std::map<std::string, std::string>> hpsides = DBWrapper::queryDatabase(db, query);

            if (!hpsides.empty()) {
            	for (std::map<std::string, std::string> hpside : hpsides) {
            		query = "select distinct ph.gamedate,pd.hits from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where ph.gamedate < '"+datestr+"' and pd.hits='"+hpside["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event >= 0 and ph.gamenumber="+std::to_string(gameNumber);
            		if (!umpire.empty()) {
            			query += " and ph.umpire='"+umpire+"'";
            		}
            		query += ";";
            		std::vector<std::map<std::string, std::string>> hpsidedates = DBWrapper::queryDatabase(db, query);

            		for (std::map<std::string, std::string> hpsidedate : hpsidedates) {
            			query = "select count(*) from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid where pd.hits='"+hpsidedate["hits"]+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.batpos="+std::to_string(i+1)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event > 0 and ph.gamenumber="+std::to_string(gameNumber)+" and ph.gamedate='"+hpsidedate["gamedate"]+"'";
            			if (!umpire.empty()) {
            				query += " and ph.umpire='"+umpire+"'";
            			}
            			query += ";";
            			std::vector<std::map<std::string, std::string>> hitQuery = DBWrapper::queryDatabase(db, query);

           				int hitcount = std::stoi(hitQuery[0]["count(*)"]);
           				if (hitcount == 0) {
           					lineup.emplace_back(i+1,hpsidedate["gamedate"],hpsidedate["hits"][0],'N');
           				}
           				else {
           					lineup.emplace_back(i+1,hpsidedate["gamedate"],hpsidedate["hits"][0],'Y');
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

            			    if (pitcherThrows[0] != std::get<2>(tp) && std::get<2>(tp) != 'S' && std::find(lineupBatPos.begin(), lineupBatPos.end(), 'S') == lineupBatPos.end() && batposStats.find(std::make_pair(std::get<0>(tp), 'S')) == batposStats.end()) {
            			    	lineupBatPos.push_back('S');
            			    }
            			    else if (std::get<2>(tp) == 'S') {
            			    	if (pitcherThrows[0] == 'R' && std::find(lineupBatPos.begin(), lineupBatPos.end(), 'L') == lineupBatPos.end() && batposStats.find(std::make_pair(std::get<0>(tp), 'L')) == batposStats.end()) {
            			    		lineupBatPos.push_back('L');
            			    	}
            			    	else if (pitcherThrows[0] == 'L' && std::find(lineupBatPos.begin(), lineupBatPos.end(), 'R') == lineupBatPos.end() && batposStats.find(std::make_pair(std::get<0>(tp), 'R')) == batposStats.end()) {
            			    		lineupBatPos.push_back('R');
            			    	}
            			    }
            		    }
            		}
            	}
            	else if (std::get<3>(tp) == 'N') {
            		lineupBatPos.erase(std::remove_if(lineupBatPos.begin(), lineupBatPos.end(), [&](char c) { return c == std::get<2>(tp);}), lineupBatPos.end());
            	}

            	++batposStats[std::make_pair(std::get<0>(tp), std::get<2>(tp))];

            	if (!umpire.empty()) {
            	    std::cout << std::get<0>(tp) << "," << std::get<1>(tp) << "," << std::get<2>(tp) << "," << std::get<3>(tp) << std::endl;
            	}
            }

            optimalLineup[prevBatPos-1] = lineupBatPos;

            std::cout << "\n";
            int printBatPos = 1;
            for (std::vector<char> sides : optimalLineup) {
            	if (!sides.empty()) {
					std::cout << printBatPos << ": ";
					std::for_each(  sides.begin(),
									sides.end(),
									[](const char elem ) {
											std::cout<<elem;
									});
					std::cout << "\n";
					std::string hitterQuery = "select distinct hitter.name from PBPDetails pd inner join PBPHeader ph on ph.id=pd.headerid inner join players hitter on hitter.id=pd.hitterid where ph.gamedate < '"+datestr+"' and pd.pitcherid="+std::to_string(pitcherId)+" and pd.hitterid in (select id from players where team like '%"+opponent+"' and position != 'P') and pd.batpos="+std::to_string(printBatPos)+" and ph.isNightGame="+(dayNight[0] == 'n' ? "1" : "0")+" and pd.inningtype='"+(tmType == teamType::AWAY ? "t" : "b")+"' and pd.event > 0 and ph.gamenumber="+std::to_string(gameNumber)+" and pd.hits in (";
					for (char s : sides) {
						hitterQuery += "'";
						hitterQuery += s;
						hitterQuery += "'";
						if (s != sides[sides.size()-1]) {
							hitterQuery += ",";
						}
					}
					hitterQuery += ");";

					std::vector<std::map<std::string, std::string>> hitterList = DBWrapper::queryDatabase(db, hitterQuery);

					for (std::map<std::string, std::string> hitter : hitterList) {
						std::cout << hitter["name"] << std::endl;
					}
            	}
                ++printBatPos;
            }

    	}

		++side;
	}

	sqlite3_close_v2(db);

	return (0);
}
