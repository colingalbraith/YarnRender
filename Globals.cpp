#include "Globals.h"

// ── Yarn tweakable parameters ──
int   fiberCount   = 24;
float yarnA        = 1.5f;
float yarnH        = 4.0f;
float yarnD        = 1.0f;
float yarnOmega    = 9.0f;
float yarnRadius   = 0.50f;
float lightIntensity = 1.8f;
float yarnColor[3] = { 0.65f, 0.30f, 0.35f };
bool  showWireframe = false;

// ── Blinn-Phong parameters ──
float bp_ambient   = 0.25f;
float bp_diffuse   = 0.70f;
float bp_specular  = 0.70f;
float bp_shininess = 64.0f;
float bp_wrap      = 0.45f;

// ── Kajiya-Kay parameters ──
float kk_ambient       = 0.22f;
float kk_diffuse       = 0.60f;
float kk_specPrimary   = 0.60f;
float kk_specSecondary = 0.35f;
float kk_shinyPrimary  = 80.0f;
float kk_shinySecondary = 18.0f;
float kk_normalInfluence = 0.35f;

// ── Marschner parameters ──
float m_ambient      = 0.18f;
float m_alphaR       = -0.07f;
float m_betaR        = 0.12f;
float m_R_strength   = 0.40f;
float m_TT_strength  = 1.00f;
float m_TRT_strength = 0.70f;
float m_normalInfluence = 0.35f;

// ── Visual enhancements ──
float colorVariation = 0.5f;
float exposure = 1.0f;
bool  gammaEnabled = true;
bool  bgGradientEnabled = true;
float bgColorTop[3] = { 0.15f, 0.15f, 0.20f };
float bgColorBot[3] = { 0.05f, 0.05f, 0.07f };
bool  checkerEnabled = true;
GLuint colVbo = 0;
GLuint bgVao = 0;
cy::GLSLProgram bgProgram;

// ── OpenGL objects ──
GLuint vao, posVbo, normVbo, tanVbo;
int VertexCount = 0;
cy::GLSLProgram program;
cy::Vec3f bboxMin, bboxMax, objectCenter;
cy::Vec3f lightPos(40.f, 35.f, 30.f);

cy::GLRenderDepth2D renderBuffer;
cy::GLSLProgram planeProgram;
GLuint planeVao, planeVbo;
GLuint lightPointVao, lightPointVbo;

// ── State ──
int currentShading = 0;
const char* shadingNames[] = { "Blinn-Phong", "Kajiya-Kay", "Marschner" };
int currentGeom = 0;
const char* geomNames[] = { "Yarn tubes", "Fiber tubes" };
bool needRebuild = true;

// ── Camera ──
cy::Matrix4f objectRotation = cy::Matrix4f::RotationXYZ(0.f, 0.f, 0.f);
float camDist = 55.f, camYaw = 0.f, camPitch = 0.25f;
double prevMouseX, prevMouseY;
bool mouseLeft = false, mouseRight = false, mouseMiddle = false;
int windowWidth = 1024, windowHeight = 768;

// ── Deep Opacity Maps ──
bool  domEnabled     = false;
float domLayerRange  = 4.0f;
float domFragOpacity = 0.04f;
GLuint domDepthFBO = 0, domDepthTex = 0, domDepthRB = 0;
GLuint domOpacityFBO = 0, domOpacityTex = 0;
cy::GLSLProgram domDepthProg, domOpacityProg;
int domResolution = 2048;
int domDebug = 0;

// ── FPS ──
double lastFpsTime = 0.0;
int    frameCount  = 0;
float  currentFps  = 0.f;
