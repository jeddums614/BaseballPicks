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
	std::string hits;
	char gotHit;
	int batpos;
	std::string name;
};

class Lineup {
public:
	Lineup(const std::string & lbl, const std::string & innType) : label(lbl), inningType(innType), oneLineOutput(false) {
		hitters.resize(9);
	}

	friend std::ostream& operator<<(std::ostream& os, const Lineup & lineup) {
		os << lineup.label << " (" << lineup.inningType << ") ";
		if (!lineup.oneLineOutput) {
			os << "\n";
		}
		for (Hitter h : lineup.hitters) {
			if (lineup.oneLineOutput && h.batpos > 1) {
				os << ",";
			}
			os << h.batpos << " (" << h.hits << ") - " << h.gotHit;
			if (!lineup.oneLineOutput) {
				os << " - " << h.name;
				os << "\n";
			}
		}

		return os;
	}

	void addHitter(Hitter& h) {
		hitters[h.batpos-1] = h;
	}

	Hitter getHitter(int batpos) {
		return hitters[batpos-1];
	}

	const std::string & getInningType() noexcept { return inningType;}
	const std::string & getLabel() noexcept { return label; }

	void setOutputType(bool isOneLine) { oneLineOutput = isOneLine; }
private:
	std::vector<Hitter> hitters;
	std::string label;
	std::string inningType;
	bool oneLineOutput;
};



#endif /* STATS_H_ */
