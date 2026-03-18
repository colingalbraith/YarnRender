#include "YarnMath.h"
#include <cmath>
#include <algorithm>

cy::Vec3f yarnCurve(float t, float a, float h, float d)
{
	return cy::Vec3f(t + a * sinf(2.f * t),
	                 h * cosf(t),
	                 d * cosf(2.f * t));
}

cy::Vec3f yarnDeriv(float t, float a, float h, float d)
{
	return cy::Vec3f(1.f + 2.f * a * cosf(2.f * t),
	                 -h * sinf(t),
	                 -2.f * d * sinf(2.f * t));
}

void frenetFrame(float t, float a, float h, float d,
                 cy::Vec3f& e1, cy::Vec3f& e2, cy::Vec3f& e3)
{
	e1 = yarnDeriv(t, a, h, d);

	float u = e1.Dot(e1);
	float v = 2.f*h*h*cosf(t)*sinf(t)
	        + 16.f*d*d*cosf(2.f*t)*sinf(2.f*t)
	        - 8.f*a*(1.f + 2.f*a*cosf(2.f*t))*sinf(2.f*t);
	float x = 1.f / sqrtf(u);
	float y = v / (2.f * powf(u, 1.5f));

	e2.x = y * (-1.f - 2.f*a*cosf(2.f*t)) - x * 4.f*a*sinf(2.f*t);
	e2.y = y * h*sinf(t)                    - x * h*cosf(t);
	e2.z = y * 2.f*d*sinf(2.f*t)            - x * 4.f*d*cosf(2.f*t);

	e1 = e1 * x;
	e2 = e2 * (1.f / e2.Length());
	e3 = e1.Cross(e2);
}

cy::Vec3f fiberCurve(float t, float a, float h, float d,
                     float r, float omega, float phi)
{
	cy::Vec3f g = yarnCurve(t, a, h, d);
	cy::Vec3f e1, e2, e3;
	frenetFrame(t, a, h, d, e1, e2, e3);
	float th = t * omega - 2.f * cosf(t) + phi;
	return g + (e2 * cosf(th) + e3 * sinf(th)) * r;
}

static float hashFloat(unsigned int x) {
	x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
	return (float)(x & 0xFFFF) / 65535.f;
}

void generateTube(
	const std::vector<cy::Vec3f>& pts,
	const std::vector<cy::Vec3f>& tans,
	float radius, int sides,
	std::vector<cy::Vec3f>& oP,
	std::vector<cy::Vec3f>& oN,
	std::vector<cy::Vec3f>& oT,
	float radiusVariation,
	unsigned int seed)
{
	int n = (int)pts.size();
	if (n < 2) return;

	// Pre-compute per-ring radius variation
	std::vector<float> radii(n, radius);
	if (radiusVariation > 0.f) {
		for (int i = 0; i < n; i++) {
			float noise = sinf((float)i * 0.37f + hashFloat(seed + i*7) * 6.28f) * 0.5f
			            + sinf((float)i * 0.13f + hashFloat(seed + i*13 + 999) * 6.28f) * 0.3f
			            + sinf((float)i * 0.71f + hashFloat(seed + i*31 + 5555) * 6.28f) * 0.2f;
			radii[i] = radius * (1.f + radiusVariation * noise);
			radii[i] = std::max(radii[i], radius * 0.3f);
		}
	}

	std::vector<cy::Vec3f> N(n), B(n);

	cy::Vec3f up(0, 1, 0);
	if (fabsf(tans[0].Dot(up)) > 0.99f) up = cy::Vec3f(1, 0, 0);
	N[0] = (up - tans[0] * tans[0].Dot(up)).GetNormalized();
	B[0] = tans[0].Cross(N[0]).GetNormalized();

	for (int i = 1; i < n; i++) {
		cy::Vec3f ax = tans[i-1].Cross(tans[i]);
		float axL = ax.Length();
		if (axL > 1e-6f) {
			ax /= axL;
			float ang = acosf(std::min(std::max(tans[i-1].Dot(tans[i]), -1.f), 1.f));
			float c = cosf(ang), s = sinf(ang);
			cy::Vec3f Np = N[i-1];
			N[i] = (Np*c + ax.Cross(Np)*s + ax*(ax.Dot(Np))*(1-c)).GetNormalized();
		} else {
			N[i] = N[i-1];
		}
		B[i] = tans[i].Cross(N[i]).GetNormalized();
	}

	for (int i = 0; i < n - 1; i++) {
		for (int j = 0; j < sides; j++) {
			float a0 = 2.f * PI * j / sides;
			float a1 = 2.f * PI * ((j+1) % sides) / sides;

			auto ring = [&](int ri, float ang, cy::Vec3f& p, cy::Vec3f& nn, cy::Vec3f& tt){
				nn = N[ri]*cosf(ang) + B[ri]*sinf(ang);
				p  = pts[ri] + nn * radii[ri];
				tt = tans[ri];
			};

			cy::Vec3f p00,n00,t00; ring(i,   a0, p00,n00,t00);
			cy::Vec3f p01,n01,t01; ring(i,   a1, p01,n01,t01);
			cy::Vec3f p10,n10,t10; ring(i+1, a0, p10,n10,t10);
			cy::Vec3f p11,n11,t11; ring(i+1, a1, p11,n11,t11);

			oP.push_back(p00); oN.push_back(n00); oT.push_back(t00);
			oP.push_back(p10); oN.push_back(n10); oT.push_back(t10);
			oP.push_back(p01); oN.push_back(n01); oT.push_back(t01);

			oP.push_back(p01); oN.push_back(n01); oT.push_back(t01);
			oP.push_back(p10); oN.push_back(n10); oT.push_back(t10);
			oP.push_back(p11); oN.push_back(n11); oT.push_back(t11);
		}
	}
}
