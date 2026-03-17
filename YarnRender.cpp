// Colin Galbraith U1592430
// YarnRender — parametric yarn visualization with shadow mapping
// Shading models: 1=Blinn-Phong  2=Kajiya-Kay  3=Marschner
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cyCore.h"
#include "cyVector.h"
#include "cyMatrix.h"
#include "cyGL.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static const float PI = 3.14159265f;

// ════════════════════════════════════════════════════════════════════════
//  Tweakable yarn parameters (exposed via ImGui)
// ════════════════════════════════════════════════════════════════════════

static int   fiberCount   = 24;
static float yarnA        = 1.5f;   // loop roundness
static float yarnH        = 4.0f;   // loop height
static float yarnD        = 1.0f;   // loop depth
static float yarnOmega    = 9.0f;   // fiber twist
static float yarnRadius   = 0.50f;  // fiber orbit radius
static float lightIntensity = 1.8f;
static float yarnColor[3] = { 0.65f, 0.30f, 0.35f };
static bool  showWireframe = false;

// ── Blinn-Phong parameters ──
static float bp_ambient   = 0.25f;
static float bp_diffuse   = 0.70f;
static float bp_specular  = 0.70f;
static float bp_shininess = 64.0f;
static float bp_wrap      = 0.45f;

// ── Kajiya-Kay parameters ──
static float kk_ambient       = 0.22f;
static float kk_diffuse       = 0.60f;
static float kk_specPrimary   = 0.60f;
static float kk_specSecondary = 0.35f;
static float kk_shinyPrimary  = 80.0f;
static float kk_shinySecondary = 18.0f;

// ── Marschner parameters ──
static float m_ambient      = 0.18f;
static float m_alphaR       = -0.07f; // cuticle tilt
static float m_betaR        = 0.12f;  // roughness
static float m_R_strength   = 0.40f;
static float m_TT_strength  = 1.00f;
static float m_TRT_strength = 0.70f;

// ════════════════════════════════════════════════════════════════════════
//  Yarn curve math  (ported from plain-knit-yarn / plain-knit.c)
// ════════════════════════════════════════════════════════════════════════

static cy::Vec3f yarnCurve(float t, float a, float h, float d)
{
	return cy::Vec3f(t + a * sinf(2.f * t),
	                 h * cosf(t),
	                 d * cosf(2.f * t));
}

static cy::Vec3f yarnDeriv(float t, float a, float h, float d)
{
	return cy::Vec3f(1.f + 2.f * a * cosf(2.f * t),
	                 -h * sinf(t),
	                 -2.f * d * sinf(2.f * t));
}

static void frenetFrame(float t, float a, float h, float d,
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

static cy::Vec3f fiberCurve(float t, float a, float h, float d,
                            float r, float omega, float phi)
{
	cy::Vec3f g = yarnCurve(t, a, h, d);
	cy::Vec3f e1, e2, e3;
	frenetFrame(t, a, h, d, e1, e2, e3);
	float th = t * omega - 2.f * cosf(t) + phi;
	return g + (e2 * cosf(th) + e3 * sinf(th)) * r;
}

// ════════════════════════════════════════════════════════════════════════
//  Tube-mesh generation around a polyline
// ════════════════════════════════════════════════════════════════════════

static void generateTube(
	const std::vector<cy::Vec3f>& pts,
	const std::vector<cy::Vec3f>& tans,
	float radius, int sides,
	std::vector<cy::Vec3f>& oP,
	std::vector<cy::Vec3f>& oN,
	std::vector<cy::Vec3f>& oT)
{
	int n = (int)pts.size();
	if (n < 2) return;

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
				p  = pts[ri] + nn * radius;
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

// ════════════════════════════════════════════════════════════════════════
//  Geometry builders
// ════════════════════════════════════════════════════════════════════════

static void buildYarnTubes(
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan)
{
	const int nRows = 6, nLoops = 6, spl = 48;
	float w = yarnH + .5f;
	float tubeR = 0.45f;
	int   sides = 8;
	float dt = 2.f * PI / spl;

	for (int row = 0; row < nRows; row++) {
		float y0 = w * row;
		int total = nLoops * spl;
		std::vector<cy::Vec3f> curve(total), tans(total);
		for (int i = 0; i < total; i++) {
			float t = dt * i;
			cy::Vec3f p = yarnCurve(t, yarnA, yarnH, yarnD);
			p.y += y0;
			curve[i] = p;
			tans[i]  = yarnDeriv(t, yarnA, yarnH, yarnD).GetNormalized();
		}
		generateTube(curve, tans, tubeR, sides, pos, nrm, tan);
	}
}

static float fibHash(int row, int layer, int fib, int channel)
{
	unsigned h = (unsigned)(row*7919 + layer*6271 + fib*3571 + channel*1301);
	h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
	return (float)(h & 0xFFFF) / 65535.f;
}

static void buildFiberTubes(
	std::vector<cy::Vec3f>& pos,
	std::vector<cy::Vec3f>& nrm,
	std::vector<cy::Vec3f>& tan)
{
	const int nRows = 6, nLoops = 6, spl = 64;
	int nOuter = fiberCount;
	float w = yarnH + .5f;
	float dt = 2.f * PI / spl;
	int total = nLoops * spl;

	struct PlyLayer { float radius, tubeR; int count; float omegaMul, phiOffset; int sides; };

	float coreR      = 0.20f;
	float innerR     = yarnRadius * 0.56f;
	int   nInner     = std::max(4, nOuter * 2 / 3);
	float outerR     = yarnRadius;
	float outerTubeR = std::max(0.04f, 0.75f * PI * outerR / nOuter);
	float innerTubeR = std::max(0.04f, 0.75f * PI * innerR / nInner);
	int   nFlyaway   = std::max(3, nOuter / 4);

	PlyLayer layers[] = {
		{ 0.0f,   coreR,      1,      0.f,   0.f,          10 },
		{ innerR, innerTubeR, nInner,  1.15f, PI / nInner,   6 },
		{ outerR, outerTubeR, nOuter,  1.0f,  0.f,           6 },
	};
	int nLayers = 3;

	for (int row = 0; row < nRows; row++) {
		float y0 = w * row;

		for (int li = 0; li < nLayers; li++) {
			PlyLayer& L = layers[li];
			float layerOmega = yarnOmega * L.omegaMul;

			for (int fib = 0; fib < L.count; fib++) {
				float phi = (2.f * PI * fib / L.count) + L.phiOffset;
				float rPerturb = (li > 0) ? 0.025f * sinf(5.3f*fib + 1.7f*row) : 0.f;
				float pPerturb = (li > 0) ? 0.04f  * sinf(3.1f*fib + 2.3f*row) : 0.f;
				float effR   = L.radius + rPerturb;
				float effPhi = phi + pPerturb;

				std::vector<cy::Vec3f> curve(total), tans(total);
				for (int i = 0; i < total; i++) {
					float t = dt * i;
					cy::Vec3f p;
					if (L.radius < 0.001f) p = yarnCurve(t, yarnA, yarnH, yarnD);
					else                    p = fiberCurve(t, yarnA, yarnH, yarnD, effR, layerOmega, effPhi);
					p.y += y0;
					curve[i] = p;
					float eps = dt * 0.01f;
					cy::Vec3f pp, pm;
					if (L.radius < 0.001f) { pp = yarnCurve(t+eps, yarnA, yarnH, yarnD); pm = yarnCurve(t-eps, yarnA, yarnH, yarnD); }
					else { pp = fiberCurve(t+eps, yarnA, yarnH, yarnD, effR, layerOmega, effPhi); pm = fiberCurve(t-eps, yarnA, yarnH, yarnD, effR, layerOmega, effPhi); }
					tans[i] = (pp - pm).GetNormalized();
				}
				generateTube(curve, tans, L.tubeR, L.sides, pos, nrm, tan);
			}
		}

		// flyaway fibers
		for (int fl = 0; fl < nFlyaway; fl++) {
			float basePhi  = 2.f * PI * fl / nFlyaway + fibHash(row,99,fl,0) * PI;
			float flyOmega = yarnOmega * (0.85f + 0.3f * fibHash(row,99,fl,1));
			float freq1 = 2.5f + 3.f*fibHash(row,99,fl,2), freq2 = 5.f + 4.f*fibHash(row,99,fl,3);
			float amp1  = 0.12f + 0.18f*fibHash(row,99,fl,4), amp2 = 0.06f + 0.10f*fibHash(row,99,fl,5);
			float phase1 = fibHash(row,99,fl,6)*2.f*PI, phase2 = fibHash(row,99,fl,7)*2.f*PI;
			float flyTubeR = 0.025f + 0.015f * fibHash(row,99,fl,8);

			std::vector<cy::Vec3f> curve(total), tans(total);
			for (int i = 0; i < total; i++) {
				float t = dt * i;
				float flyR = outerR + amp1*sinf(freq1*t+phase1) + amp2*sinf(freq2*t+phase2);
				flyR = std::max(flyR, outerR * 0.7f);
				cy::Vec3f p = fiberCurve(t, yarnA, yarnH, yarnD, flyR, flyOmega, basePhi);
				p.y += y0; curve[i] = p;
				float eps = dt*0.01f;
				float flyRp = outerR + amp1*sinf(freq1*(t+eps)+phase1) + amp2*sinf(freq2*(t+eps)+phase2);
				float flyRm = outerR + amp1*sinf(freq1*(t-eps)+phase1) + amp2*sinf(freq2*(t-eps)+phase2);
				flyRp = std::max(flyRp, outerR*0.7f); flyRm = std::max(flyRm, outerR*0.7f);
				cy::Vec3f pp = fiberCurve(t+eps, yarnA, yarnH, yarnD, flyRp, flyOmega, basePhi);
				cy::Vec3f pm = fiberCurve(t-eps, yarnA, yarnH, yarnD, flyRm, flyOmega, basePhi);
				tans[i] = (pp - pm).GetNormalized();
			}
			generateTube(curve, tans, flyTubeR, 4, pos, nrm, tan);
		}
	}
}

// ════════════════════════════════════════════════════════════════════════
//  OpenGL globals
// ════════════════════════════════════════════════════════════════════════

GLuint vao, posVbo, normVbo, tanVbo;
int VertexCount = 0;
cy::GLSLProgram program;
cy::Vec3f bboxMin, bboxMax, objectCenter;
cy::Vec3f lightPos(40.f, 35.f, 30.f);

cy::GLRenderDepth2D renderBuffer;
cy::GLSLProgram planeProgram;
GLuint planeVao, planeVbo;
GLuint lightPointVao, lightPointVbo;

int currentShading = 0;
const char* shadingNames[] = { "Blinn-Phong", "Kajiya-Kay", "Marschner" };

int currentGeom = 0;
const char* geomNames[] = { "Yarn tubes", "Fiber tubes" };
bool needRebuild = true;

cy::Matrix4f objectRotation = cy::Matrix4f::RotationXYZ(0.f, 0.f, 0.f);
float camDist = 55.f, camYaw = 0.f, camPitch = 0.25f;
double prevMouseX, prevMouseY;
bool mouseLeft = false, mouseRight = false, mouseMiddle = false;
int windowWidth = 1024, windowHeight = 768;

// FPS tracking
double lastFpsTime = 0.0;
int    frameCount  = 0;
float  currentFps  = 0.f;

// ════════════════════════════════════════════════════════════════════════
//  (Re)build geometry VBOs
// ════════════════════════════════════════════════════════════════════════

static void rebuildGeometry()
{
	std::vector<cy::Vec3f> pos, nrm, tan;

	if (currentGeom == 1)
		buildFiberTubes(pos, nrm, tan);
	else
		buildYarnTubes(pos, nrm, tan);

	VertexCount = (int)pos.size();

	bboxMin = bboxMax = pos[0];
	for (auto& v : pos) {
		bboxMin.x = std::min(bboxMin.x, v.x); bboxMin.y = std::min(bboxMin.y, v.y); bboxMin.z = std::min(bboxMin.z, v.z);
		bboxMax.x = std::max(bboxMax.x, v.x); bboxMax.y = std::max(bboxMax.y, v.y); bboxMax.z = std::max(bboxMax.z, v.z);
	}
	objectCenter = (bboxMin + bboxMax) * 0.5f;

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	glBufferData(GL_ARRAY_BUFFER, pos.size()*sizeof(cy::Vec3f), pos.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, normVbo);
	glBufferData(GL_ARRAY_BUFFER, nrm.size()*sizeof(cy::Vec3f), nrm.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, tanVbo);
	glBufferData(GL_ARRAY_BUFFER, tan.size()*sizeof(cy::Vec3f), tan.data(), GL_STATIC_DRAW);

	needRebuild = false;
}

// ════════════════════════════════════════════════════════════════════════
//  Display
// ════════════════════════════════════════════════════════════════════════

static void myDisplay()
{
	if (needRebuild) rebuildGeometry();

	float aspect = (float)windowWidth / (float)windowHeight;
	cy::Matrix4f proj = cy::Matrix4f::Perspective(45.f*(PI/180.f), aspect, 0.1f, 1000.f);
	cy::Vec3f eye(
		camDist*cosf(camPitch)*sinf(camYaw),
		camDist*sinf(camPitch),
		camDist*cosf(camPitch)*cosf(camYaw));
	cy::Matrix4f view = cy::Matrix4f::View(eye, cy::Vec3f(0,0,0), cy::Vec3f(0,1,0));
	cy::Matrix4f model = objectRotation * cy::Matrix4f::Translation(-objectCenter);

	// pass 1: shadow map
	cy::Matrix4f lProj = cy::Matrix4f::Perspective(90.f*(PI/180.f), 1.f, 1.f, 500.f);
	cy::Matrix4f lView = cy::Matrix4f::View(lightPos, objectCenter, cy::Vec3f(0,1,0));
	cy::Matrix4f lMat  = lProj * lView;

	renderBuffer.Bind();
	glClear(GL_DEPTH_BUFFER_BIT);
	program.Bind();
	program.SetUniformMatrix4("mvp", (lMat * model).cell);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, VertexCount);
	renderBuffer.Unbind();

	// pass 2: shaded scene
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	cy::Matrix4f bias_m = cy::Matrix4f::Translation(cy::Vec3f(.5f,.5f,.5f)) * cy::Matrix4f::Scale(.5f);
	cy::Matrix4f sMat = bias_m * lMat;
	cy::Matrix4f mv  = view * model;
	cy::Matrix4f mvp = proj * mv;
	cy::Matrix3f nMat = mv.GetSubMatrix3().GetInverse().GetTranspose();
	cy::Vec4f lv4 = view * cy::Vec4f(lightPos, 1.f);

	program.Bind();
	program.SetUniformMatrix4("mv",  mv.cell);
	program.SetUniformMatrix4("mvp", mvp.cell);
	program.SetUniformMatrix3("normalMat", nMat.cell);
	program.SetUniform("lightPos", lv4.x, lv4.y, lv4.z);
	program.SetUniform("lightIntensity", lightIntensity);
	program.SetUniform("shadingModel", currentShading);
	program.SetUniform("baseColor", yarnColor[0], yarnColor[1], yarnColor[2]);

	// Blinn-Phong params
	program.SetUniform("bp_ambient",   bp_ambient);
	program.SetUniform("bp_diffuse",   bp_diffuse);
	program.SetUniform("bp_specular",  bp_specular);
	program.SetUniform("bp_shininess", bp_shininess);
	program.SetUniform("bp_wrap",      bp_wrap);
	// Kajiya-Kay params
	program.SetUniform("kk_ambient",        kk_ambient);
	program.SetUniform("kk_diffuse",        kk_diffuse);
	program.SetUniform("kk_specPrimary",    kk_specPrimary);
	program.SetUniform("kk_specSecondary",  kk_specSecondary);
	program.SetUniform("kk_shinyPrimary",   kk_shinyPrimary);
	program.SetUniform("kk_shinySecondary", kk_shinySecondary);
	// Marschner params
	program.SetUniform("m_ambient",      m_ambient);
	program.SetUniform("m_alphaR",       m_alphaR);
	program.SetUniform("m_betaR",        m_betaR);
	program.SetUniform("m_R_strength",   m_R_strength);
	program.SetUniform("m_TT_strength",  m_TT_strength);
	program.SetUniform("m_TRT_strength", m_TRT_strength);

	program.SetUniformMatrix4("shadowMatrix", (sMat * model).cell);
	renderBuffer.BindTexture(0);
	program.SetUniform("shadowMap", 0);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, VertexCount);

	if (showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// floor
	float floorY = bboxMin.y - objectCenter.y - 1.f;
	planeProgram.Bind();
	cy::Matrix4f planeMvp = proj * view;
	cy::Matrix3f planeNM  = view.GetSubMatrix3().GetInverse().GetTranspose();
	planeProgram.SetUniformMatrix4("mvp", planeMvp.cell);
	planeProgram.SetUniformMatrix4("mv",  view.cell);
	planeProgram.SetUniformMatrix3("normalMat", planeNM.cell);
	planeProgram.SetUniform("lightPos", lv4.x, lv4.y, lv4.z);
	planeProgram.SetUniform("lightIntensity", lightIntensity);
	planeProgram.SetUniform("color", 0.45f, 0.45f, 0.45f);
	planeProgram.SetUniformMatrix4("shadowMatrix", sMat.cell);
	renderBuffer.BindTexture(0);
	planeProgram.SetUniform("shadowMap", 0);

	float sz = 80.f;
	float fv[] = { -sz,floorY,-sz, sz,floorY,-sz, sz,floorY,sz, -sz,floorY,-sz, sz,floorY,sz, -sz,floorY,sz };
	glBindVertexArray(planeVao);
	glBindBuffer(GL_ARRAY_BUFFER, planeVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fv), fv, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// light indicator
	cy::Matrix4f lMvp2 = proj * view * cy::Matrix4f::Translation(lightPos);
	planeProgram.SetUniformMatrix4("mvp", lMvp2.cell);
	planeProgram.SetUniform("color", 1.f, 1.f, 0.f);
	cy::Matrix4f dummyS = cy::Matrix4f::Translation(cy::Vec3f(-99,-99,-99));
	planeProgram.SetUniformMatrix4("shadowMatrix", dummyS.cell);
	glPointSize(12.f);
	glBindVertexArray(lightPointVao);
	glDrawArrays(GL_POINTS, 0, 1);
}

// ════════════════════════════════════════════════════════════════════════
//  ImGui panel
// ════════════════════════════════════════════════════════════════════════

static void drawImGuiPanel()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// FPS counter
	frameCount++;
	double now = glfwGetTime();
	if (now - lastFpsTime >= 0.5) {
		currentFps = (float)frameCount / (float)(now - lastFpsTime);
		frameCount = 0;
		lastFpsTime = now;
	}

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);

	ImGui::Begin("Debug / Parameters");

	// ── Performance ──
	if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("FPS: %.1f", currentFps);
		ImGui::Text("Frame time: %.2f ms", currentFps > 0 ? 1000.f/currentFps : 0.f);
		ImGui::Text("Triangles: %s", []{
			static char buf[32];
			if (VertexCount/3 >= 1000000) snprintf(buf, sizeof(buf), "%.2fM", VertexCount/3/1e6f);
			else if (VertexCount/3 >= 1000) snprintf(buf, sizeof(buf), "%.1fK", VertexCount/3/1e3f);
			else snprintf(buf, sizeof(buf), "%d", VertexCount/3);
			return buf;
		}());
		ImGui::Text("Vertices: %d", VertexCount);
	}

	// ── System info ──
	if (ImGui::CollapsingHeader("System Info")) {
		ImGui::Text("GPU: %s", glGetString(GL_RENDERER));
		ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
		ImGui::Text("GL Version: %s", glGetString(GL_VERSION));
		ImGui::Text("GLSL: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
		#ifdef __aarch64__
		ImGui::Text("CPU: Apple Silicon (arm64)");
		#else
		ImGui::Text("CPU: x86_64");
		#endif
		ImGui::Text("Window: %dx%d", windowWidth, windowHeight);
	}

	// ── Shading model ──
	if (ImGui::CollapsingHeader("Shading Model", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::RadioButton("Blinn-Phong (1)", &currentShading, 0);
		ImGui::RadioButton("Kajiya-Kay (2)", &currentShading, 1);
		ImGui::RadioButton("Marschner (3)", &currentShading, 2);
	}

	// ── Geometry ──
	if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) {
		int prevGeom = currentGeom;
		ImGui::RadioButton("Yarn tubes (4)", &currentGeom, 0);
		ImGui::RadioButton("Fiber tubes (5)", &currentGeom, 1);
		if (currentGeom != prevGeom) needRebuild = true;

		int prevFibers = fiberCount;
		ImGui::SliderInt("Fiber count", &fiberCount, 2, 48);
		if (fiberCount != prevFibers && currentGeom == 1) needRebuild = true;

		ImGui::Checkbox("Wireframe", &showWireframe);
	}

	// ── Yarn parameters ──
	if (ImGui::CollapsingHeader("Yarn Parameters")) {
		bool changed = false;
		changed |= ImGui::SliderFloat("Loop roundness (a)", &yarnA, 0.1f, 3.0f);
		changed |= ImGui::SliderFloat("Loop height (h)", &yarnH, 1.0f, 8.0f);
		changed |= ImGui::SliderFloat("Loop depth (d)", &yarnD, 0.1f, 3.0f);
		changed |= ImGui::SliderFloat("Fiber twist", &yarnOmega, 1.0f, 20.0f);
		changed |= ImGui::SliderFloat("Fiber orbit radius", &yarnRadius, 0.1f, 1.5f);
		if (changed) needRebuild = true;

		if (ImGui::Button("Reset defaults")) {
			yarnA = 1.5f; yarnH = 4.0f; yarnD = 1.0f; yarnOmega = 9.0f; yarnRadius = 0.50f;
			needRebuild = true;
		}
	}

	// ── Lighting / Color ──
	if (ImGui::CollapsingHeader("Lighting / Color")) {
		ImGui::SliderFloat("Intensity", &lightIntensity, 0.2f, 5.0f);
		ImGui::DragFloat3("Light pos", &lightPos.x, 0.5f, -100.f, 100.f);
		ImGui::ColorEdit3("Yarn color", yarnColor);
	}

	// ── Blinn-Phong params ──
	if (ImGui::CollapsingHeader("Blinn-Phong Params")) {
		ImGui::SliderFloat("BP Ambient",   &bp_ambient,   0.0f, 1.0f);
		ImGui::SliderFloat("BP Diffuse",   &bp_diffuse,   0.0f, 2.0f);
		ImGui::SliderFloat("BP Specular",  &bp_specular,  0.0f, 2.0f);
		ImGui::SliderFloat("BP Shininess", &bp_shininess,  1.0f, 256.0f);
		ImGui::SliderFloat("BP Wrap",      &bp_wrap,       0.0f, 1.0f);
		if (ImGui::Button("Reset BP")) { bp_ambient=0.25f; bp_diffuse=0.70f; bp_specular=0.70f; bp_shininess=64.f; bp_wrap=0.45f; }
	}

	// ── Kajiya-Kay params ──
	if (ImGui::CollapsingHeader("Kajiya-Kay Params")) {
		ImGui::SliderFloat("KK Ambient",        &kk_ambient,        0.0f, 1.0f);
		ImGui::SliderFloat("KK Diffuse",        &kk_diffuse,        0.0f, 2.0f);
		ImGui::SliderFloat("KK Spec Primary",   &kk_specPrimary,    0.0f, 2.0f);
		ImGui::SliderFloat("KK Spec Secondary", &kk_specSecondary,  0.0f, 2.0f);
		ImGui::SliderFloat("KK Shiny Primary",  &kk_shinyPrimary,   1.0f, 256.0f);
		ImGui::SliderFloat("KK Shiny Secondary",&kk_shinySecondary,  1.0f, 128.0f);
		if (ImGui::Button("Reset KK")) { kk_ambient=0.22f; kk_diffuse=0.60f; kk_specPrimary=0.60f; kk_specSecondary=0.35f; kk_shinyPrimary=80.f; kk_shinySecondary=18.f; }
	}

	// ── Marschner params ──
	if (ImGui::CollapsingHeader("Marschner Params")) {
		ImGui::SliderFloat("M Ambient",       &m_ambient,      0.0f, 1.0f);
		ImGui::SliderFloat("Cuticle tilt (a)", &m_alphaR,      -0.3f, 0.3f);
		ImGui::SliderFloat("Roughness (b)",    &m_betaR,        0.01f, 0.5f);
		ImGui::SliderFloat("R strength",       &m_R_strength,   0.0f, 2.0f);
		ImGui::SliderFloat("TT strength",      &m_TT_strength,  0.0f, 3.0f);
		ImGui::SliderFloat("TRT strength",     &m_TRT_strength, 0.0f, 3.0f);
		if (ImGui::Button("Reset M")) { m_ambient=0.18f; m_alphaR=-0.07f; m_betaR=0.12f; m_R_strength=0.40f; m_TT_strength=1.0f; m_TRT_strength=0.70f; }
	}

	// ── Camera ──
	if (ImGui::CollapsingHeader("Camera")) {
		ImGui::SliderFloat("Distance", &camDist, 2.f, 200.f);
		ImGui::SliderFloat("Yaw", &camYaw, -PI, PI);
		ImGui::SliderFloat("Pitch", &camPitch, -1.5f, 1.5f);
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ════════════════════════════════════════════════════════════════════════
//  Callbacks
// ════════════════════════════════════════════════════════════════════════

static void keyCallback(GLFWwindow* win, int key, int sc, int action, int mod)
{
	// let ImGui handle keys when it wants focus
	ImGui_ImplGlfw_KeyCallback(win, key, sc, action, mod);
	if (ImGui::GetIO().WantCaptureKeyboard) return;

	if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

	if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, GLFW_TRUE);

	if (key == GLFW_KEY_1 && action == GLFW_PRESS) currentShading = 0;
	if (key == GLFW_KEY_2 && action == GLFW_PRESS) currentShading = 1;
	if (key == GLFW_KEY_3 && action == GLFW_PRESS) currentShading = 2;

	if (key == GLFW_KEY_4 && action == GLFW_PRESS) { currentGeom = 0; needRebuild = true; }
	if (key == GLFW_KEY_5 && action == GLFW_PRESS) { currentGeom = 1; needRebuild = true; }

	if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_PRESS) {
		fiberCount = std::max(2, fiberCount - 2);
		if (currentGeom == 1) needRebuild = true;
	}
	if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_PRESS) {
		fiberCount = std::min(48, fiberCount + 2);
		if (currentGeom == 1) needRebuild = true;
	}

	float sp = 1.5f;
	if (key == GLFW_KEY_W || key == GLFW_KEY_UP)    lightPos.z -= sp;
	if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN)  lightPos.z += sp;
	if (key == GLFW_KEY_A || key == GLFW_KEY_LEFT)  lightPos.x -= sp;
	if (key == GLFW_KEY_D || key == GLFW_KEY_RIGHT) lightPos.x += sp;
	if (key == GLFW_KEY_E) lightPos.y += sp;
	if (key == GLFW_KEY_Q) lightPos.y -= sp;
}

static void mouseButtonCB(GLFWwindow* win, int btn, int action, int mod)
{
	ImGui_ImplGlfw_MouseButtonCallback(win, btn, action, mod);
	if (ImGui::GetIO().WantCaptureMouse) return;

	bool down = (action == GLFW_PRESS);
	if (btn == GLFW_MOUSE_BUTTON_LEFT)   mouseLeft   = down;
	if (btn == GLFW_MOUSE_BUTTON_RIGHT)  mouseRight  = down;
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE) mouseMiddle = down;
	glfwGetCursorPos(win, &prevMouseX, &prevMouseY);
}

static void cursorPosCB(GLFWwindow* win, double x, double y)
{
	ImGui_ImplGlfw_CursorPosCallback(win, x, y);
	if (ImGui::GetIO().WantCaptureMouse) { prevMouseX = x; prevMouseY = y; return; }

	double dx = x - prevMouseX, dy = y - prevMouseY;
	if (mouseLeft) {
		camYaw   += (float)dx * 0.01f;
		camPitch += (float)dy * 0.01f;
		camPitch  = std::min(std::max(camPitch, -1.5f), 1.5f);
	} else if (mouseRight) {
		camDist += (float)dy * 0.15f;
		if (camDist < 2.f) camDist = 2.f;
	} else if (mouseMiddle) {
		lightPos.x += (float)dx * 0.3f;
		lightPos.y -= (float)dy * 0.3f;
	}
	prevMouseX = x; prevMouseY = y;
}

static void scrollCB(GLFWwindow* win, double xoff, double yoff)
{
	ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
	if (ImGui::GetIO().WantCaptureMouse) return;
	camDist -= (float)yoff * 3.f;
	if (camDist < 2.f) camDist = 2.f;
}

static void fbSizeCB(GLFWwindow*, int w, int h) { windowWidth = w; windowHeight = h; }

// ════════════════════════════════════════════════════════════════════════
//  main
// ════════════════════════════════════════════════════════════════════════

int main(int /*argc*/, char** /*argv*/)
{
	if (!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return 1; }

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	GLFWwindow* win = glfwCreateWindow(windowWidth, windowHeight,
		"YarnRender — Colin Galbraith", NULL, NULL);
	if (!win) { glfwTerminate(); return 1; }
	glfwMakeContextCurrent(win);
	glfwSwapInterval(1); // vsync

	glewExperimental = GL_TRUE;
	glewInit();
	glGetError();

	// ── ImGui setup ──
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.f;
	style.FrameRounding  = 4.f;
	style.Alpha = 0.92f;

	ImGui_ImplGlfw_InitForOpenGL(win, false); // false = we install callbacks ourselves
	ImGui_ImplOpenGL3_Init("#version 330");

	glEnable(GL_DEPTH_TEST);

	if (!program.BuildFiles("shader.vert", "shader.frag")) return 1;
	if (!planeProgram.BuildFiles("plane.vert", "plane.frag")) return 1;

	// ── yarn VAO + VBOs ──
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &posVbo);
	glBindBuffer(GL_ARRAY_BUFFER, posVbo);
	GLuint loc = glGetAttribLocation(program.GetID(), "pos");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glGenBuffers(1, &normVbo);
	glBindBuffer(GL_ARRAY_BUFFER, normVbo);
	loc = glGetAttribLocation(program.GetID(), "normal");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glGenBuffers(1, &tanVbo);
	glBindBuffer(GL_ARRAY_BUFFER, tanVbo);
	loc = glGetAttribLocation(program.GetID(), "tangent");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// floor VAO
	glGenVertexArrays(1, &planeVao);
	glBindVertexArray(planeVao);
	glGenBuffers(1, &planeVbo);
	glBindBuffer(GL_ARRAY_BUFFER, planeVbo);
	loc = glGetAttribLocation(planeProgram.GetID(), "pos");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// light-point VAO
	glGenVertexArrays(1, &lightPointVao);
	glBindVertexArray(lightPointVao);
	glGenBuffers(1, &lightPointVbo);
	glBindBuffer(GL_ARRAY_BUFFER, lightPointVbo);
	cy::Vec3f origin(0,0,0);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f), &origin, GL_STATIC_DRAW);
	loc = glGetAttribLocation(planeProgram.GetID(), "pos");
	glEnableVertexAttribArray(loc);
	glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// shadow FBO
	renderBuffer.Initialize(true, 4096, 4096);
	renderBuffer.BindTexture();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glClearColor(0.f, 0.f, 0.f, 0.f);

	// install callbacks (after ImGui init so we can forward events)
	glfwSetKeyCallback(win, keyCallback);
	glfwSetMouseButtonCallback(win, mouseButtonCB);
	glfwSetCursorPosCallback(win, cursorPosCB);
	glfwSetScrollCallback(win, scrollCB);
	glfwSetFramebufferSizeCallback(win, fbSizeCB);
	glfwSetCharCallback(win, ImGui_ImplGlfw_CharCallback);
	glfwGetFramebufferSize(win, &windowWidth, &windowHeight);

	lastFpsTime = glfwGetTime();

	while (!glfwWindowShouldClose(win)) {
		glfwPollEvents();
		myDisplay();
		drawImGuiPanel();
		glfwSwapBuffers(win);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(win);
	glfwTerminate();
	return 0;
}
