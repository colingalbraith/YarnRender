#pragma once
#include <vector>
#include "cyVector.h"

static const float PI = 3.14159265f;

cy::Vec3f yarnCurve(float t, float a, float h, float d);
cy::Vec3f yarnDeriv(float t, float a, float h, float d);
void frenetFrame(float t, float a, float h, float d,
                 cy::Vec3f& e1, cy::Vec3f& e2, cy::Vec3f& e3);
cy::Vec3f fiberCurve(float t, float a, float h, float d,
                     float r, float omega, float phi);

void generateTube(
	const std::vector<cy::Vec3f>& pts,
	const std::vector<cy::Vec3f>& tans,
	float radius, int sides,
	std::vector<cy::Vec3f>& oP,
	std::vector<cy::Vec3f>& oN,
	std::vector<cy::Vec3f>& oT,
	float radiusVariation = 0.f,
	unsigned int seed = 0);
