#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cyCore.h"
#include "cyVector.h"
#include "cyMatrix.h"
#include "cyGL.h"

// ── Yarn tweakable parameters ──
extern int   fiberCount;
extern float yarnA;
extern float yarnH;
extern float yarnD;
extern float yarnOmega;
extern float yarnRadius;
extern float lightIntensity;
extern float yarnColor[3];
extern bool  showWireframe;

// ── Blinn-Phong parameters ──
extern float bp_ambient;
extern float bp_diffuse;
extern float bp_specular;
extern float bp_shininess;
extern float bp_wrap;

// ── Kajiya-Kay parameters ──
extern float kk_ambient;
extern float kk_diffuse;
extern float kk_specPrimary;
extern float kk_specSecondary;
extern float kk_shinyPrimary;
extern float kk_shinySecondary;

// ── Marschner parameters ──
extern float m_ambient;
extern float m_alphaR;
extern float m_betaR;
extern float m_R_strength;
extern float m_TT_strength;
extern float m_TRT_strength;

// ── OpenGL objects ──
extern GLuint vao, posVbo, normVbo, tanVbo;
extern int VertexCount;
extern cy::GLSLProgram program;
extern cy::Vec3f bboxMin, bboxMax, objectCenter;
extern cy::Vec3f lightPos;

extern cy::GLRenderDepth2D renderBuffer;
extern cy::GLSLProgram planeProgram;
extern GLuint planeVao, planeVbo;
extern GLuint lightPointVao, lightPointVbo;

// ── State ──
extern int currentShading;
extern const char* shadingNames[];
extern int currentGeom;
extern const char* geomNames[];
extern bool needRebuild;

// ── Camera ──
extern cy::Matrix4f objectRotation;
extern float camDist, camYaw, camPitch;
extern double prevMouseX, prevMouseY;
extern bool mouseLeft, mouseRight, mouseMiddle;
extern int windowWidth, windowHeight;

// ── FPS ──
extern double lastFpsTime;
extern int    frameCount;
extern float  currentFps;
