#pragma once
#include <vector>
#include "cyVector.h"

struct YarnParams {
	int   plyCount;
	int   fiberCount;
	int   flyawayCount;
	float flyawayLength, flyawaySpread, flyawayCurl, flyawayThickness;
	float yarnA, yarnH, yarnD, yarnOmega, yarnRadius;
};

void buildYarnTubes(const YarnParams& p,
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan,
	std::vector<cy::Vec3f>& col,
	std::vector<float>& ftype,
	std::vector<float>& tubeU,
	std::vector<float>& tubeV);

void buildFiberTubes(const YarnParams& p,
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan,
	std::vector<cy::Vec3f>& col,
	std::vector<float>& ftype,
	std::vector<float>& tubeU,
	std::vector<float>& tubeV);
