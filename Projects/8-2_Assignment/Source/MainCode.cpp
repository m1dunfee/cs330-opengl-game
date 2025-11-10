#include <GLFW\glfw3.h>
#include "linmath.h"
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>
#include <vector>
//#include <windows.h>
#include <time.h>
#include <cmath>

// Forward declarations (types)
struct Walls;   // for prototypes
struct Rect;
struct GameState;
class Circle;
class Brick;
struct Paddle;

// Forward declarations (functions)
void processInput(GLFWwindow* window, GameState& g);
void updateGame(GameState& g);
void renderGame(const GameState& g, GLFWwindow* window);

bool circlesVsBricks(size_t ci, GameState& g);
void circlesVsPaddles(Circle& c, GameState& g);
void circlesVsWalls(Circle& c, GameState& g);


// ============================
// Global Variables
// ============================
using namespace std;

const float DEG2RAD = 3.14159 / 180;

void processInput(GLFWwindow* window);

enum BRICKTYPE { REFLECTIVE, DESTRUCTABLE };
enum ONOFF { ON, OFF };

// ============================
// Math helpers
// ============================
static inline float clampf(float v, float a, float b) {
	return v < a ? a : (v > b ? b : v);
}

struct Closest {
	float px, py;   // closest point on target to circle center
};

// ============================
// Rect helper struct
// ============================
struct Rect {
	float cx, cy, hw, hh;

	Closest closestPoint(float x, float y) const {
		auto clampf = [](float v, float a, float b) {
			return v < a ? a : (v > b ? b : v);
			};
		float minx = cx - hw, maxx = cx + hw;
		float miny = cy - hh, maxy = cy + hh;
		return Closest{ clampf(x, minx, maxx), clampf(y, miny, maxy) };
	}
};

// ============================
// Walls.h / Walls struct
// ============================
struct Walls {
	float left, right, top, bottom;

	Walls() : left(-1.0f), right(1.0f), top(1.0f), bottom(-1.0f) {}
	Walls(float l, float r, float t, float b) : left(l), right(r), top(t), bottom(b) {}

	Closest closestPoint(float x, float y) const {
		auto clampf = [](float v, float a, float b) {
			return v < a ? a : (v > b ? b : v);
			};
		return Closest{ clampf(x, left, right), clampf(y, bottom, top) };
	}
};

// ============================
// Paddle.h / Paddle struct
// ============================
struct Paddle {
	float x;        // Center x position
	float y;        // Center y position
	float w;        // Width
	float h;        // Height
	float r, g, b;  // Color (RGB)

	// Construct paddle with default values
	Paddle() : x(0), y(0), w(0.3f), h(0.05f), r(1), g(1), b(1) {}

	Paddle(float xx, float yy, float ww, float hh, float rr, float gg, float bb)
		: x(xx), y(yy), w(ww), h(hh), r(rr), g(gg), b(bb) {
	}

	// Return an AABB for collision checks
	Rect rect() const {
		Rect R{ x, y, 0.5f * w, 0.5f * h };
		return R;
	}

	// Move horizontally (clamped between walls)
	void move(float dx) {
		float half = 0.5f * w;
		x += dx;
		if (x < -1.0f + half) x = -1.0f + half;
		if (x > 1.0f - half) x = 1.0f - half;
	}

	// Draw paddle
	void draw() const {
		float hx = 0.5f * w;
		float hy = 0.5f * h;
		glColor3f(r, g, b);
		glBegin(GL_QUADS);
		glVertex2f(x - hx, y - hy);
		glVertex2f(x + hx, y - hy);
		glVertex2f(x + hx, y + hy);
		glVertex2f(x - hx, y + hy);
		glEnd();
	}
};

// ============================
// Brick.h / Brick Class
class Brick {
public:
	float red, green, blue;
	float x, y, width;
	int health;
	BRICKTYPE brick_type;
	ONOFF onoff;

	Brick(BRICKTYPE bt, float xx, float yy, float ww, float rr, float gg, float bb)
		: brick_type(bt), x(xx), y(yy), width(ww),
		red(rr), green(gg), blue(bb), health(3), onoff(ON) {
	}

	void takeHit() {
		health--;
		if (health <= 0) onoff = OFF;
		else updateColor();
	}

	void updateColor() {
		switch (health) {
		case 2: red = 1.0f; green = 0.6f; blue = 0.0f; break;  // orange
		case 1: red = 0.5f; green = 0.25f; blue = 0.0f; break; // brown
		default: red = 0.2f; green = 1.0f; blue = 0.2f; break; // green
		}
	}

	Rect rect() const {
		float hw = 0.5f * width;
		return Rect{ x, y, hw, hw };
	}

	void drawBrick() const {
		if (onoff == ON) {
			float halfside = width / 2;
			glColor3f(red, green, blue);
			glBegin(GL_POLYGON);
			glVertex2f(x + halfside, y + halfside);
			glVertex2f(x + halfside, y - halfside);
			glVertex2f(x - halfside, y - halfside);
			glVertex2f(x - halfside, y + halfside);
			glEnd();
		}
	}
};


// ============================
// Circle.h / Circle Class
// ============================
class Circle {
public:
	float red{}, green{}, blue{};
	float radius{};
	float x{}, y{};
	float speed{ 0.03f };
	int direction{ 1 }; 
	

	// --- constructor ---
	Circle(float xx, float yy, float rad, int dir, float spd, float rr, float gg, float bb)
		: red(rr), green(gg), blue(bb),
		radius(rad), x(xx), y(yy), speed(spd), direction(dir) {
	}
	// --- movement only ---
	void step() {
		float dx = 0.0f, dy = 0.0f;
		switch (direction) {
		case 1: dy = +speed; break;                  // up
		case 2: dx = +speed; break;                  // right
		case 3: dy = -speed; break;                  // down
		case 4: dx = -speed; break;                  // left
		case 5: dx = +speed; dy = +speed; break;     // up-right
		case 6: dx = -speed; dy = +speed; break;     // up-left
		case 7: dx = +speed; dy = -speed; break;     // down-right
		case 8: dx = -speed; dy = -speed; break;     // down-left
		}
		x += dx; y += dy;
	}

	// --- universal collision kernel: circle vs *point* ---
	// Push out along normal and flip the dominant axis in 'direction'.
	bool collidePoint(float px, float py) {
		float dx = x - px;
		float dy = y - py;
		float d2 = dx * dx + dy * dy;
		float r = radius;

		if (d2 > r * r) return false; // no overlap

		// If the point is at center (degenerate), nudge
		if (d2 < 1e-12f) { dx = 1.0f; dy = 0.0f; d2 = 1.0f; }

		float d = sqrtf(d2);
		float nx = dx / d;  // outward normal
		float ny = dy / d;

		// minimal positional correction: center = point + normal * r
		x = px + nx * r;
		y = py + ny * r;

		// flip dominant axis of motion based on normal direction
		if (fabsf(nx) > fabsf(ny)) flipX(); else flipY();
		return true;
	}

	// Adapters for shapes
	bool collideRect(const Rect& R) {
		Closest c = R.closestPoint(x, y);
		return collidePoint(c.px, c.py);
	}

	bool collideWalls(const Walls& w) {
		bool hit = false;

		if (x - radius <= w.left) { x = w.left + radius; flipX(); hit = true; }
		if (x + radius >= w.right) { x = w.right - radius; flipX(); hit = true; }
		if (y - radius <= w.bottom) { y = w.bottom + radius; flipY(); hit = true; }
		if (y + radius >= w.top) { y = w.top - radius; flipY(); hit = true; }

		return hit;
	}

	void drawCircle() const {
		glColor3f(red, green, blue);
		glBegin(GL_POLYGON);
		for (int i = 0; i < 360; ++i) {
			float a = i * (3.14159265f / 180.0f);
			glVertex2f(x + cosf(a) * radius, y + sinf(a) * radius);
		}
		glEnd();
	}

private:
	void flipX() {
		switch (direction) {
		case 2: direction = 4; break; case 4: direction = 2; break;
		case 5: direction = 6; break; case 6: direction = 5; break;
		case 7: direction = 8; break; case 8: direction = 7; break;
		default: break;
		}
	}
	void flipY() {
		switch (direction) {
		case 1: direction = 3; break; case 3: direction = 1; break;
		case 5: direction = 7; break; case 7: direction = 5; break;
		case 6: direction = 8; break; case 8: direction = 6; break;
		default: break;
		}
	}
};


// ============================
// GameState.h / GameState struct
// ============================
struct GameState {
	vector<Circle> Circles;
	vector<Brick> Bricks;
	vector<Paddle> Players;     // incase I want to make multiplayer later with arrow keys
	Walls  walls;               // World boundaries
};

// ========================================================
// Main life cycles
// ========================================================
// 
// ============================
// systemInit
// ============================
GLFWwindow* systemInit() {
	int screensize = 480;

	srand(time(NULL));

	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	GLFWwindow* window = glfwCreateWindow(screensize, screensize, "8-2 Assignment", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	return window;
}

// Make a classic "Space Invader" sprite out of DESTRUCTABLE bricks.
static void loadInvaderLevel(GameState& g) {
	g.Bricks.clear();

	// 1 = brick present, 0 = empty
	// 11 columns x 8 rows
	const int COLS = 11;
	const int ROWS = 8;
	const char* rows[ROWS] = {
		"00111111100",
		"01111111110",
		"11101110111",
		"11111111111",
		"10111111101",
		"10100100101",
		"00111001100",
		"00010000100"
	};

	// Layout params (NDC coordinates, -1..+1)
	const float brickSize = 0.05f;   // width of each (square) brick
	const float padding = 0.010f;  // gap between bricks
	const float cell = brickSize + padding;
	const float topCenterY = 0.65f;   // where the top row is centered vertically
	const float centerX = 0.0f;    // horizontally centered
	const float halfCols = (COLS - 1) * 0.5f;

	// Colors for the sprite
	const float rr = 0.2f, gg = 1.0f, bb = 0.6f;

	for (int r = 0; r < ROWS; ++r) {
		for (int c = 0; c < COLS; ++c) {
			if (rows[r][c] == '1') {
				float x = centerX + (c - halfCols) * cell;
				float y = topCenterY - r * cell;
				g.Bricks.emplace_back(DESTRUCTABLE, x, y, brickSize, rr, gg, bb);
			}
		}
	}
}


// ============================
// gameInit
// ============================
static void gameInit(GameState& g) {
	g.Players.clear();
	g.Players.emplace_back(0.0f, -0.9f, 0.3f, 0.05f, 0.8f, 0.8f, 1.0f);  // P1
	//g.p2 = Paddle{ 0.0f,  0.9f, 0.3f, 0.05f, 0.8f, 1.0f, 0.8f };
	g.walls = Walls(-1.0f, 1.0f, 1.0f, -1.0f); // left=-1, right=+1, top=+1, bottom=-1
	//loadLevel(g.bricks); // load from csv later
	loadInvaderLevel(g);
}

// ============================
// processInput
// ============================
void processInput(GameState& g, GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) g.Players[0].move(-0.03f);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) g.Players[0].move(+0.03f);
	//if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) g.p2.move(-0.03f);
	//if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) g.p2.move(+0.03f);

	// Spawn ball with SPACE
	static bool spacePrev = false;
	bool spaceNow = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
	if (spaceNow && !spacePrev) {
		const Paddle& p = g.Players[0];
		float radius = 0.02f;
		float spawnY = p.y + 0.5f * p.h + radius + 0.01f;
		int dir = 1; // up
		float rr = float(rand()) / RAND_MAX, gg = float(rand()) / RAND_MAX, bb = float(rand()) / RAND_MAX;
		g.Circles.emplace_back(p.x, spawnY, radius, dir, 0.05f, rr, gg, bb);
	}
	spacePrev = spaceNow;

}

// ============================
// updateGame
// ============================
void updateGame(GameState& g) {
	for (size_t i = 0; i < g.Circles.size(); ) {
		g.Circles[i].step();

		if (circlesVsBricks(i, g)) {
			// After swap-pop, index i now refers to the element that was at the end; keep i unchanged this iteration.
			continue;
		}

		circlesVsPaddles(g.Circles[i], g);
		circlesVsWalls(g.Circles[i], g);
		++i; // only advance when we didn't remove
	}
}


// returns true if the circle was consumed (removed)
bool circlesVsBricks(size_t ci, GameState& g) {
	Circle& c = g.Circles[ci];
	for (size_t i = 0; i < g.Bricks.size(); ++i) {
		Brick& br = g.Bricks[i];
		if (br.onoff == OFF) continue;

		if (c.collideRect(br.rect())) {
			if (br.brick_type == DESTRUCTABLE) {
				br.takeHit();
				//  remove circle in O(1) (order not preserved)
				g.Circles[ci] = g.Circles.back();
				g.Circles.pop_back();
				return true; // consumed
			}
		}
	}
	return false;
}

void circlesVsPaddles(Circle& c, GameState& g) {
	// Early-exit if no players
	if (g.Players.empty()) return;

	// Try each paddle. If you want only one bounce per frame, break on first hit.
	for (Paddle& p : g.Players) {
		if (c.collideRect(p.rect())) {
			// optional: break;   // uncomment to allow only one paddle collision per frame
		}
	}
}

void circlesVsWalls(Circle& c, GameState& g) {
	c.collideWalls(g.walls);
}

// ============================
// renderGame
// ============================
void renderGame(const GameState& g, GLFWwindow* window) {
	//Setup View
	float ratio;
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	ratio = width / (float)height;
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);

	for (const Brick& b : g.Bricks)   b.drawBrick();
	for (const Circle& c : g.Circles)  c.drawCircle();
	for (const Paddle& p : g.Players)  p.draw();

	glfwSwapBuffers(window);
	glfwPollEvents();
}

// ============================
// cleanup
// ============================
void cleanup(GLFWwindow* window) {
	glfwDestroyWindow(window);
	glfwTerminate();
}

// ============================
// Main 
// ============================
int main(void) {
	//system setup
	GLFWwindow* window = systemInit();
	//load resources
		//import skins
	GameState sessionState = GameState();
	//game setup
	gameInit(sessionState);


	//main game life cycle
	while (!glfwWindowShouldClose(window)) {
		//input
		processInput(sessionState, window);
		//update
		updateGame(sessionState);
		//render
		renderGame(sessionState, window);
	}

	cleanup(window);
	exit(EXIT_SUCCESS);
}



