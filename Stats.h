/*
 * Stats.h
 *
 *  Created on: Jul 3, 2021
 *      Author: jeremy
 */

#ifndef STATS_H_
#define STATS_H_

#include <optional>

struct Stats {
	double hits;
	double atbats;
	double strikeouts;
	double sacflies;
	double homeruns;
};

struct Hitter {
	int playerId;
	std::string hits;
	char gotHit;
	int batpos;
};

class Lineup {
public:
	Lineup(const std::string & lbl, const std::string & innType) : label(lbl), inningType(innType) {
		hitters.resize(9);
	}

	friend std::ostream& operator<<(std::ostream& os, const Lineup & lineup) {
		os << lineup.label << " (" << lineup.inningType << ") ";
		for (Hitter h : lineup.hitters) {
			if (h.batpos > 1) {
				os << ",";
			}
			os << h.batpos << " (" << h.hits << ") - " << h.gotHit;
		}

		return os;
	}

	void addHitter(Hitter& h) {
		hitters[h.batpos-1] = h;
	}

	Hitter getHitter(int batpos) {
		return hitters[batpos-1];
	}

	const std::string & getInningType() { return inningType;}
private:
	std::vector<Hitter> hitters;
	std::string label;
	std::string inningType;
};



#endif /* STATS_H_ */
