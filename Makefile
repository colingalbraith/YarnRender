CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall \
           -DGL_SILENCE_DEPRECATION \
           -DCY_NO_IMMINTRIN_H \
           -I/opt/homebrew/include \
           -Iimgui

LDFLAGS = -L/opt/homebrew/lib \
          -framework OpenGL \
          -lGLEW \
          -lglfw

IMGUI_SRC = imgui/imgui.cpp \
            imgui/imgui_draw.cpp \
            imgui/imgui_tables.cpp \
            imgui/imgui_widgets.cpp \
            imgui/imgui_impl_glfw.cpp \
            imgui/imgui_impl_opengl3.cpp

TARGET = YarnRender
SRC = YarnRender.cpp Globals.cpp YarnMath.cpp YarnGeometry.cpp $(IMGUI_SRC)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
