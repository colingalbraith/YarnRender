#include "YarnGeometry.h"
#include "YarnMath.h"
#include <cmath>
#include <algorithm>

static float fibHash(int row, int layer, int fib, int channel)
{
	unsigned h = (unsigned)(row*7919 + layer*6271 + fib*3571 + channel*1301);
	h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
	return (float)(h & 0xFFFF) / 65535.f;
}

void buildYarnTubes(const YarnParams& p,
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan,
	std::vector<cy::Vec3f>& col,
	std::vector<float>& ftype,
	std::vector<float>& tubeU,
	std::vector<float>& tubeV)
{
	const int nRows = 6, nLoops = 6, spl = 48;
	float w = p.yarnH + .5f;
	float tubeR = 0.45f;
	int   sides = 8;
	float dt = 2.f * PI / spl;

	for (int row = 0; row < nRows; row++) {
		float y0 = w * row;
		int total = nLoops * spl;
		std::vector<cy::Vec3f> curve(total), tans(total);
		for (int i = 0; i < total; i++) {
			float t = dt * i;
			cy::Vec3f pt = yarnCurve(t, p.yarnA, p.yarnH, p.yarnD);
			pt.y += y0;
			curve[i] = pt;
			tans[i]  = yarnDeriv(t, p.yarnA, p.yarnH, p.yarnD).GetNormalized();
		}
		generateTube(curve, tans, tubeR, sides, pos, nrm, tan, tubeU, tubeV);
		cy::Vec3f c(0.9f + 0.1f * fibHash(row,0,0,0),
		            0.9f + 0.1f * fibHash(row,0,0,1),
		            0.9f + 0.1f * fibHash(row,0,0,2));
		col.resize(pos.size(), c);
		ftype.resize(pos.size(), 0.f); // 0 = ply
	}
}

void buildFiberTubes(const YarnParams& p,
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan,
	std::vector<cy::Vec3f>& col,
	std::vector<float>& ftype,
	std::vector<float>& tubeU,
	std::vector<float>& tubeV)
{
	const int nRows = 6, nLoops = 6, spl = 48;
	int nOuter = p.fiberCount;
	float w = p.yarnH + .5f;
	float dt = 2.f * PI / spl;
	int total = nLoops * spl;

	struct PlyLayer { float radius, tubeR; int count; float omegaMul, phiOffset; int sides; };

	float outerR     = p.yarnRadius;
	float outerTubeR = nOuter > 0 ? std::max(0.04f, 0.75f * PI * outerR / nOuter) : 0.f;
	int   nFlyaway   = p.flyawayCount;

	PlyLayer layers[] = {
		{ outerR, outerTubeR, nOuter,  1.0f,  0.f,           6 },
	};
	int nLayers = (nOuter > 0) ? 1 : 0;

	for (int row = 0; row < nRows; row++) {
		float y0 = w * row;

		for (int li = 0; li < nLayers; li++) {
			PlyLayer& L = layers[li];
			float layerOmega = p.yarnOmega * L.omegaMul;

			for (int fib = 0; fib < L.count; fib++) {
				float phi = (2.f * PI * fib / L.count) + L.phiOffset;
				float rPerturb = (li > 0) ? 0.025f * sinf(5.3f*fib + 1.7f*row) : 0.f;
				float pPerturb = (li > 0) ? 0.04f  * sinf(3.1f*fib + 2.3f*row) : 0.f;
				float effR   = L.radius + rPerturb;
				float effPhi = phi + pPerturb;

				std::vector<cy::Vec3f> curve(total), tans(total);
				for (int i = 0; i < total; i++) {
					float t = dt * i;
					cy::Vec3f pt;
					if (L.radius < 0.001f) pt = yarnCurve(t, p.yarnA, p.yarnH, p.yarnD);
					else                    pt = fiberCurve(t, p.yarnA, p.yarnH, p.yarnD, effR, layerOmega, effPhi);
					pt.y += y0;
					curve[i] = pt;
					float eps = dt * 0.01f;
					cy::Vec3f pp, pm;
					if (L.radius < 0.001f) { pp = yarnCurve(t+eps, p.yarnA, p.yarnH, p.yarnD); pm = yarnCurve(t-eps, p.yarnA, p.yarnH, p.yarnD); }
					else { pp = fiberCurve(t+eps, p.yarnA, p.yarnH, p.yarnD, effR, layerOmega, effPhi); pm = fiberCurve(t-eps, p.yarnA, p.yarnH, p.yarnD, effR, layerOmega, effPhi); }
					tans[i] = (pp - pm).GetNormalized();
				}
				unsigned seed = (unsigned)(row*997 + li*131 + fib*37);
				generateTube(curve, tans, L.tubeR, L.sides, pos, nrm, tan, tubeU, tubeV, 0.25f, seed);
				cy::Vec3f fc(0.8f + 0.4f * fibHash(row,li,fib,10),
				             0.8f + 0.4f * fibHash(row,li,fib,11),
				             0.8f + 0.4f * fibHash(row,li,fib,12));
				col.resize(pos.size(), fc);
				ftype.resize(pos.size(), 0.f); // 0 = ply
			}
		}

		// flyaway fibers
		for (int fl = 0; fl < nFlyaway; fl++) {
			float basePhi  = 2.f * PI * fl / nFlyaway + fibHash(row,99,fl,0) * PI;
			float flyOmega = p.yarnOmega * (0.85f + 0.3f * fibHash(row,99,fl,1));
			float freq1 = 2.5f + 3.f*fibHash(row,99,fl,2), freq2 = 5.f + 4.f*fibHash(row,99,fl,3);
			float amp1  = 0.12f + 0.18f*fibHash(row,99,fl,4), amp2 = 0.06f + 0.10f*fibHash(row,99,fl,5);
			float phase1 = fibHash(row,99,fl,6)*2.f*PI, phase2 = fibHash(row,99,fl,7)*2.f*PI;
			float flyTubeR = 0.025f + 0.015f * fibHash(row,99,fl,8);

			std::vector<cy::Vec3f> curve(total), tans(total);
			for (int i = 0; i < total; i++) {
				float t = dt * i;
				float flyR = outerR + amp1*sinf(freq1*t+phase1) + amp2*sinf(freq2*t+phase2);
				flyR = std::max(flyR, outerR * 0.7f);
				cy::Vec3f pt = fiberCurve(t, p.yarnA, p.yarnH, p.yarnD, flyR, flyOmega, basePhi);
				pt.y += y0; curve[i] = pt;
				float eps = dt*0.01f;
				float flyRp = outerR + amp1*sinf(freq1*(t+eps)+phase1) + amp2*sinf(freq2*(t+eps)+phase2);
				float flyRm = outerR + amp1*sinf(freq1*(t-eps)+phase1) + amp2*sinf(freq2*(t-eps)+phase2);
				flyRp = std::max(flyRp, outerR*0.7f); flyRm = std::max(flyRm, outerR*0.7f);
				cy::Vec3f pp = fiberCurve(t+eps, p.yarnA, p.yarnH, p.yarnD, flyRp, flyOmega, basePhi);
				cy::Vec3f pm = fiberCurve(t-eps, p.yarnA, p.yarnH, p.yarnD, flyRm, flyOmega, basePhi);
				tans[i] = (pp - pm).GetNormalized();
			}
			unsigned flySeed = (unsigned)(row*1999 + fl*71);
			generateTube(curve, tans, flyTubeR, 4, pos, nrm, tan, tubeU, tubeV, 0.35f, flySeed);
			cy::Vec3f fc(0.7f + 0.6f * fibHash(row,99,fl,10),
			             0.7f + 0.6f * fibHash(row,99,fl,11),
			             0.7f + 0.6f * fibHash(row,99,fl,12));
			col.resize(pos.size(), fc);
			ftype.resize(pos.size(), 1.f); // 1 = flyaway
		}
	}
}
