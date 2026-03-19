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
	int nOuter = p.plyCount;
	float w = p.yarnH + .5f;
	float dt = 2.f * PI / spl;
	int total = nLoops * spl;

	struct PlyLayer { float radius, tubeR; int count; float omegaMul, phiOffset; int sides; };

	float outerR     = p.yarnRadius;
	float outerTubeR = nOuter > 0 ? std::max(0.04f, 0.75f * PI * outerR / nOuter) : 0.f;
	int   nFibers    = p.fiberCount;

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

		// flyaways — short wispy strands that follow a ply/fiber then peel off
		int nFlyaways = p.flyawayCount;
		int nAttach = nOuter + nFibers;
		for (int fa = 0; fa < nFlyaways; fa++) {
			if (nAttach <= 0) break;

			// pick random ply or fiber strand to attach to
			int attachIdx = (int)(fibHash(row,200,fa,1) * nAttach) % nAttach;
			bool attachToPly = (attachIdx < nOuter);

			// separation point along the parent strand
			float t0 = fibHash(row,200,fa,0) * total * dt;
			float eps = dt * 0.01f;

			// parent strand parameters (needed to walk along it)
			int    parentPly = 0;
			float  parentPhi = 0.f, parentOmega = p.yarnOmega;
			float  parentFreq1=0, parentFreq2=0, parentAmp1=0, parentAmp2=0;
			float  parentPhase1=0, parentPhase2=0;
			float  parentTubeR = outerTubeR;
			cy::Vec3f parentColor;

			if (attachToPly) {
				parentPly = attachIdx;
				parentPhi = (2.f * PI * parentPly / nOuter);
				parentOmega = p.yarnOmega;
				parentTubeR = outerTubeR;
				// inherit ply color
				parentColor = cy::Vec3f(
					0.8f + 0.4f * fibHash(row,0,parentPly,10),
					0.8f + 0.4f * fibHash(row,0,parentPly,11),
					0.8f + 0.4f * fibHash(row,0,parentPly,12));
			} else {
				int fi = attachIdx - nOuter;
				parentPhi   = 2.f * PI * fi / nFibers + fibHash(row,99,fi,0) * PI;
				parentOmega = p.yarnOmega * (0.85f + 0.3f * fibHash(row,99,fi,1));
				parentFreq1 = 2.5f + 3.f*fibHash(row,99,fi,2);
				parentFreq2 = 5.f + 4.f*fibHash(row,99,fi,3);
				parentAmp1  = 0.12f + 0.18f*fibHash(row,99,fi,4);
				parentAmp2  = 0.06f + 0.10f*fibHash(row,99,fi,5);
				parentPhase1 = fibHash(row,99,fi,6)*2.f*PI;
				parentPhase2 = fibHash(row,99,fi,7)*2.f*PI;
				parentTubeR = 0.025f + 0.015f * fibHash(row,99,fi,8);
				// inherit fiber color
				parentColor = cy::Vec3f(
					0.7f + 0.6f * fibHash(row,99,fi,10),
					0.7f + 0.6f * fibHash(row,99,fi,11),
					0.7f + 0.6f * fibHash(row,99,fi,12));
			}

			// helper: evaluate parent strand centerline at parameter t
			auto parentPos = [&](float t) -> cy::Vec3f {
				float r = outerR;
				if (!attachToPly) {
					r = outerR + parentAmp1*sinf(parentFreq1*t+parentPhase1)
					           + parentAmp2*sinf(parentFreq2*t+parentPhase2);
					r = std::max(r, outerR * 0.7f);
				}
				cy::Vec3f pt = fiberCurve(t, p.yarnA, p.yarnH, p.yarnD, r, parentOmega, parentPhi);
				pt.y += y0;
				return pt;
			};

			// per-flyaway random properties
			float len = p.flyawayLength * (0.4f + 1.2f * fibHash(row,200,fa,2));
			float curlFreq = 4.f + 8.f * fibHash(row,200,fa,3);
			float curlAmp = p.flyawayCurl * (0.3f + fibHash(row,200,fa,4));
			float tanBias = (fibHash(row,200,fa,5) - 0.5f) * 0.6f;
			float sideBias = (fibHash(row,200,fa,6) - 0.5f) * 0.4f;
			float flyTubeR = (0.008f + 0.012f * fibHash(row,200,fa,7)) * p.flyawayThickness;

			// outward peel direction: from yarn center through parent at t0
			cy::Vec3f parentAtT0 = parentPos(t0);
			cy::Vec3f yarnCtr = yarnCurve(t0, p.yarnA, p.yarnH, p.yarnD);
			yarnCtr.y += y0;
			cy::Vec3f radial = (parentAtT0 - yarnCtr).GetNormalized();
			cy::Vec3f tanDir = (parentPos(t0+eps) - parentPos(t0-eps)).GetNormalized();
			cy::Vec3f sideDir = tanDir.Cross(radial).GetNormalized();

			// how far (in t) the flyaway follows the parent before peeling
			float followLen = dt * 4.f; // ~4 samples of following
			float peelLen   = len * 1.5f; // t-range for the peel-off portion

			int nFollow = 5;  // samples that follow parent
			int nPeel   = 12; // samples that diverge outward
			int nSamples = nFollow + nPeel;
			std::vector<cy::Vec3f> curve(nSamples), tans2(nSamples);

			// phase 1: follow the parent strand centerline
			for (int si = 0; si < nFollow; si++) {
				float frac = (float)si / (float)(nFollow - 1);
				float t = t0 - followLen * (1.f - frac); // walk up to t0
				curve[si] = parentPos(t);
			}

			// phase 2: peel away from the parent
			cy::Vec3f peelBase = parentPos(t0); // separation point
			for (int si = 0; si < nPeel; si++) {
				float s = (float)(si + 1) / (float)nPeel; // 0..1 after separation
				float tAlong = t0 + s * peelLen * 0.3f;   // still drifts forward
				cy::Vec3f onParent = parentPos(tAlong);

				// outward displacement grows with s
				float outward = s * s * len * p.flyawaySpread;
				float side = sideBias * s * len + curlAmp * sinf(s * curlFreq) * len * 0.15f;
				float droop = s * s * len * 0.08f;

				// recompute radial at this t for accuracy
				cy::Vec3f yc = yarnCurve(tAlong, p.yarnA, p.yarnH, p.yarnD);
				yc.y += y0;
				cy::Vec3f localRadial = (onParent - yc).GetNormalized();
				cy::Vec3f localTan = (parentPos(tAlong+eps) - parentPos(tAlong-eps)).GetNormalized();
				cy::Vec3f localSide = localTan.Cross(localRadial).GetNormalized();

				curve[nFollow + si] = onParent
					+ localRadial * outward
					+ localSide * side
					+ localTan * tanBias * s * len * 0.3f
					+ cy::Vec3f(0, -droop, 0);
			}

			// finite-difference tangents
			for (int si = 0; si < nSamples; si++) {
				if (si == 0) tans2[si] = (curve[1] - curve[0]).GetNormalized();
				else if (si == nSamples-1) tans2[si] = (curve[si] - curve[si-1]).GetNormalized();
				else tans2[si] = (curve[si+1] - curve[si-1]).GetNormalized();
			}

			unsigned faSeed = (unsigned)(row*3001 + fa*113);
			generateTube(curve, tans2, flyTubeR, 3, pos, nrm, tan, tubeU, tubeV, 0.4f, faSeed);
			// inherit parent color with slight variation
			cy::Vec3f fc = parentColor;
			fc.x *= 0.9f + 0.2f * fibHash(row,200,fa,10);
			fc.y *= 0.9f + 0.2f * fibHash(row,200,fa,11);
			fc.z *= 0.9f + 0.2f * fibHash(row,200,fa,12);
			col.resize(pos.size(), fc);
			ftype.resize(pos.size(), 2.f); // 2 = flyaway
		}

		// fiber strands
		for (int fl = 0; fl < nFibers; fl++) {
			float basePhi  = 2.f * PI * fl / nFibers + fibHash(row,99,fl,0) * PI;
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
			ftype.resize(pos.size(), 1.f); // 1 = fiber
		}
	}
}
