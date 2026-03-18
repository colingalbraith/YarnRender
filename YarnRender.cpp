// Colin Galbraith U1592430
// YarnRender — parametric yarn visualization with shadow mapping
// Shading models: 1=Blinn-Phong  2=Kajiya-Kay  3=Marschner
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "Globals.h"
#include "YarnMath.h"
#include "YarnGeometry.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// ════════════════════════════════════════════════════════════════════════
//  (Re)build geometry VBOs
// ════════════════════════════════════════════════════════════════════════

static void rebuildGeometry()
{
	std::vector<cy::Vec3f> pos, nrm, tan;

	YarnParams p = { fiberCount, yarnA, yarnH, yarnD, yarnOmega, yarnRadius };

	if (currentGeom == 1)
		buildFiberTubes(p, pos, nrm, tan);
	else
		buildYarnTubes(p, pos, nrm, tan);

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

	ImGui_ImplGlfw_InitForOpenGL(win, false);
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

	// install callbacks
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
