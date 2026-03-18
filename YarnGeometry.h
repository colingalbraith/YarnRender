#pragma once
#include <vector>
#include "cyVector.h"

struct YarnParams {
	int   fiberCount;
	float yarnA, yarnH, yarnD, yarnOmega, yarnRadius;
};

void buildYarnTubes(const YarnParams& p,
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan);

void buildFiberTubes(const YarnParams& p,
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan);
