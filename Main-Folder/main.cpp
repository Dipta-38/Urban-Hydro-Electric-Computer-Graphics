#include <GL/glut.h>
#include <GL/freeglut_ext.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cstring>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =====================================================================
//  KEY LEVELS
// =====================================================================
const float GATE_Y    =  0.5f;
const float TURBINE_Y = -1.5f;
const float RIVER_Y   = -3.5f;
const float CITY_BACK_Z  = -140.0f;
const float CITY_FRONT_Z =  18.5f;
const float CITY_HALF_WIDTH = 120.0f;
const float GATE_Z       =  15.0f;
const float SIDE_CITY_GATE_XS[2] = {-56.0f, 56.0f};
const int NSIDE_CITY_GATES = 2;
const float TURBINE_Z    =  20.0f;
const float RIVER_NEAR   =  TURBINE_Z - 1.5f;
const float RIVER_FAR    = 160.0f;

// =====================================================================
//  TERRAIN
// =====================================================================
float cityYatZ(float z) {
    float t = (z - CITY_BACK_Z) / (CITY_FRONT_Z - CITY_BACK_Z);
    if (t < 0) t = 0; if (t > 1) t = 1;
    return 4.0f - t * 3.5f;
}

const float DEEP_BLUE_R = 0.02f, DEEP_BLUE_G = 0.24f, DEEP_BLUE_B = 0.72f;
const float SKY_BLUE_R  = 0.55f, SKY_BLUE_G  = 0.80f, SKY_BLUE_B  = 1.00f;

// =====================================================================
//  BARRAGE constants
// =====================================================================
const float BARRAGE_Z        = 60.0f;
const float BARRAGE_DECK_Y   = RIVER_Y + 6.5f;    //  3.0
const float BARRAGE_BASE_Y   = RIVER_Y - 1.5f;    // -5.0
const int   BARRAGE_NGATES   = 10;
const float BARRAGE_OVERLOAD_LEVEL = 0.72f;
const float BARRAGE_GATE_W   = 10.0f;
const float BARRAGE_PIER_W   = 2.0f;
const float BARRAGE_SPAN     = BARRAGE_NGATES*BARRAGE_GATE_W + (BARRAGE_NGATES+1)*BARRAGE_PIER_W;
const float BARRAGE_X0       = -BARRAGE_SPAN * 0.5f;

// Turbine wheels sit downstream of the gates, elevated so they are
// always visible above the tailrace (they get hit by the falling sheet).
const float BARRAGE_WHEEL_R  = 1.9f;
const float BARRAGE_WHEEL_SCALE = 1.65f;
const float BARRAGE_WHEEL_Y  = RIVER_Y + 1.6f;    // -1.9
const float BARRAGE_WHEEL_Z  = BARRAGE_Z - 1.2f;

// =====================================================================
//  CAMERA / INPUT
// =====================================================================
float camYaw   = 30.0f;
float camPitch = 32.0f;
float camDist  = 220.0f;
float camTargetX = 0.0f, camTargetY = 0.0f, camTargetZ = 20.0f;
const float CAMERA_MIN_DISTANCE = 0.5f;
const float CAMERA_MAX_DISTANCE = 230.0f;

int lastMouseX = 0, lastMouseY = 0;
int lastFocusClickTime = -1000;
int lastFocusClickX = -1000, lastFocusClickY = -1000;
bool lmb = false, rmb = false, mmb = false;
bool showHUD = false;
bool showRenderControls = false;
bool ambientLightingOn = true;
bool diffuseLightingOn = true;
bool specularLightingOn = true;
bool riverRayTracingOn = true;
bool wheelGouraudOn = true;
bool wheelPhongOn = true;
bool cityWheelPhongOn = false;
int windowWidth = 1100, windowHeight = 750;

static void setSceneLightingEnabled() {
    glEnable(GL_LIGHTING);
}

typedef GLuint (APIENTRY *CreateShaderProc)(GLenum);
typedef void (APIENTRY *ShaderSourceProc)(GLuint, GLsizei, const char**, const GLint*);
typedef void (APIENTRY *CompileShaderProc)(GLuint);
typedef void (APIENTRY *GetShaderivProc)(GLuint, GLenum, GLint*);
typedef void (APIENTRY *GetShaderInfoLogProc)(GLuint, GLsizei, GLsizei*, char*);
typedef GLuint (APIENTRY *CreateProgramProc)(void);
typedef void (APIENTRY *AttachShaderProc)(GLuint, GLuint);
typedef void (APIENTRY *LinkProgramProc)(GLuint);
typedef void (APIENTRY *GetProgramivProc)(GLuint, GLenum, GLint*);
typedef void (APIENTRY *GetProgramInfoLogProc)(GLuint, GLsizei, GLsizei*, char*);
typedef void (APIENTRY *UseProgramProc)(GLuint);
typedef GLint (APIENTRY *GetUniformLocationProc)(GLuint, const char*);
typedef void (APIENTRY *Uniform1iProc)(GLint, GLint);
typedef void (APIENTRY *Uniform1fProc)(GLint, GLfloat);
typedef void (APIENTRY *Uniform3fProc)(GLint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY *DeleteShaderProc)(GLuint);
typedef void (APIENTRY *DeleteProgramProc)(GLuint);

static CreateShaderProc pCreateShader = 0;
static ShaderSourceProc pShaderSource = 0;
static CompileShaderProc pCompileShader = 0;
static GetShaderivProc pGetShaderiv = 0;
static GetShaderInfoLogProc pGetShaderInfoLog = 0;
static CreateProgramProc pCreateProgram = 0;
static AttachShaderProc pAttachShader = 0;
static LinkProgramProc pLinkProgram = 0;
static GetProgramivProc pGetProgramiv = 0;
static GetProgramInfoLogProc pGetProgramInfoLog = 0;
static UseProgramProc pUseProgram = 0;
static GetUniformLocationProc pGetUniformLocation = 0;
static Uniform1iProc pUniform1i = 0;
static Uniform1fProc pUniform1f = 0;
static Uniform3fProc pUniform3f = 0;
static DeleteShaderProc pDeleteShader = 0;
static DeleteProgramProc pDeleteProgram = 0;
static GLuint comparisonShaderProgram = 0;
static GLint comparisonShadingModeLocation = -1;
static GLint comparisonShininessLocation = -1;
static GLint comparisonLightPositionLocation = -1;
static GLint comparisonAmbientLocation = -1;
static GLint comparisonDiffuseLocation = -1;
static GLint comparisonSpecularLocation = -1;
static GLfloat comparisonLightPositionEye[3] = {40.0f, 100.0f, 30.0f};
static GLfloat comparisonAmbientColor[3] = {0.35f, 0.37f, 0.42f};
static GLfloat comparisonDiffuseColor[3] = {0.90f, 0.90f, 0.95f};
static GLfloat comparisonSpecularColor[3] = {0.90f, 0.92f, 1.00f};
static const GLenum FLOOD_VERTEX_SHADER = 0x8B31;
static const GLenum FLOOD_FRAGMENT_SHADER = 0x8B30;
static const GLenum FLOOD_COMPILE_STATUS = 0x8B81;
static const GLenum FLOOD_LINK_STATUS = 0x8B82;

static GLuint riverReflectionTexture = 0;
static const int RIVER_REFLECTION_WIDTH = 128;
static const int RIVER_REFLECTION_HEIGHT = 64;

// =====================================================================
//  SIMULATION STATE
// =====================================================================
bool  rainOn = false;
float rainIntensity = 0.0f, targetRain = 0.0f;
float roadFlood     = 0.0f,  targetRoad  = 0.0f;
float riverLevel    = 0.05f, targetRiver = 0.05f;   // downstream reservoir
float cloudCover    = 0.10f, targetCloud = 0.10f;
float gateOpen      = 0.0f,  targetGate  = 0.0f;

float turbineFlow = 0.0f;
float powerOutput = 0.0f;
float lightIntensity = 0.0f;
float timeAcc = 0.0f;
float windX = 0.5f;

// Barrage state
float upstreamLevel   = 0.82f, targetUpstream = 0.82f;
float barrageGateOpen = 0.0f,  targetBarrageGate = 0.0f;
float barrageFlow     = 0.0f;
float barragePower    = 0.0f;
bool barrageOverloaded = true;
float barrageWheelAngles[BARRAGE_NGATES] = {0};
float barrageReleaseRate = 0.0f;

void setBarrageGate(bool open) {
    targetBarrageGate = open ? 1.0f : 0.0f;
    if (open) {
        barrageReleaseRate = fmaxf(0.0f, upstreamLevel - riverLevel) / 80.0f;
        barrageOverloaded = false;
    } else {
        barrageReleaseRate = 0.0f;
    }
}

// =====================================================================
//  ROAD / DRAIN DIMENSIONS
// =====================================================================
const float ROAD_GRAY = 0.22f;
const float KERB_W = 0.6f;
const float KERB_H = 0.22f;
const float DRAIN_W = 2.0f;
const float DRAIN_DEPTH = 0.75f;
const float DRAIN_WATER_HEAD = 1.0f;
const float DRAIN_BASE_WATER = 0.38f;
const float DRAIN_WARNING_LEVEL = 0.78f;
const float DRAIN_SPILL_LEVEL = (DRAIN_DEPTH + KERB_H) / DRAIN_WATER_HEAD;

float drainLevel = DRAIN_BASE_WATER, targetDrain = DRAIN_BASE_WATER;

float drainSurfaceY(float groundY, float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    return groundY - DRAIN_DEPTH + level * DRAIN_WATER_HEAD;
}

// =====================================================================
//  ROAD GRID
// =====================================================================
struct Road { float x1, x2, z1, z2; bool vertical; };
const Road roads[] = {
    { -CITY_HALF_WIDTH, CITY_HALF_WIDTH, -32.0f, -24.0f, false },
    { -CITY_HALF_WIDTH, CITY_HALF_WIDTH, -56.0f, -48.0f, false },
    { -CITY_HALF_WIDTH, CITY_HALF_WIDTH, -80.0f, -72.0f, false },
    { -CITY_HALF_WIDTH, CITY_HALF_WIDTH, -104.0f, -96.0f, false },
    { -CITY_HALF_WIDTH, CITY_HALF_WIDTH, -128.0f, -120.0f, false },
    { -CITY_HALF_WIDTH, CITY_HALF_WIDTH,  -8.0f,   0.0f, false },
    { -42.0f, -34.0f, -140.0f, 10.0f, true },
    {  -4.0f,   4.0f, -140.0f, 10.0f, true },
    {  34.0f,  42.0f, -140.0f, 10.0f, true },
    { -86.0f, -78.0f, -140.0f, 10.0f, true },
    {  78.0f,  86.0f, -140.0f, 10.0f, true },
};
const int NROADS = sizeof(roads)/sizeof(roads[0]);

// =====================================================================
//  GOLI
// =====================================================================
const float GOLI_Z1 = -21.5f;
const float GOLI_Z2 = -11.5f;
const float GOLI_HALF_W = 1.8f;
const float GOLI_KERB_W = 0.5f;
const float GOLI_DRAIN_W = 1.0f;
float goliCenterX(float z) {
    float t = (z - GOLI_Z1) / (GOLI_Z2 - GOLI_Z1);
    return -18.0f + sinf(t * M_PI) * 2.5f;
}

// =====================================================================
//  DRAINS
// =====================================================================
struct Drain { float x1, x2, z1, z2; bool vertical; float roadEdge; };
std::vector<Drain> drains;

void buildDrains() {
    drains.clear();
    auto addDrain = [&](float x1, float x2, float z1, float z2, bool vertical, float roadEdge) {
        if (x2 <= x1 || z2 <= z1) return;
        Drain d; d.x1=x1; d.x2=x2; d.z1=z1; d.z2=z2; d.vertical=vertical; d.roadEdge=roadEdge;
        drains.push_back(d);
    };
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        if (r.vertical) {
            auto addVerticalPair = [&](float z1, float z2) {
                addDrain(r.x1 - KERB_W - DRAIN_W, r.x1 - KERB_W, z1, z2, true, r.x1);
                addDrain(r.x2 + KERB_W, r.x2 + KERB_W + DRAIN_W, z1, z2, true, r.x2);
            };
            float segmentStart = r.z1;
            for (int j = 0; j < NROADS; j++) {
                Road c = roads[j];
                if (c.vertical || c.x1 > r.x2 || c.x2 < r.x1) continue;
                float cs = c.z1 - KERB_W;
                float ce = c.z2 + KERB_W;
                if (cs > segmentStart) addVerticalPair(segmentStart, fminf(cs, r.z2));
                segmentStart = fmaxf(segmentStart, ce);
            }
            addVerticalPair(segmentStart, r.z2);
        } else {
            auto addHorizontalPair = [&](float x1, float x2) {
                addDrain(x1, x2, r.z1 - KERB_W - DRAIN_W, r.z1 - KERB_W, false, r.z1);
                addDrain(x1, x2, r.z2 + KERB_W, r.z2 + KERB_W + DRAIN_W, false, r.z2);
            };
            float segmentStart = r.x1;
            for (int j = 0; j < NROADS; j++) {
                Road c = roads[j];
                if (!c.vertical || c.z1 > r.z2 || c.z2 < r.z1) continue;
                float cs = c.x1 - KERB_W;
                float ce = c.x2 + KERB_W;
                if (cs > segmentStart) addHorizontalPair(segmentStart, fminf(cs, r.x2));
                segmentStart = fmaxf(segmentStart, ce);
            }
            addHorizontalPair(segmentStart, r.x2);
        }
    }
}

// =====================================================================
//  BUILDINGS  /  TREES
// =====================================================================
struct Building { float x, z, w, d, h, r, g, b; int floors; int type; };
std::vector<Building> buildings;

void buildBuildings() {
    struct B { float x,z,w,d,h,r,g,b; int fl; int type; };
    B list[] = {
        {-20, -42, 8, 8, 12, 0.55f,0.52f,0.48f, 5, 3},
        {-10, -42, 6, 8, 10, 0.50f,0.50f,0.54f, 4, 1},
        { 10, -42, 8, 8, 14, 0.54f,0.50f,0.46f, 6, 1},
        { 24, -42, 8, 8, 11, 0.46f,0.48f,0.52f, 5, 1},
        {-27.5f, -16, 6, 8, 11, 0.52f,0.50f,0.54f, 5, 1},
        { -9,    -16, 4, 8,  7, 0.60f,0.55f,0.48f, 3, 0},
        { 15,    -16, 12, 8,  9, 0.40f,0.44f,0.52f, 3, 3},
        { 26,    -16,  6, 8, 14, 0.48f,0.50f,0.54f, 7, 1},
        {-25,      5, 8, 5, 10, 0.55f,0.52f,0.48f, 5, 1},
        { 10,      5, 6, 5,  8, 0.55f,0.54f,0.50f, 4, 0},
        { 25,      5, 8, 5, 12, 0.50f,0.50f,0.54f, 6, 3},
        {-25,    -66, 8, 8, 13, 0.50f,0.50f,0.54f, 6, 3},
        { -9,    -66, 6, 8,  9, 0.55f,0.52f,0.48f, 4, 1},
        { 14,    -66, 9, 8, 15, 0.48f,0.50f,0.54f, 7, 1},
        { 28,    -66, 7, 8, 11, 0.52f,0.48f,0.46f, 5, 1},
        {-25,    -90, 8, 8, 12, 0.55f,0.52f,0.48f, 5, 1},
        { -9,    -90, 7, 8, 14, 0.48f,0.50f,0.54f, 7, 1},
        { 14,    -90, 8, 8, 10, 0.52f,0.50f,0.46f, 4, 3},
        { 28,    -90, 7, 8, 13, 0.50f,0.50f,0.54f, 6, 1},
        {-25,   -114, 8, 8, 14, 0.48f,0.50f,0.54f, 7, 1},
        { -9,   -114, 6, 8, 10, 0.55f,0.52f,0.48f, 4, 1},
        { 14,   -114, 9, 8, 13, 0.50f,0.50f,0.54f, 6, 3},
        { 28,   -114, 7, 8, 11, 0.52f,0.48f,0.46f, 5, 1},
        {-25,   -134, 8, 8, 11, 0.52f,0.50f,0.48f, 5, 1},
        { -9,   -134, 7, 8, 13, 0.48f,0.50f,0.54f, 6, 1},
        { 14,   -134, 9, 8, 10, 0.52f,0.48f,0.46f, 4, 3},
        { 28,   -134, 7, 8, 12, 0.50f,0.50f,0.54f, 5, 1},
        {-102, -42, 10, 8, 12, 0.48f,0.50f,0.54f, 5, 1},
        { 102, -42, 10, 8, 14, 0.52f,0.50f,0.48f, 6, 3},
        {-102, -66, 10, 8, 15, 0.50f,0.48f,0.46f, 7, 1},
        { 102, -66, 10, 8, 11, 0.50f,0.50f,0.54f, 5, 1},
        {-102, -90, 10, 8, 12, 0.52f,0.50f,0.48f, 5, 1},
        { 102, -90, 10, 8, 16, 0.48f,0.50f,0.54f, 8, 3},
        {-102,-114, 10, 8, 13, 0.50f,0.50f,0.54f, 6, 1},
        { 102,-114, 10, 8, 10, 0.52f,0.48f,0.46f, 4, 1},
        {-102,-134, 10, 8, 14, 0.48f,0.50f,0.54f, 7, 1},
        { 102,-134, 10, 8, 12, 0.50f,0.48f,0.46f, 5, 3},
        {-50, -16, 8, 10, 12, 0.52f,0.50f,0.48f, 5, 1},
        {-50,   5, 8,  6,  8, 0.55f,0.52f,0.48f, 4, 1},
        { 50, -16, 8, 10, 12, 0.48f,0.50f,0.52f, 5, 1},
        { 50,   5, 8,  6,  8, 0.52f,0.50f,0.48f, 4, 1},
        {-62, -16, 10, 9, 15, 0.48f,0.50f,0.54f, 7, 3},
        { 68, -16,  9, 9, 12, 0.52f,0.48f,0.46f, 5, 1},
        {-68,   5,  9, 7, 10, 0.54f,0.52f,0.48f, 4, 1},
        { 62,   5, 10, 7, 14, 0.48f,0.50f,0.54f, 6, 3},
        {-62, -42, 10, 9, 13, 0.50f,0.48f,0.46f, 6, 1},
        { 62, -42, 10, 9, 16, 0.48f,0.50f,0.54f, 8, 3},
        {-62, -66, 10, 9, 11, 0.55f,0.52f,0.48f, 5, 1},
        { 62, -66, 10, 9, 14, 0.50f,0.50f,0.54f, 6, 1},
        {-62, -90, 10, 9, 16, 0.48f,0.50f,0.54f, 8, 3},
        { 62, -90, 10, 9, 12, 0.52f,0.48f,0.46f, 5, 1},
        {-62,-114, 10, 9, 13, 0.52f,0.50f,0.48f, 6, 1},
        { 62,-114, 10, 9, 15, 0.48f,0.50f,0.54f, 7, 3},
        {-62,-134, 10, 9, 11, 0.55f,0.52f,0.48f, 5, 1},
        { 62,-134, 10, 9, 14, 0.50f,0.48f,0.46f, 6, 1},
        {-108,-16, 10, 9, 16, 0.48f,0.50f,0.54f, 8, 3},
        { 108,-16, 10, 9, 12, 0.52f,0.48f,0.46f, 5, 1},
        {-108,  5, 10, 7, 11, 0.52f,0.50f,0.48f, 5, 1},
        { 108,  5, 10, 7, 15, 0.48f,0.50f,0.54f, 7, 3},
        {-108,-66, 10, 9, 12, 0.55f,0.52f,0.48f, 5, 1},
        { 108,-66, 10, 9, 15, 0.50f,0.50f,0.54f, 7, 1},
        {-108,-90, 10, 9, 14, 0.48f,0.50f,0.54f, 6, 3},
        { 108,-90, 10, 9, 11, 0.52f,0.48f,0.46f, 5, 1},
        {-108,-114, 10, 9, 16, 0.50f,0.48f,0.46f, 8, 1},
        { 108,-114, 10, 9, 13, 0.48f,0.50f,0.54f, 6, 3},
        {-108,-134, 10, 9, 12, 0.52f,0.50f,0.48f, 5, 1},
        { 108,-134, 10, 9, 16, 0.48f,0.50f,0.54f, 8, 1},
    };
    for (auto& b : list) {
        Building bb;
        bb.x=b.x; bb.z=b.z; bb.w=b.w; bb.d=b.d; bb.h=b.h;
        bb.r=b.r; bb.g=b.g; bb.b=b.b; bb.floors=b.fl; bb.type=b.type;
        buildings.push_back(bb);
    }
}

struct Tree { float x, z, h, r; };
std::vector<Tree> trees;

void buildTrees() {
    Tree list[] = {
        {-31, -19, 3.8f, 0.9f}, {-30, -12, 4.2f, 1.0f},
        { -8, -19, 3.5f, 0.8f}, { -8, -12, 3.8f, 0.9f},
        {  8, -19, 4.0f, 0.9f}, {  8, -12, 3.7f, 0.9f},
        { 30, -19, 3.8f, 0.85f}, { 30, -12, 4.0f, 0.95f},
        {-30, -42, 4.5f, 1.0f}, {-30, -38, 4.0f, 0.9f},
        {  0, -42, 4.2f, 1.0f}, {  0, -38, 4.5f, 1.0f},
        { 32, -42, 4.0f, 0.9f}, { 32, -38, 4.3f, 0.95f},
        {-30, -66, 4.2f, 0.95f}, {32, -66, 4.0f, 0.9f},
        {-30, -90, 4.0f, 0.9f}, {32, -90, 4.3f, 0.95f},
        {-30, -114, 4.2f, 0.95f}, {32, -114, 4.0f, 0.9f},
        {-30, -134, 4.0f, 0.9f}, {32, -134, 4.2f, 0.95f},
        {-38,   5, 4.0f, 0.9f}, {-38,   8, 3.8f, 0.85f},
        { 38,   5, 4.0f, 0.9f}, { 38,   8, 3.8f, 0.85f},
        {-50,  -2, 4.0f, 0.9f},
        { 50,  -2, 4.0f, 0.9f},
    };
    for (auto& t : list) trees.push_back(t);
}

// =====================================================================
//  RAIN
// =====================================================================
struct RainDrop { float x, y, z, speed; };
std::vector<RainDrop> rainDrops;
const float RAIN_X_MIN = -120.0f, RAIN_Z_MIN = -50.0f, RAIN_Y_MIN = 55.0f;

void initRain() {
    for (int i = 0; i < 6000; i++) {
        RainDrop r;
        r.x = RAIN_X_MIN + (rand()%24000)/100.0f;
        r.y = RAIN_Y_MIN + (rand()%10000)/100.0f;
        r.z = RAIN_Z_MIN + (rand()%21000)/100.0f;
        r.speed = 26.0f + (rand()%100)/6.0f;
        rainDrops.push_back(r);
    }
}

// =====================================================================
//  PRIMITIVES
// =====================================================================
void drawCube() {
    float s = 0.5f;
    glBegin(GL_QUADS);
    glNormal3f(1,0,0);  glVertex3f( s,-s,-s); glVertex3f( s, s,-s); glVertex3f( s, s, s); glVertex3f( s,-s, s);
    glNormal3f(-1,0,0); glVertex3f(-s,-s, s); glVertex3f(-s, s, s); glVertex3f(-s, s,-s); glVertex3f(-s,-s,-s);
    glNormal3f(0,1,0);  glVertex3f(-s, s,-s); glVertex3f(-s, s, s); glVertex3f( s, s, s); glVertex3f( s, s,-s);
    glNormal3f(0,-1,0); glVertex3f(-s,-s, s); glVertex3f(-s,-s,-s); glVertex3f( s,-s,-s); glVertex3f( s,-s, s);
    glNormal3f(0,0,1);  glVertex3f(-s,-s, s); glVertex3f( s,-s, s); glVertex3f( s, s, s); glVertex3f(-s, s, s);
    glNormal3f(0,0,-1); glVertex3f( s,-s,-s); glVertex3f(-s,-s,-s); glVertex3f(-s, s,-s); glVertex3f( s, s,-s);
    glEnd();
}
void sphere(float r) {
    GLUquadric* q = gluNewQuadric(); gluSphere(q, r, 14, 12); gluDeleteQuadric(q);
}
void cylinder(float r, float h) {
    GLUquadric* q = gluNewQuadric(); gluCylinder(q, r, r, h, 16, 4); gluDeleteQuadric(q);
}
void disc(float r) {
    GLUquadric* q = gluNewQuadric(); gluDisk(q, 0, r, 16, 1); gluDeleteQuadric(q);
}
void quadY(float x1,float z1,float x2,float z2,float y) {
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);
    glVertex3f(x1,y,z1); glVertex3f(x2,y,z1); glVertex3f(x2,y,z2); glVertex3f(x1,y,z2);
    glEnd();
}
float ripple(float x, float z, float t, float amp, float freq, float speed) {
    float a = sinf(z * freq - t * speed);
    float b = sinf((x + z) * freq * 0.55f - t * speed * 0.65f);
    float c = sinf(x * freq * 1.7f + t * speed * 1.3f);
    return amp * (a * 0.62f + b * 0.25f + c * 0.13f);
}
void waterColorAt(float t, float& r, float& g, float& b) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    r = DEEP_BLUE_R * (1 - t) + SKY_BLUE_R * t;
    g = DEEP_BLUE_G * (1 - t) + SKY_BLUE_G * t;
    b = DEEP_BLUE_B * (1 - t) + SKY_BLUE_B * t;
}

static bool loadFloodShaderFunctions() {
#define LOAD_GL_FUNCTION(variable, symbol, type) \
    variable = reinterpret_cast<type>(glutGetProcAddress(symbol))
    LOAD_GL_FUNCTION(pCreateShader, "glCreateShader", CreateShaderProc);
    LOAD_GL_FUNCTION(pShaderSource, "glShaderSource", ShaderSourceProc);
    LOAD_GL_FUNCTION(pCompileShader, "glCompileShader", CompileShaderProc);
    LOAD_GL_FUNCTION(pGetShaderiv, "glGetShaderiv", GetShaderivProc);
    LOAD_GL_FUNCTION(pGetShaderInfoLog, "glGetShaderInfoLog", GetShaderInfoLogProc);
    LOAD_GL_FUNCTION(pCreateProgram, "glCreateProgram", CreateProgramProc);
    LOAD_GL_FUNCTION(pAttachShader, "glAttachShader", AttachShaderProc);
    LOAD_GL_FUNCTION(pLinkProgram, "glLinkProgram", LinkProgramProc);
    LOAD_GL_FUNCTION(pGetProgramiv, "glGetProgramiv", GetProgramivProc);
    LOAD_GL_FUNCTION(pGetProgramInfoLog, "glGetProgramInfoLog", GetProgramInfoLogProc);
    LOAD_GL_FUNCTION(pUseProgram, "glUseProgram", UseProgramProc);
    LOAD_GL_FUNCTION(pGetUniformLocation, "glGetUniformLocation", GetUniformLocationProc);
    LOAD_GL_FUNCTION(pUniform1i, "glUniform1i", Uniform1iProc);
    LOAD_GL_FUNCTION(pUniform1f, "glUniform1f", Uniform1fProc);
    LOAD_GL_FUNCTION(pUniform3f, "glUniform3f", Uniform3fProc);
    LOAD_GL_FUNCTION(pDeleteShader, "glDeleteShader", DeleteShaderProc);
    LOAD_GL_FUNCTION(pDeleteProgram, "glDeleteProgram", DeleteProgramProc);
#undef LOAD_GL_FUNCTION
    return pCreateShader && pShaderSource && pCompileShader && pGetShaderiv &&
           pGetShaderInfoLog && pCreateProgram && pAttachShader && pLinkProgram &&
           pGetProgramiv && pGetProgramInfoLog && pUseProgram &&
           pGetUniformLocation && pUniform1i && pUniform1f && pUniform3f &&
           pDeleteShader && pDeleteProgram;
}

static GLuint compileComparisonShader(GLenum type, const char* source) {
    GLuint shader = pCreateShader(type);
    pShaderSource(shader, 1, &source, 0);
    pCompileShader(shader);
    GLint compiled = GL_FALSE;
    pGetShaderiv(shader, FLOOD_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        char log[2048] = {0};
        pGetShaderInfoLog(shader, sizeof(log), 0, log);
        fprintf(stderr, "Comparison shader compilation failed: %s\n", log);
        pDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool initComparisonShader() {
    if (!loadFloodShaderFunctions()) {
        fprintf(stderr, "OpenGL shader functions unavailable; Gouraud/Phong wheel comparison is disabled.\n");
        return false;
    }

    const char* vertexSource =
        "#version 120\n"
        "uniform int uPhong;\n"
        "uniform float uShininess;\n"
        "uniform vec3 uLightPosition;\n"
        "uniform vec3 uAmbient;\n"
        "uniform vec3 uDiffuse;\n"
        "uniform vec3 uSpecular;\n"
        "varying vec3 vPosition;\n"
        "varying vec3 vNormal;\n"
        "varying vec3 vGouraud;\n"
        "varying vec4 vBaseColor;\n"
        "vec3 illuminate(vec3 p, vec3 n, vec3 base) {\n"
        "  vec3 N = normalize(n);\n"
        "  vec3 L = normalize(uLightPosition - p);\n"
        "  vec3 V = normalize(-p);\n"
        "  float diffuseTerm = max(dot(N, L), 0.0);\n"
        "  float specularTerm = diffuseTerm > 0.0 ? pow(max(dot(reflect(-L, N), V), 0.0), uShininess) : 0.0;\n"
        "  return uAmbient * base + uDiffuse * base * diffuseTerm +\n"
        "         uSpecular * vec3(0.5) * specularTerm;\n"
        "}\n"
        "void main() {\n"
        "  vec4 eyePosition = gl_ModelViewMatrix * gl_Vertex;\n"
        "  vPosition = eyePosition.xyz;\n"
        "  vNormal = gl_NormalMatrix * gl_Normal;\n"
        "  vBaseColor = gl_Color;\n"
        "  vGouraud = illuminate(vPosition, vNormal, gl_Color.rgb);\n"
        "  gl_Position = ftransform();\n"
        "}\n";
    const char* fragmentSource =
        "#version 120\n"
        "uniform int uPhong;\n"
        "uniform float uShininess;\n"
        "uniform vec3 uLightPosition;\n"
        "uniform vec3 uAmbient;\n"
        "uniform vec3 uDiffuse;\n"
        "uniform vec3 uSpecular;\n"
        "varying vec3 vPosition;\n"
        "varying vec3 vNormal;\n"
        "varying vec3 vGouraud;\n"
        "varying vec4 vBaseColor;\n"
        "void main() {\n"
        "  vec3 lit = vGouraud;\n"
        "  if (uPhong != 0) {\n"
        "    vec3 N = normalize(vNormal);\n"
        "    vec3 L = normalize(uLightPosition - vPosition);\n"
        "    vec3 V = normalize(-vPosition);\n"
        "    float diffuseTerm = max(dot(N, L), 0.0);\n"
        "    float specularTerm = diffuseTerm > 0.0 ? pow(max(dot(reflect(-L, N), V), 0.0), uShininess) : 0.0;\n"
        "    lit = uAmbient * vBaseColor.rgb + uDiffuse * vBaseColor.rgb * diffuseTerm +\n"
        "          uSpecular * vec3(0.5) * specularTerm;\n"
        "  }\n"
        "  gl_FragColor = vec4(lit, vBaseColor.a);\n"
        "}\n";

    GLuint vertexShader = compileComparisonShader(FLOOD_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileComparisonShader(FLOOD_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) pDeleteShader(vertexShader);
        if (fragmentShader) pDeleteShader(fragmentShader);
        return false;
    }

    comparisonShaderProgram = pCreateProgram();
    pAttachShader(comparisonShaderProgram, vertexShader);
    pAttachShader(comparisonShaderProgram, fragmentShader);
    pLinkProgram(comparisonShaderProgram);
    pDeleteShader(vertexShader);
    pDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    pGetProgramiv(comparisonShaderProgram, FLOOD_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char log[2048] = {0};
        pGetProgramInfoLog(comparisonShaderProgram, sizeof(log), 0, log);
        fprintf(stderr, "Comparison shader link failed: %s\n", log);
        pDeleteProgram(comparisonShaderProgram);
        comparisonShaderProgram = 0;
        return false;
    }
    comparisonShadingModeLocation = pGetUniformLocation(comparisonShaderProgram, "uPhong");
    comparisonShininessLocation = pGetUniformLocation(comparisonShaderProgram, "uShininess");
    comparisonLightPositionLocation = pGetUniformLocation(comparisonShaderProgram, "uLightPosition");
    comparisonAmbientLocation = pGetUniformLocation(comparisonShaderProgram, "uAmbient");
    comparisonDiffuseLocation = pGetUniformLocation(comparisonShaderProgram, "uDiffuse");
    comparisonSpecularLocation = pGetUniformLocation(comparisonShaderProgram, "uSpecular");
    return true;
}

static void beginComparisonShading(bool usePhong) {
    if (!comparisonShaderProgram) return;
    pUseProgram(comparisonShaderProgram);
    pUniform1i(comparisonShadingModeLocation, usePhong ? 1 : 0);
    pUniform1f(comparisonShininessLocation, 48.0f);
    pUniform3f(comparisonLightPositionLocation, comparisonLightPositionEye[0],
               comparisonLightPositionEye[1], comparisonLightPositionEye[2]);
    pUniform3f(comparisonAmbientLocation, comparisonAmbientColor[0],
               comparisonAmbientColor[1], comparisonAmbientColor[2]);
    pUniform3f(comparisonDiffuseLocation, comparisonDiffuseColor[0],
               comparisonDiffuseColor[1], comparisonDiffuseColor[2]);
    pUniform3f(comparisonSpecularLocation, comparisonSpecularColor[0],
               comparisonSpecularColor[1], comparisonSpecularColor[2]);
}

static void endComparisonShading() {
    if (comparisonShaderProgram) pUseProgram(0);
}

static void floodSurfaceNormal(float x, float z, float offset, float amp,
                               float freq, float speed, float& nx, float& ny, float& nz) {
    const float e = 0.08f;
    float yLeft = cityYatZ(z) + offset + ripple(x - e, z, timeAcc, amp, freq, speed);
    float yRight = cityYatZ(z) + offset + ripple(x + e, z, timeAcc, amp, freq, speed);
    float yNear = cityYatZ(z - e) + offset + ripple(x, z - e, timeAcc, amp, freq, speed);
    float yFar = cityYatZ(z + e) + offset + ripple(x, z + e, timeAcc, amp, freq, speed);
    nx = -(yRight - yLeft) / (2.0f * e);
    ny = 1.0f;
    nz = -(yFar - yNear) / (2.0f * e);
    float length = sqrtf(nx*nx + ny*ny + nz*nz);
    nx /= length;
    ny /= length;
    nz /= length;
}

// =====================================================================
//  GENERIC SOLID PIPE
// =====================================================================
void drawSolidPipe(float x0, float y0, float z0,
                   float x1, float y1, float z1,
                   float r0, float r1, int sides, bool caps)
{
    float dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
    float len = sqrtf(dx*dx + dy*dy + dz*dz);
    if (len < 1e-5f) return;
    float ux = dx/len, uy = dy/len, uz = dz/len;
    float ax, ay, az;
    if (fabsf(uy) < 0.9f) {
        ax = -uz; ay = 0; az = ux;
        float l = sqrtf(ax*ax + az*az); ax /= l; az /= l;
    } else {
        ax = 0; ay = -uz; az = uy;
        float l = sqrtf(ay*ay + az*az); ay /= l; az /= l;
    }
    float bx = uy*az - uz*ay;
    float by = uz*ax - ux*az;
    float bz = ux*ay - uy*ax;
    glBegin(GL_QUAD_STRIP);
    for (int s = 0; s <= sides; s++) {
        float a = s * 2.0f * M_PI / sides;
        float ca = cosf(a), sa = sinf(a);
        float nx = ax*ca + bx*sa;
        float ny = ay*ca + by*sa;
        float nz = az*ca + bz*sa;
        glNormal3f(nx, ny, nz);
        glVertex3f(x0 + nx*r0, y0 + ny*r0, z0 + nz*r0);
        glVertex3f(x1 + nx*r1, y1 + ny*r1, z1 + nz*r1);
    }
    glEnd();
    if (caps) {
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(-ux, -uy, -uz);
        glVertex3f(x0, y0, z0);
        for (int s = 0; s <= sides; s++) {
            float a = -s * 2.0f * M_PI / sides;
            float ca = cosf(a), sa = sinf(a);
            float nx = ax*ca + bx*sa, ny = ay*ca + by*sa, nz = az*ca + bz*sa;
            glVertex3f(x0 + nx*r0, y0 + ny*r0, z0 + nz*r0);
        }
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(ux, uy, uz);
        glVertex3f(x1, y1, z1);
        for (int s = 0; s <= sides; s++) {
            float a = s * 2.0f * M_PI / sides;
            float ca = cosf(a), sa = sinf(a);
            float nx = ax*ca + bx*sa, ny = ay*ca + by*sa, nz = az*ca + bz*sa;
            glVertex3f(x1 + nx*r1, y1 + ny*r1, z1 + nz*r1);
        }
        glEnd();
    }
}

// =====================================================================
//  STENCIL
// =====================================================================
void stampGoliFootprint() {
    const int SEG = 60;
    for (int i = 0; i < SEG; i++) {
        float za = GOLI_Z1 + (GOLI_Z2 - GOLI_Z1) * i / SEG;
        float zb = GOLI_Z1 + (GOLI_Z2 - GOLI_Z1) * (i + 1) / SEG;
        float xa = goliCenterX(za);
        float xb = goliCenterX(zb);
        float dx = xb - xa, dz = zb - za;
        float len = sqrtf(dx*dx + dz*dz);
        if (len < 1e-5f) continue;
        float nx = -dz / len, nz = dx / len;
        for (int side = -1; side <= 1; side += 2) {
            float inOff  = GOLI_HALF_W + GOLI_KERB_W;
            float outOff = GOLI_HALF_W + GOLI_KERB_W + GOLI_DRAIN_W;
            glBegin(GL_QUADS);
            glVertex3f(xa + nx * inOff  * side, 0, za + nz * inOff  * side);
            glVertex3f(xa + nx * outOff * side, 0, za + nz * outOff * side);
            glVertex3f(xb + nx * outOff * side, 0, zb + nz * outOff * side);
            glVertex3f(xb + nx * inOff  * side, 0, zb + nz * inOff  * side);
            glEnd();
        }
    }
}

void markDrainOpeningsInStencil() {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    for (auto& d : drains) {
        glBegin(GL_QUADS);
        glVertex3f(d.x1, 0, d.z1);
        glVertex3f(d.x2, 0, d.z1);
        glVertex3f(d.x2, 0, d.z2);
        glVertex3f(d.x1, 0, d.z2);
        glEnd();
    }
    stampGoliFootprint();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}

// =====================================================================
//  WATER PLANE (with per-cell churn parameter)
// =====================================================================
void drawWaterPlane(float x1, float z1, float x2, float z2, float yTop,
                    float alpha, float ripAmp, float ripFreq, float ripSpeed,
                    bool mixByHeight, bool mixByFresnel, float skyBlueAmount)
{
    const int NX = 32, NZ = 32;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLfloat wspec[] = {0.82f, 0.90f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, wspec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 72.0f);
    for (int i = 0; i < NX; i++) {
        for (int j = 0; j < NZ; j++) {
            float xa = x1 + (x2 - x1) * i / NX;
            float xb = x1 + (x2 - x1) * (i + 1) / NX;
            float za = z1 + (z2 - z1) * j / NZ;
            float zb = z1 + (z2 - z1) * (j + 1) / NZ;
            float ya = yTop + ripple(xa, za, timeAcc, ripAmp, ripFreq, ripSpeed);
            float yb = yTop + ripple(xb, za, timeAcc, ripAmp, ripFreq, ripSpeed);
            float yc = yTop + ripple(xb, zb, timeAcc, ripAmp, ripFreq, ripSpeed);
            float yd = yTop + ripple(xa, zb, timeAcc, ripAmp, ripFreq, ripSpeed);
            float nx = (ya - yb) + (yd - yc);
            float nz = (ya - yd) + (yb - yc);
            float ny = (xb - xa) * (zb - za) * 0.5f;
            float len = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-5f;
            nx /= len; ny /= len; nz /= len;
            float avg = (ya + yb + yc + yd) * 0.25f;
            float tH = (avg - yTop) / (ripAmp + 1e-5f) * 0.5f + 0.5f;
            float tF = 1.0f - ny; if (tF < 0) tF = 0;
            float t = 0.0f;
            if (mixByHeight)  t += tH * 0.7f;
            if (mixByFresnel) t += tF * 0.5f;
            t *= skyBlueAmount;
            if (t < 0) t = 0; if (t > 1) t = 1;
            float r, g, b; waterColorAt(t, r, g, b);
            glColor4f(r, g, b, alpha);
            glBegin(GL_QUADS);
            glNormal3f(nx, ny, nz);
            glVertex3f(xa, ya, za); glVertex3f(xb, yb, za);
            glVertex3f(xb, yc, zb); glVertex3f(xa, yd, zb);
            glEnd();
            if (mixByHeight && tH > 0.82f) {
                glColor4f(0.92f, 0.98f, 1.0f, 0.18f * alpha);
                glBegin(GL_QUADS);
                glNormal3f(nx, ny, nz);
                glVertex3f(xa, ya + 0.018f, za);
                glVertex3f(xb, yb + 0.018f, za);
                glVertex3f(xb, yc + 0.018f, zb);
                glVertex3f(xa, yd + 0.018f, zb);
                glEnd();
            }
        }
    }
    GLfloat dspec[] = {0.20f, 0.20f, 0.20f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, dspec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 16.0f);
    glDisable(GL_BLEND);
}

// =====================================================================
//  SKY
// =====================================================================
float darkness() { return cloudCover * 0.85f; }

void drawSkyDome() {
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    float c = cloudCover;
    float topR = 0.28f*(1-c) + 0.08f*c;
    float topG = 0.55f*(1-c) + 0.10f*c;
    float topB = 0.90f*(1-c) + 0.15f*c;
    float botR = 0.78f*(1-c) + 0.18f*c;
    float botG = 0.85f*(1-c) + 0.20f*c;
    float botB = 0.95f*(1-c) + 0.25f*c;
    glBegin(GL_QUADS);
    glColor3f(botR, botG, botB); glVertex2f(-1,-1); glVertex2f(1,-1);
    glColor3f(topR, topG, topB); glVertex2f(1, 1); glVertex2f(-1, 1);
    glEnd();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST); setSceneLightingEnabled();
}

void drawClouds() {
    if (cloudCover < 0.05f) return;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    float shade = 0.85f - cloudCover * 0.60f;
    glColor4f(shade, shade, shade*1.05f, 0.55f + cloudCover*0.4f);
    const int N = 40;
    for (int i = 0; i < N; i++) {
        float bx = -120.0f + (i * 6.0f);
        float bz = -90.0f + fmodf(i * 15.7f, 180.0f);
        float by = 118.0f + fmodf(i * 8.3f, 26.0f);
        float drift = fmodf(timeAcc * 1.5f + i*3.0f, 40.0f);
        glPushMatrix();
        glTranslatef(bx + drift, by, bz);
        glScalef(14.0f + (i%3)*4.0f, 3.0f, 12.0f);
        drawCube();
        glPopMatrix();
    }
    glDisable(GL_BLEND); setSceneLightingEnabled();
}

// =====================================================================
//  GROUND / GRASS / CITY FLOOD
// =====================================================================
void drawGround() {
    float c = cloudCover;
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    const int SEG = 40;
    float zmin = CITY_BACK_Z - 5.0f;
    float zmax = CITY_FRONT_Z;
    for (int i = 0; i < SEG; i++) {
        float z0 = zmin + (zmax - zmin) * i / SEG;
        float z1 = zmin + (zmax - zmin) * (i + 1) / SEG;
        float y0 = cityYatZ(z0);
        float y1 = cityYatZ(z1);
        glColor3f(0.30f - c*0.14f, 0.32f - c*0.15f, 0.28f - c*0.12f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0.25f);
        glVertex3f(-CITY_HALF_WIDTH, y0, z0); glVertex3f(CITY_HALF_WIDTH, y0, z0);
        glVertex3f(CITY_HALF_WIDTH, y1, z1); glVertex3f(-CITY_HALF_WIDTH, y1, z1);
        glEnd();
    }
    glDisable(GL_STENCIL_TEST);
}

void drawGrassPatches() {
    float c = cloudCover;
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    struct G { float x1, x2, z1, z2; };
    G patches[] = {
        { -31.0f, -7.0f, -21.0f, -11.0f },
        {   7.0f, 31.0f, -21.0f, -11.0f },
        { -CITY_HALF_WIDTH, CITY_HALF_WIDTH, -50.0f, -35.0f },
        { -CITY_HALF_WIDTH, CITY_HALF_WIDTH,   3.0f,  CITY_FRONT_Z },
        { -CITY_HALF_WIDTH,-90.0f, CITY_BACK_Z, CITY_FRONT_Z },
        {  90.0f, CITY_HALF_WIDTH, CITY_BACK_Z, CITY_FRONT_Z },
    };
    for (auto& p : patches) {
        glColor3f(0.20f - c*0.08f, 0.42f - c*0.16f, 0.15f - c*0.06f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0.25f);
        glVertex3f(p.x1, cityYatZ(p.z1) + 0.008f, p.z1);
        glVertex3f(p.x2, cityYatZ(p.z1) + 0.008f, p.z1);
        glVertex3f(p.x2, cityYatZ(p.z2) + 0.008f, p.z2);
        glVertex3f(p.x1, cityYatZ(p.z2) + 0.008f, p.z2);
        glEnd();
    }
    glDisable(GL_STENCIL_TEST);
}

void drawCityFlood() {
    if (roadFlood <= 0.01f) return;
    const float xMin = -CITY_HALF_WIDTH, xMax = CITY_HALF_WIDTH;
    const float zMin = CITY_BACK_Z - 5.0f, zMax = CITY_FRONT_Z;
    const int NX = 120, NZ = 38;
    const float spread = 2.0f + roadFlood * 30.0f;
    const float fadeWidth = 5.0f;
    auto floodCoverage = [&](float x, float z) {
        float nearest = 1000.0f;
        for (int i = 0; i < NROADS; i++) {
            const Road& road = roads[i];
            float dx = x < road.x1 ? road.x1 - x : (x > road.x2 ? x - road.x2 : 0.0f);
            float dz = z < road.z1 ? road.z1 - z : (z > road.z2 ? z - road.z2 : 0.0f);
            nearest = fminf(nearest, sqrtf(dx * dx + dz * dz));
        }
        float t = (spread - nearest) / fadeWidth;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    };
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (int ix = 0; ix < NX; ix++) {
        float xa = xMin + (xMax - xMin) * ix / NX;
        float xb = xMin + (xMax - xMin) * (ix + 1) / NX;
        for (int iz = 0; iz < NZ; iz++) {
            float za = zMin + (zMax - zMin) * iz / NZ;
            float zb = zMin + (zMax - zMin) * (iz + 1) / NZ;
            float aS = 0.30f + roadFlood * 0.35f;
            float y00 = cityYatZ(za) + 0.045f + ripple(xa, za, timeAcc, 0.025f, 1.1f, 2.0f);
            float y10 = cityYatZ(za) + 0.045f + ripple(xb, za, timeAcc, 0.025f, 1.1f, 2.0f);
            float y11 = cityYatZ(zb) + 0.045f + ripple(xb, zb, timeAcc, 0.025f, 1.1f, 2.0f);
            float y01 = cityYatZ(zb) + 0.045f + ripple(xa, zb, timeAcc, 0.025f, 1.1f, 2.0f);
            float nx, ny, nz;
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xa, za) * aS);
            floodSurfaceNormal(xa, za, 0.045f, 0.025f, 1.1f, 2.0f, nx, ny, nz);
            glNormal3f(nx, ny, nz); glVertex3f(xa, y00, za);
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xb, za) * aS);
            floodSurfaceNormal(xb, za, 0.045f, 0.025f, 1.1f, 2.0f, nx, ny, nz);
            glNormal3f(nx, ny, nz); glVertex3f(xb, y10, za);
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xb, zb) * aS);
            floodSurfaceNormal(xb, zb, 0.045f, 0.025f, 1.1f, 2.0f, nx, ny, nz);
            glNormal3f(nx, ny, nz); glVertex3f(xb, y11, zb);
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xa, zb) * aS);
            floodSurfaceNormal(xa, zb, 0.045f, 0.025f, 1.1f, 2.0f, nx, ny, nz);
            glNormal3f(nx, ny, nz); glVertex3f(xa, y01, zb);
        }
    }
    glEnd();
    glDisable(GL_BLEND);
}

// =====================================================================
//  ROADS
// =====================================================================
void drawRoads() {
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        const int SEG = 24;
        if (r.vertical) {
            for (int s = 0; s < SEG; s++) {
                float za = r.z1 + (r.z2 - r.z1) * s / SEG;
                float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / SEG;
                float ya = cityYatZ(za), yb = cityYatZ(zb);
                glColor3f(ROAD_GRAY, ROAD_GRAY, ROAD_GRAY);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(r.x1, ya, za); glVertex3f(r.x2, ya, za);
                glVertex3f(r.x2, yb, zb); glVertex3f(r.x1, yb, zb);
                glEnd();
            }
            glColor3f(0.85f, 0.75f, 0.15f);
            float cx = (r.x1 + r.x2) * 0.5f;
            for (float z = r.z1 + 1.0f; z < r.z2 - 1.0f; z += 4.0f) {
                float ya = cityYatZ(z) + 0.01f;
                float yb = cityYatZ(z + 2.5f) + 0.01f;
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(cx - 0.14f, ya, z); glVertex3f(cx + 0.14f, ya, z);
                glVertex3f(cx + 0.14f, yb, z + 2.5f); glVertex3f(cx - 0.14f, yb, z + 2.5f);
                glEnd();
            }
            glColor3f(0.76f, 0.76f, 0.72f);
            for (int side = 0; side < 2; side++) {
                float x = (side == 0) ? r.x1 + 0.35f : r.x2 - 0.45f;
                for (int s = 0; s < SEG; s++) {
                    float za = r.z1 + (r.z2 - r.z1) * s / SEG;
                    float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / SEG;
                    float ya = cityYatZ(za) + 0.012f;
                    float yb = cityYatZ(zb) + 0.012f;
                    glBegin(GL_QUADS);
                    glNormal3f(0, 1, 0.25f);
                    glVertex3f(x, ya, za); glVertex3f(x + 0.10f, ya, za);
                    glVertex3f(x + 0.10f, yb, zb); glVertex3f(x, yb, zb);
                    glEnd();
                }
            }
        } else {
            const int RS = 16;
            glColor3f(ROAD_GRAY, ROAD_GRAY, ROAD_GRAY);
            for (int s = 0; s < RS; s++) {
                float za = r.z1 + (r.z2 - r.z1) * s / RS;
                float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / RS;
                float ya = cityYatZ(za), yb = cityYatZ(zb);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(r.x1, ya, za); glVertex3f(r.x2, ya, za);
                glVertex3f(r.x2, yb, zb); glVertex3f(r.x1, yb, zb);
                glEnd();
            }
            glColor3f(0.85f, 0.75f, 0.15f);
            float cz = (r.z1 + r.z2) * 0.5f;
            for (float x = r.x1 + 1.0f; x < r.x2 - 1.0f; x += 4.0f)
                quadY(x, cz - 0.14f, x + 2.5f, cz + 0.14f, cityYatZ(cz) + 0.01f);
            glColor3f(0.76f, 0.76f, 0.72f);
            quadY(r.x1, r.z1 + 0.35f, r.x2, r.z1 + 0.45f, cityYatZ(r.z1 + 0.4f) + 0.012f);
            quadY(r.x1, r.z2 - 0.45f, r.x2, r.z2 - 0.35f, cityYatZ(r.z2 - 0.4f) + 0.012f);
        }
    }
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        if (!r.vertical) continue;
        const int SEG = 24;
        for (int s = 0; s < SEG; s++) {
            float za = r.z1 + (r.z2 - r.z1) * s / SEG;
            float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / SEG;
            bool atIntersection = false;
            for (int j = 0; j < NROADS; j++) {
                const Road& c = roads[j];
                if (!c.vertical &&
                    c.x1 <= r.x2 && c.x2 >= r.x1 &&
                    zb > c.z1 - KERB_W && za < c.z2 + KERB_W) {
                    atIntersection = true; break;
                }
            }
            if (atIntersection) continue;
            float ya = cityYatZ(za) + KERB_H, yb = cityYatZ(zb) + KERB_H;
            glColor3f(0.72f, 0.72f, 0.70f);
            glBegin(GL_QUADS);
            glNormal3f(0,1,0.25f);
            glVertex3f(r.x1 - KERB_W, ya, za); glVertex3f(r.x1, ya, za);
            glVertex3f(r.x1, yb, zb); glVertex3f(r.x1 - KERB_W, yb, zb);
            glEnd();
            glColor3f(0.10f, 0.10f, 0.12f);
            glBegin(GL_QUADS);
            glNormal3f(-1, 0.3f, 0);
            glVertex3f(r.x1 - KERB_W, ya, za);
            glVertex3f(r.x1 - KERB_W, yb, zb);
            glVertex3f(r.x1 - KERB_W, yb - KERB_H - DRAIN_DEPTH, zb);
            glVertex3f(r.x1 - KERB_W, ya - KERB_H - DRAIN_DEPTH, za);
            glEnd();
            glColor3f(0.72f, 0.72f, 0.70f);
            glBegin(GL_QUADS);
            glNormal3f(0,1,0.25f);
            glVertex3f(r.x2, ya, za); glVertex3f(r.x2 + KERB_W, ya, za);
            glVertex3f(r.x2 + KERB_W, yb, zb); glVertex3f(r.x2, yb, zb);
            glEnd();
            glColor3f(0.10f, 0.10f, 0.12f);
            glBegin(GL_QUADS);
            glNormal3f(1, 0.3f, 0);
            glVertex3f(r.x2 + KERB_W, ya, za);
            glVertex3f(r.x2 + KERB_W, yb, zb);
            glVertex3f(r.x2 + KERB_W, yb - KERB_H - DRAIN_DEPTH, zb);
            glVertex3f(r.x2 + KERB_W, ya - KERB_H - DRAIN_DEPTH, za);
            glEnd();
        }
    }
}

// =====================================================================
//  DRAIN CHANNELS
// =====================================================================
void drawDrainChannel(const Drain& d) {
    if (d.vertical) {
        const int SEG = 24;
        for (int s = 0; s < SEG; s++) {
            float za = d.z1 + (d.z2 - d.z1) * s / SEG;
            float zb = d.z1 + (d.z2 - d.z1) * (s + 1) / SEG;
            float ytA = cityYatZ(za), ytB = cityYatZ(zb);
            float ybA = ytA - DRAIN_DEPTH, ybB = ytB - DRAIN_DEPTH;
            bool roadOnRight = (d.roadEdge > (d.x1 + d.x2) * 0.5f);
            glColor3f(0.08f, 0.08f, 0.10f);
            glBegin(GL_QUADS);
            if (roadOnRight) {
                glNormal3f(-1, 0.3f, 0);
                glVertex3f(d.x2, ytA, za); glVertex3f(d.x2, ytB, zb);
                glVertex3f(d.x2, ybB, zb); glVertex3f(d.x2, ybA, za);
            } else {
                glNormal3f(1, 0.3f, 0);
                glVertex3f(d.x1, ytA, za); glVertex3f(d.x1, ytB, zb);
                glVertex3f(d.x1, ybB, zb); glVertex3f(d.x1, ybA, za);
            }
            glEnd();
            glColor3f(0.05f, 0.05f, 0.07f);
            glBegin(GL_QUADS);
            if (roadOnRight) {
                glNormal3f(1, 0.3f, 0);
                glVertex3f(d.x1, ytA, za); glVertex3f(d.x1, ytB, zb);
                glVertex3f(d.x1, ybB, zb); glVertex3f(d.x1, ybA, za);
            } else {
                glNormal3f(-1, 0.3f, 0);
                glVertex3f(d.x2, ytA, za); glVertex3f(d.x2, ytB, zb);
                glVertex3f(d.x2, ybB, zb); glVertex3f(d.x2, ybA, za);
            }
            glEnd();
            glColor3f(0.03f, 0.03f, 0.05f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0.25f);
            glVertex3f(d.x1, ybA, za); glVertex3f(d.x2, ybA, za);
            glVertex3f(d.x2, ybB, zb); glVertex3f(d.x1, ybB, zb);
            glEnd();
            if (s % 3 == 0) {
                float z = (za + zb) * 0.5f;
                float y = cityYatZ(z) + 0.02f;
                glColor3f(0.15f, 0.15f, 0.17f);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0);
                glVertex3f(d.x1, y, z - 0.035f); glVertex3f(d.x2, y, z - 0.035f);
                glVertex3f(d.x2, y, z + 0.035f); glVertex3f(d.x1, y, z + 0.035f);
                glEnd();
            }
        }
    } else {
        float yTop = cityYatZ(d.roadEdge);
        float yBot = yTop - DRAIN_DEPTH;
        glColor3f(0.08f, 0.08f, 0.10f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0.3f, 1);
        glVertex3f(d.x1, yTop, d.z1); glVertex3f(d.x2, yTop, d.z1);
        glVertex3f(d.x2, yBot, d.z1); glVertex3f(d.x1, yBot, d.z1);
        glEnd();
        glColor3f(0.05f, 0.05f, 0.07f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0.3f, -1);
        glVertex3f(d.x1, yTop, d.z2); glVertex3f(d.x2, yTop, d.z2);
        glVertex3f(d.x2, yBot, d.z2); glVertex3f(d.x1, yBot, d.z2);
        glEnd();
        glColor3f(0.03f, 0.03f, 0.05f);
        quadY(d.x1, d.z1, d.x2, d.z2, yBot);
        glColor3f(0.15f, 0.15f, 0.17f);
        for (float x = d.x1 + 0.7f; x < d.x2; x += 1.4f) {
            float y = cityYatZ(d.roadEdge) + 0.02f;
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(x - 0.035f, y, d.z1); glVertex3f(x + 0.035f, y, d.z1);
            glVertex3f(x + 0.035f, y, d.z2); glVertex3f(x - 0.035f, y, d.z2);
            glEnd();
        }
    }
}
void drawDrains() { for (auto& d : drains) drawDrainChannel(d); }

// =====================================================================
//  DRAIN WATER
// =====================================================================
void drawDrainWater() {
    float waterLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    glDisable(GL_LIGHTING);
    for (auto& d : drains) {
        if (d.vertical) {
            const int SEG = 20;
            for (int s = 0; s < SEG; s++) {
                float za = d.z1 + (d.z2 - d.z1) * s / SEG;
                float zb = d.z1 + (d.z2 - d.z1) * (s + 1) / SEG;
                float yA = drainSurfaceY(cityYatZ(za), waterLevel);
                float yB = drainSurfaceY(cityYatZ(zb), waterLevel);
                const int SUB = 5;
                for (int k = 0; k < SUB; k++) {
                    float xa = d.x1 + (d.x2 - d.x1) * k / SUB;
                    float xb = d.x1 + (d.x2 - d.x1) * (k + 1) / SUB;
                    float y1 = yA + ripple(xa, za, timeAcc, 0.075f, 2.3f, 4.5f);
                    float y2 = yA + ripple(xb, za, timeAcc, 0.075f, 2.3f, 4.5f);
                    float y3 = yB + ripple(xb, zb, timeAcc, 0.075f, 2.3f, 4.5f);
                    float y4 = yB + ripple(xa, zb, timeAcc, 0.075f, 2.3f, 4.5f);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    float gl = 0.5f + 0.5f * sinf((xa + za) * 1.35f - timeAcc * 3.2f);
                    glColor4f(DEEP_BLUE_R + gl * 0.12f,
                              DEEP_BLUE_G + gl * 0.30f,
                              fminf(1.0f, DEEP_BLUE_B + gl * 0.28f), 1.0f);
                    glBegin(GL_QUADS);
                    glNormal3f(0, 1, 0);
                    glVertex3f(xa, y1, za); glVertex3f(xb, y2, za);
                    glVertex3f(xb, y3, zb); glVertex3f(xa, y4, zb);
                    glEnd();
                    glDisable(GL_BLEND);
                }
            }
        } else {
            float yTop = cityYatZ(d.roadEdge);
            float wY = drainSurfaceY(yTop, waterLevel);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.95f);
            const int SEG = 32;
            for (int s = 0; s < SEG; s++) {
                float xa = d.x1 + (d.x2 - d.x1) * s / SEG;
                float xb = d.x1 + (d.x2 - d.x1) * (s + 1) / SEG;
                float ya = wY + ripple(xa, d.z1, timeAcc, 0.075f, 2.3f, 4.5f);
                float yb = wY + ripple(xb, d.z1, timeAcc, 0.075f, 2.3f, 4.5f);
                float yc = wY + ripple(xb, d.z2, timeAcc, 0.075f, 2.3f, 4.5f);
                float yd = wY + ripple(xa, d.z2, timeAcc, 0.075f, 2.3f, 4.5f);
                float gl = 0.5f + 0.5f * sinf((xa + d.z1) * 1.35f - timeAcc * 3.2f);
                glColor4f(DEEP_BLUE_R + gl * 0.12f,
                          DEEP_BLUE_G + gl * 0.30f,
                          fminf(1.0f, DEEP_BLUE_B + gl * 0.28f), 1.0f);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0);
                glVertex3f(xa, ya, d.z1 + 0.05f);
                glVertex3f(xb, yb, d.z1 + 0.05f);
                glVertex3f(xb, yc, d.z2 - 0.05f);
                glVertex3f(xa, yd, d.z2 - 0.05f);
                glEnd();
            }
            glDisable(GL_BLEND);
        }
    }
    setSceneLightingEnabled();
}

// =====================================================================
//  DRAIN OVERFLOW
// =====================================================================
void drawDrainOverflow() {
    if (drainLevel <= DRAIN_SPILL_LEVEL) return;
    float spill = (drainLevel - DRAIN_SPILL_LEVEL) / (1.0f - DRAIN_SPILL_LEVEL);
    spill = fminf(1.0f, spill);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.88f * spill);
    for (auto& d : drains) {
        if (d.vertical) {
            bool roadOnRight = d.roadEdge > (d.x1 + d.x2) * 0.5f;
            float edgeX = roadOnRight ? d.x2 : d.x1;
            float roadX = d.roadEdge;
            for (float z = d.z1 + 1.0f; z < d.z2 - 0.5f; z += 3.0f) {
                float y = cityYatZ(z) + KERB_H + 0.04f;
                float phase = fmodf(timeAcc * 1.8f + z, 3.0f);
                if (phase < 1.6f) {
                    glBegin(GL_QUADS);
                    glNormal3f(0, 1, 0.25f);
                    glVertex3f(edgeX, y, z); glVertex3f(roadX, y + 0.02f, z);
                    glVertex3f(roadX, y + 0.02f, z + 1.0f); glVertex3f(edgeX, y, z + 1.0f);
                    glEnd();
                }
            }
        } else {
            bool roadOnBack = d.roadEdge > (d.z1 + d.z2) * 0.5f;
            float edgeZ = roadOnBack ? d.z1 : d.z2;
            float roadZ = d.roadEdge;
            for (float x = d.x1 + 1.0f; x < d.x2 - 0.5f; x += 3.0f) {
                float y = cityYatZ(roadZ) + KERB_H + 0.04f;
                float phase = fmodf(timeAcc * 1.8f + x, 3.0f);
                if (phase < 1.6f) {
                    glBegin(GL_QUADS);
                    glNormal3f(0, 1, 0.25f);
                    glVertex3f(x, y, edgeZ); glVertex3f(x + 1.0f, y, edgeZ);
                    glVertex3f(x + 1.0f, y + 0.02f, roadZ); glVertex3f(x, y + 0.02f, roadZ);
                    glEnd();
                }
            }
        }
    }
    glDisable(GL_BLEND);
}

// =====================================================================
//  ROAD FLOOD
// =====================================================================
void drawRoadFlood() {
    if (roadFlood <= 0.01f) return;
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        if (r.vertical) {
            float coverZ0 = r.z2 - roadFlood * (r.z2 - r.z1);
            const int SEG = 16;
            for (int s = 0; s < SEG; s++) {
                float za = coverZ0 + (r.z2 - coverZ0) * s / SEG;
                float zb = coverZ0 + (r.z2 - coverZ0) * (s + 1) / SEG;
                float ya0 = cityYatZ(za) + 0.03f + roadFlood * 0.05f;
                float yb0 = cityYatZ(zb) + 0.03f + roadFlood * 0.05f;
                const int SUB = 8;
                for (int k = 0; k < SUB; k++) {
                    float xa = r.x1 + (r.x2 - r.x1) * k / SUB;
                    float xb = r.x1 + (r.x2 - r.x1) * (k + 1) / SUB;
                    float y1 = ya0 + ripple(xa, za, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y2 = ya0 + ripple(xb, za, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y3 = yb0 + ripple(xb, zb, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y4 = yb0 + ripple(xa, zb, timeAcc, 0.04f, 1.2f, 2.5f);
                    float wm = 0.08f + roadFlood * 0.16f;
                    float rr = 0.10f*(1-wm) + wm;
                    float gg = 0.34f*(1-wm) + wm;
                    float bb = 0.82f*(1-wm) + wm;
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glColor4f(rr, gg, bb, 0.75f + roadFlood * 0.20f);
                    glBegin(GL_QUADS);
                    float nx, ny, nz;
                    floodSurfaceNormal(xa, za, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xa, y1, za);
                    floodSurfaceNormal(xb, za, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xb, y2, za);
                    floodSurfaceNormal(xb, zb, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xb, y3, zb);
                    floodSurfaceNormal(xa, zb, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xa, y4, zb);
                    glEnd();
                    glDisable(GL_BLEND);
                }
            }
        } else {
            const int SX = 24, SZ = 8;
            float wm = 0.08f + roadFlood * 0.16f;
            float rr = 0.10f*(1-wm) + wm;
            float gg = 0.34f*(1-wm) + wm;
            float bb = 0.82f*(1-wm) + wm;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (int sx = 0; sx < SX; sx++) {
                float xa = r.x1 + (r.x2 - r.x1) * sx / SX;
                float xb = r.x1 + (r.x2 - r.x1) * (sx + 1) / SX;
                for (int sz = 0; sz < SZ; sz++) {
                    float za = r.z1 + (r.z2 - r.z1) * sz / SZ;
                    float zb = r.z1 + (r.z2 - r.z1) * (sz + 1) / SZ;
                    float ya = cityYatZ(za) + 0.03f + roadFlood * 0.05f;
                    float yb = cityYatZ(zb) + 0.03f + roadFlood * 0.05f;
                    float y1 = ya + ripple(xa, za, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y2 = ya + ripple(xb, za, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y3 = yb + ripple(xb, zb, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y4 = yb + ripple(xa, zb, timeAcc, 0.04f, 1.2f, 2.5f);
                    glColor4f(rr, gg, bb, 0.75f + roadFlood * 0.20f);
                    glBegin(GL_QUADS);
                    float nx, ny, nz;
                    floodSurfaceNormal(xa, za, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz); glVertex3f(xa, y1, za);
                    floodSurfaceNormal(xb, za, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz); glVertex3f(xb, y2, za);
                    floodSurfaceNormal(xb, zb, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz); glVertex3f(xb, y3, zb);
                    floodSurfaceNormal(xa, zb, 0.03f + roadFlood * 0.05f, 0.04f, 1.2f, 2.5f, nx, ny, nz);
                    glNormal3f(nx, ny, nz); glVertex3f(xa, y4, zb);
                    glEnd();
                }
            }
            glDisable(GL_BLEND);
        }
    }
}

// =====================================================================
//  GOLI
// =====================================================================
void drawGoliAndDrains() {
    float z1 = GOLI_Z1, z2 = GOLI_Z2;
    float roadHalfW = GOLI_HALF_W;
    float drainW = GOLI_DRAIN_W;
    float kerbW = GOLI_KERB_W;
    float drainDepth = DRAIN_DEPTH;
    const int SEG = 50;
    for (int i = 0; i < SEG; i++) {
        float t0 = (float)i / SEG;
        float t1 = (float)(i + 1) / SEG;
        float za = z1 + (z2 - z1) * t0;
        float zb = z1 + (z2 - z1) * t1;
        float xa0 = goliCenterX(za), xa1 = goliCenterX(zb);
        float dx = xa1 - xa0, dz = zb - za;
        float len = sqrtf(dx*dx + dz*dz);
        if (len < 1e-5f) continue;
        float nx = -dz / len, nz = dx / len;
        float ya = cityYatZ(za), yb = cityYatZ(zb);
        float xl0 = xa0 - nx * roadHalfW, zl0 = za - nz * roadHalfW;
        float xr0 = xa0 + nx * roadHalfW, zr0 = za + nz * roadHalfW;
        float xl1 = xa1 - nx * roadHalfW, zl1 = zb - nz * roadHalfW;
        float xr1 = xa1 + nx * roadHalfW, zr1 = zb + nz * roadHalfW;
        glColor3f(ROAD_GRAY + 0.03f, ROAD_GRAY + 0.03f, ROAD_GRAY + 0.03f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(xl0, ya + 0.005f, zl0); glVertex3f(xr0, ya + 0.005f, zr0);
        glVertex3f(xr1, yb + 0.005f, zr1); glVertex3f(xl1, yb + 0.005f, zl1);
        glEnd();
        if (roadFlood > 0.01f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            float wm = 0.08f + roadFlood * 0.16f;
            glColor4f(0.10f*(1-wm)+wm, 0.34f*(1-wm)+wm, 0.82f*(1-wm)+wm,
                      0.75f + roadFlood * 0.20f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xl0, ya + 0.04f, zl0); glVertex3f(xr0, ya + 0.04f, zr0);
            glVertex3f(xr1, yb + 0.04f, zr1); glVertex3f(xl1, yb + 0.04f, zl1);
            glEnd();
            glDisable(GL_BLEND);
        }
        for (int side = -1; side <= 1; side += 2) {
            float xE0 = (side == -1) ? xl0 : xr0;
            float zE0 = (side == -1) ? zl0 : zr0;
            float xE1 = (side == -1) ? xl1 : xr1;
            float zE1 = (side == -1) ? zl1 : zr1;
            float xK0 = xE0 + nx * kerbW * side, zK0 = zE0 + nz * kerbW * side;
            float xK1 = xE1 + nx * kerbW * side, zK1 = zE1 + nz * kerbW * side;
            float xD0 = xK0 + nx * drainW * side, zD0 = zK0 + nz * drainW * side;
            float xD1 = xK1 + nx * drainW * side, zD1 = zK1 + nz * drainW * side;
            glColor3f(0.72f, 0.72f, 0.70f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xE0, ya + KERB_H, zE0); glVertex3f(xK0, ya + KERB_H, zK0);
            glVertex3f(xK1, yb + KERB_H, zK1); glVertex3f(xE1, yb + KERB_H, zE1);
            glEnd();
            glColor3f(0.05f, 0.05f, 0.07f);
            glBegin(GL_QUADS);
            glNormal3f(-nx * side, 0.3f, -nz * side);
            glVertex3f(xK0, ya + KERB_H, zK0); glVertex3f(xK1, yb + KERB_H, zK1);
            glVertex3f(xK1, yb - drainDepth, zK1); glVertex3f(xK0, ya - drainDepth, zK0);
            glEnd();
            glColor3f(0.03f, 0.03f, 0.05f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xK0, ya - drainDepth, zK0); glVertex3f(xD0, ya - drainDepth, zD0);
            glVertex3f(xD1, yb - drainDepth, zD1); glVertex3f(xK1, yb - drainDepth, zK1);
            glEnd();
            glColor3f(0.08f, 0.08f, 0.10f);
            glBegin(GL_QUADS);
            glNormal3f(nx * side, 0.3f, nz * side);
            glVertex3f(xD0, ya - drainDepth, zD0); glVertex3f(xD1, yb - drainDepth, zD1);
            glVertex3f(xD1, yb + KERB_H, zD1); glVertex3f(xD0, ya + KERB_H, zD0);
            glEnd();
        }
    }
}

void drawGoliWater() {
    float waterLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    float z1 = GOLI_Z1, z2 = GOLI_Z2;
    float roadHalfW = GOLI_HALF_W;
    float drainW = GOLI_DRAIN_W;
    float kerbW = GOLI_KERB_W;
    const int SEG = 40;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < SEG; i++) {
        float t0 = (float)i / SEG;
        float t1 = (float)(i + 1) / SEG;
        float za = z1 + (z2 - z1) * t0;
        float zb = z1 + (z2 - z1) * t1;
        float xa0 = goliCenterX(za), xa1 = goliCenterX(zb);
        float dx = xa1 - xa0, dz = zb - za;
        float len = sqrtf(dx*dx + dz*dz);
        if (len < 1e-5f) continue;
        float nx = -dz / len, nz = dx / len;
        float ya = cityYatZ(za), yb = cityYatZ(zb);
        float wY0 = drainSurfaceY(ya, waterLevel);
        float wY1 = drainSurfaceY(yb, waterLevel);
        for (int side = -1; side <= 1; side += 2) {
            float xE0 = (side == -1) ? (xa0 - nx * roadHalfW) : (xa0 + nx * roadHalfW);
            float zE0 = (side == -1) ? (za - nz * roadHalfW) : (za + nz * roadHalfW);
            float xK0 = xE0 + nx * kerbW * side, zK0 = zE0 + nz * kerbW * side;
            float xD0 = xK0 + nx * drainW * side, zD0 = zK0 + nz * drainW * side;
            float xE1 = (side == -1) ? (xa1 - nx * roadHalfW) : (xa1 + nx * roadHalfW);
            float zE1 = (side == -1) ? (zb - nz * roadHalfW) : (zb + nz * roadHalfW);
            float xK1 = xE1 + nx * kerbW * side, zK1 = zE1 + nz * kerbW * side;
            float xD1 = xK1 + nx * drainW * side, zD1 = zK1 + nz * drainW * side;
            float xI0 = xK0 + nx * 0.08f * side, zI0 = zK0 + nz * 0.08f * side;
            float xO0 = xD0 - nx * 0.08f * side, zO0 = zD0 - nz * 0.08f * side;
            float xI1 = xK1 + nx * 0.08f * side, zI1 = zK1 + nz * 0.08f * side;
            float xO1 = xD1 - nx * 0.08f * side, zO1 = zD1 - nz * 0.08f * side;
            float yI0 = wY0 + ripple(xI0, zI0, timeAcc, 0.075f, 2.3f, 4.5f);
            float yO0 = wY0 + ripple(xO0, zO0, timeAcc, 0.075f, 2.3f, 4.5f);
            float yI1 = wY1 + ripple(xI1, zI1, timeAcc, 0.075f, 2.3f, 4.5f);
            float yO1 = wY1 + ripple(xO1, zO1, timeAcc, 0.075f, 2.3f, 4.5f);
            float gl = 0.5f + 0.5f * sinf((za + side) * 1.35f - timeAcc * 3.2f);
            glColor4f(DEEP_BLUE_R + gl * 0.12f, DEEP_BLUE_G + gl * 0.30f,
                      fminf(1.0f, DEEP_BLUE_B + gl * 0.28f), 1.0f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xI0, yI0, zI0); glVertex3f(xO0, yO0, zO0);
            glVertex3f(xO1, yO1, zO1); glVertex3f(xI1, yI1, zI1);
            glEnd();
        }
    }
    glDisable(GL_BLEND);
    setSceneLightingEnabled();
}

// =====================================================================
//  FLOW RIBBONS
// =====================================================================
void drawFlowRibbon(float sx, float sz, float sy, float ex, float ez, float ey,
                    float w0, float w1, float alpha, bool drainFlow = false)
{
    const int SEG = 24;
    float dx = ex - sx, dz = ez - sz;
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 1e-5f) return;
    float sideX = -dz / length, sideZ = dx / length;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < SEG; i++) {
        float t0 = (float)i / SEG;
        float t1 = (float)(i + 1) / SEG;
        float cx0 = sx + dx * t0, cz0 = sz + dz * t0;
        float cx1 = sx + dx * t1, cz1 = sz + dz * t1;
        float ww0 = w0 + (w1 - w0) * t0;
        float ww1 = w0 + (w1 - w0) * t1;
        float y0 = sy + (ey - sy) * t0;
        float y1 = sy + (ey - sy) * t1;
        float streak = 0.5f + 0.5f * sinf(t0 * 32.0f - timeAcc * 7.0f);
        float bR = drainFlow ? DEEP_BLUE_R : 0.10f;
        float bG = drainFlow ? DEEP_BLUE_G : 0.34f;
        float bB = drainFlow ? DEEP_BLUE_B : 0.82f;
        float sh = streak * 0.16f;
        glColor4f(bR + sh, bG + sh, fminf(1.0f, bB + sh), alpha);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(cx0 - sideX * ww0, y0, cz0 - sideZ * ww0);
        glVertex3f(cx0 + sideX * ww0, y0, cz0 + sideZ * ww0);
        glVertex3f(cx1 + sideX * ww1, y1, cz1 + sideZ * ww1);
        glVertex3f(cx1 - sideX * ww1, y1, cz1 - sideZ * ww1);
        glEnd();
    }
    glDisable(GL_BLEND);
}

void drawRoadFlowToGate() {
    if (roadFlood <= 0.08f) return;
    float flow = fminf(1.0f, roadFlood * 1.6f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.18f, 0.46f, 0.92f, 0.82f * flow);
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        if (r.vertical) {
            for (float z = r.z1 + 1.0f; z < r.z2 - 0.5f; z += 3.0f) {
                float y = cityYatZ(z) + 0.13f;
                float phase = fmodf(timeAcc * 1.25f + z * 0.12f, 2.0f);
                float reach = 0.35f + 0.65f * phase / 2.0f;
                float centre = (r.x1 + r.x2) * 0.5f;
                float left = r.x1 + (centre - r.x1) * reach;
                float right = r.x2 - (r.x2 - centre) * reach;
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(r.x1 + 0.2f, y, z); glVertex3f(left, y + 0.015f, z);
                glVertex3f(left, y + 0.015f, z + 0.55f); glVertex3f(r.x1 + 0.2f, y, z + 0.55f);
                glVertex3f(right, y + 0.015f, z); glVertex3f(r.x2 - 0.2f, y, z);
                glVertex3f(r.x2 - 0.2f, y, z + 0.55f); glVertex3f(right, y + 0.015f, z + 0.55f);
                glEnd();
            }
        } else {
            float z = (r.z1 + r.z2) * 0.5f;
            float y = cityYatZ(z) + 0.13f;
            for (float x = r.x1 + 1.0f; x < r.x2 - 0.5f; x += 3.0f) {
                float phase = fmodf(timeAcc * 1.25f + x * 0.12f, 2.0f);
                float reach = 0.35f + 0.65f * phase / 2.0f;
                float centre = (r.z1 + r.z2) * 0.5f;
                float front = r.z1 + (centre - r.z1) * reach;
                float back = r.z2 - (r.z2 - centre) * reach;
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(x, y, r.z1 + 0.2f); glVertex3f(x + 0.55f, y, r.z1 + 0.2f);
                glVertex3f(x + 0.55f, y + 0.015f, front); glVertex3f(x, y + 0.015f, front);
                glVertex3f(x, y + 0.015f, back); glVertex3f(x + 0.55f, y + 0.015f, back);
                glVertex3f(x + 0.55f, y, r.z2 - 0.2f); glVertex3f(x, y, r.z2 - 0.2f);
                glEnd();
            }
        }
    }
    if (roadFlood > 0.18f) {
        float y0 = cityYatZ(7.0f) + 0.16f;
        float y1 = cityYatZ(GATE_Z) + 0.16f;
        glColor4f(0.18f, 0.46f, 0.92f, 0.85f * flow);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0.25f);
        glVertex3f(-1.2f, y0, 7.0f); glVertex3f( 1.2f, y0, 7.0f);
        glVertex3f( 1.2f, y1, GATE_Z); glVertex3f(-1.2f, y1, GATE_Z);
        glEnd();
    }
    glDisable(GL_BLEND);
}

void drawRoadAndDrainFlowToTurbine() {
    const float outletZ = GATE_Z + 1.5f;
    const float outletY = RIVER_Y + 1.3f;
    float roadFlow = fminf(1.0f, roadFlood * 1.6f);
    if (roadFlow > 0.08f) {
        drawFlowRibbon(0.0f, -6.5f, cityYatZ(-6.5f) + 0.14f,
                       0.0f, outletZ, outletY, 1.4f, 0.6f, 0.82f * roadFlow);
    }
    if (roadFlow <= 0.08f) return;
    float waterLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    for (auto& d : drains) {
        if (d.vertical) {
            float sourceX = (d.x1 + d.x2) * 0.5f;
            float sourceZ = d.z2;
            float sourceY = drainSurfaceY(cityYatZ(sourceZ), waterLevel);
            drawFlowRibbon(sourceX, sourceZ, sourceY, 0.0f, outletZ, outletY,
                           0.75f, 0.45f, 0.38f + 0.42f * waterLevel, true);
        } else {
            float sourceX = (d.x1 + d.x2) * 0.5f;
            float sourceZ = (d.z1 + d.z2) * 0.5f;
            float sourceY = drainSurfaceY(cityYatZ(d.roadEdge), waterLevel);
            drawFlowRibbon(sourceX, sourceZ, sourceY, 0.0f, outletZ, outletY,
                           0.6f, 0.45f, 0.38f + 0.42f * waterLevel, true);
        }
    }
}

// =====================================================================
//  GATE COLLECTION POOL
// =====================================================================
void drawGateCollectionPool() {
    if (drainLevel <= 0.12f) return;
    float fill = fminf(1.0f, (drainLevel - 0.12f) / 0.88f);
    float poolY = GATE_Y - 0.35f + fill * 0.55f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.82f);
    drawWaterPlane(-18.0f, 9.5f, 18.0f, GATE_Z - 0.4f, poolY,
                   0.82f, 0.06f, 1.1f, 2.2f, true, true, 0.9f);
    glDisable(GL_BLEND);
    glColor3f(0.55f, 0.56f, 0.52f);
    glPushMatrix(); glTranslatef(-19.0f, poolY - 0.2f, 12.0f);
    glScalef(0.8f, 0.4f, 6.0f); drawCube(); glPopMatrix();
    glPushMatrix(); glTranslatef( 19.0f, poolY - 0.2f, 12.0f);
    glScalef(0.8f, 0.4f, 6.0f); drawCube(); glPopMatrix();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int side = -1; side <= 1; side += 2) {
        glColor4f(0.18f, 0.46f, 0.92f, 0.82f * fill);
        glBegin(GL_QUADS);
        glVertex3f(side * 15.0f, poolY + 0.05f, 2.0f);
        glVertex3f(side * 16.0f, poolY + 0.05f, 2.0f);
        glVertex3f(side * 2.0f,  poolY + 0.05f, 11.5f);
        glVertex3f(side * 1.0f,  poolY + 0.05f, 11.5f);
        glEnd();
    }
    for (int i = 0; i < NSIDE_CITY_GATES; ++i) {
        float x = SIDE_CITY_GATE_XS[i];
        glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.82f);
        drawWaterPlane(x - 12.0f, 9.5f, x + 12.0f, GATE_Z - 0.4f,
                       poolY, 0.82f, 0.06f, 1.1f, 2.2f, true, true, 0.9f);
    }
    glDisable(GL_BLEND);
}

void drawGate() {
    float c = cloudCover;
    glColor3f(0.66f - c*0.26f, 0.64f - c*0.26f, 0.60f - c*0.26f);
    glPushMatrix();
    glTranslatef(0, GATE_Y - 1.5f, GATE_Z + 0.6f);
    glScalef(50.0f, 3.0f, 1.2f); drawCube(); glPopMatrix();
    glColor3f(0.78f - c*0.28f, 0.76f - c*0.28f, 0.72f - c*0.28f);
    glPushMatrix();
    glTranslatef(0, GATE_Y + 0.1f, GATE_Z + 0.6f);
    glScalef(51.0f, 0.3f, 1.6f); drawCube(); glPopMatrix();
    glColor3f(0.05f, 0.05f, 0.07f);
    glPushMatrix();
    glTranslatef(0, GATE_Y - 1.4f, GATE_Z + 1.15f);
    glScalef(12.0f, 2.6f, 0.25f); drawCube(); glPopMatrix();
    float gateSlideY = GATE_Y - 1.4f + gateOpen * 2.5f;
    glColor3f(0.72f, 0.62f, 0.15f);
    glPushMatrix();
    glTranslatef(0, gateSlideY, GATE_Z + 1.25f);
    glScalef(12.0f, 2.5f, 0.30f); drawCube(); glPopMatrix();
    glColor3f(0.28f, 0.28f, 0.30f);
    glPushMatrix();
    glTranslatef(-10.0f, GATE_Y + 1.5f, GATE_Z + 0.6f);
    glScalef(0.5f, 3.0f, 0.5f); drawCube(); glPopMatrix();
    glDisable(GL_LIGHTING);
    const float signX = -10.0f;
    const float signY = GATE_Y + 3.2f;
    const float signZ = GATE_Z + 1.35f;
    bool cityGateOpen = targetGate > 0.5f || gateOpen > 0.5f;
    bool cityGateNeedsOpening = drainLevel >= DRAIN_WARNING_LEVEL && !cityGateOpen;
    glColor3f(0.035f, 0.045f, 0.065f);
    glBegin(GL_QUADS);
    glVertex3f(signX - 4.6f, signY - 0.85f, signZ);
    glVertex3f(signX + 4.6f, signY - 0.85f, signZ);
    glVertex3f(signX + 4.6f, signY + 0.85f, signZ);
    glVertex3f(signX - 4.6f, signY + 0.85f, signZ);
    glEnd();
    glLineWidth(3.0f);
    float signR = cityGateOpen ? 0.08f : (cityGateNeedsOpening ? 1.0f : 0.85f);
    float signG = cityGateOpen ? 1.0f : (cityGateNeedsOpening ? 0.10f : 0.48f);
    glColor3f(signR, signG, 0.035f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(signX - 4.6f, signY - 0.85f, signZ + 0.04f);
    glVertex3f(signX + 4.6f, signY - 0.85f, signZ + 0.04f);
    glVertex3f(signX + 4.6f, signY + 0.85f, signZ + 0.04f);
    glVertex3f(signX - 4.6f, signY + 0.85f, signZ + 0.04f);
    glEnd();
    glPushMatrix();
    glTranslatef(signX - 3.55f, signY - 0.28f, signZ + 0.06f);
    glScalef(0.0075f, 0.0075f, 0.0075f);
    glColor3f(signR, signG, 0.04f);
    const char* gateMessage = cityGateOpen
        ? "GATE OPEN"
        : (cityGateNeedsOpening ? "OPEN GATE" : "GATE CLOSED");
    for (const char* c = gateMessage; *c; ++c)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix();
    glLineWidth(1.0f);
    setSceneLightingEnabled();
}

void drawSideCityGates() {
    const float gateHalfWidth = 11.0f;
    bool cityGateOpen = targetGate > 0.5f || gateOpen > 0.5f;
    bool cityGateNeedsOpening = drainLevel >= DRAIN_WARNING_LEVEL && !cityGateOpen;
    float signR = cityGateOpen ? 0.08f : (cityGateNeedsOpening ? 1.0f : 0.85f);
    float signG = cityGateOpen ? 1.0f : (cityGateNeedsOpening ? 0.10f : 0.48f);
    const char* gateMessage = cityGateOpen
        ? "GATE OPEN"
        : (cityGateNeedsOpening ? "OPEN GATE" : "GATE CLOSED");

    for (int i = 0; i < 2; ++i) {
        float x = SIDE_CITY_GATE_XS[i];
        glColor3f(0.60f, 0.59f, 0.55f);
        glPushMatrix();
        glTranslatef(x, GATE_Y - 1.5f, GATE_Z + 0.6f);
        glScalef(23.0f, 3.0f, 1.2f); drawCube(); glPopMatrix();
        glColor3f(0.76f, 0.74f, 0.70f);
        glPushMatrix();
        glTranslatef(x, GATE_Y + 0.1f, GATE_Z + 0.6f);
        glScalef(24.0f, 0.3f, 1.6f); drawCube(); glPopMatrix();

        for (int side = -1; side <= 1; side += 2) {
            glColor3f(0.35f, 0.36f, 0.38f);
            glPushMatrix();
            glTranslatef(x + side * gateHalfWidth, GATE_Y + 1.2f, GATE_Z + 0.6f);
            glScalef(0.75f, 5.0f, 0.9f); drawCube(); glPopMatrix();
        }

        glColor3f(0.05f, 0.05f, 0.07f);
        glPushMatrix();
        glTranslatef(x, GATE_Y - 1.4f, GATE_Z + 1.15f);
        glScalef(20.0f, 2.6f, 0.25f); drawCube(); glPopMatrix();
        float gateSlideY = GATE_Y - 1.4f + gateOpen * 2.5f;
        glColor3f(0.72f, 0.62f, 0.15f);
        glPushMatrix();
        glTranslatef(x, gateSlideY, GATE_Z + 1.25f);
        glScalef(20.0f, 2.5f, 0.30f); drawCube(); glPopMatrix();

        float signY = GATE_Y + 4.0f;
        float signZ = GATE_Z + 1.25f;
        glDisable(GL_LIGHTING);
        glColor3f(0.035f, 0.045f, 0.065f);
        glBegin(GL_QUADS);
        glVertex3f(x - 4.4f, signY - 0.75f, signZ);
        glVertex3f(x + 4.4f, signY - 0.75f, signZ);
        glVertex3f(x + 4.4f, signY + 0.75f, signZ);
        glVertex3f(x - 4.4f, signY + 0.75f, signZ);
        glEnd();
        glColor3f(signR, signG, 0.035f);
        glPushMatrix();
        glTranslatef(x - 3.45f, signY - 0.27f, signZ + 0.05f);
        glScalef(0.007f, 0.007f, 0.007f);
        for (const char* c = gateMessage; *c; ++c)
            glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
        glPopMatrix();
        setSceneLightingEnabled();
    }
}

// =====================================================================
//  CITY HYDRO
// =====================================================================
const int NTURBINES = 4;
const float TURBINE_XS[NTURBINES] = { -9.0f, -3.0f, 3.0f, 9.0f };
const int NCITY_TURBINES = NTURBINES * (NSIDE_CITY_GATES + 1);
const float TURBINE_R = 1.7f;
const float CITY_WHEEL_RADIAL_SCALE = 2.4f;
const float TURBINE_WHEEL_Y = RIVER_Y + 0.55f;
const float TURBINE_WHEEL_TOP_Y = TURBINE_WHEEL_Y + TURBINE_R * CITY_WHEEL_RADIAL_SCALE;
const float MANIFOLD_Y = -0.30f;
const float MANIFOLD_Z = 18.6f;
const float MANIFOLD_R = 0.55f;
const float NOZZLE_EXIT_Y = -0.55f;
const float NOZZLE_Z = 19.0f;
const float PENSTOCK_R = 0.32f;

float turbineWheelAngles[NTURBINES] = {0,0,0,0};
float turbineGenAngles[NTURBINES]   = {0,0,0,0};
float sideTurbineWheelAngles[NSIDE_CITY_GATES][NTURBINES] = {};
float sideTurbineGenAngles[NSIDE_CITY_GATES][NTURBINES] = {};

void drawWaterJetSegment(float x0, float y0, float z0,
                         float x1, float y1, float z1,
                         float r0, float r1, float alpha, float streakPhase)
{
    float dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
    float len = sqrtf(dx*dx + dy*dy + dz*dz);
    if (len < 1e-5f) return;
    float ux = dx/len, uy = dy/len, uz = dz/len;
    float ax, ay, az;
    if (fabsf(uy) < 0.9f) {
        ax = -uz; ay = 0; az = ux;
        float l = sqrtf(ax*ax + az*az); ax /= l; az /= l;
    } else {
        ax = 0; ay = -uz; az = uy;
        float l = sqrtf(ay*ay + az*az); ay /= l; az /= l;
    }
    float bx = uy*az - uz*ay;
    float by = uz*ax - ux*az;
    float bz = ux*ay - uy*ax;
    const int SIDES = 10;
    const int RINGS = 10;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < RINGS; i++) {
        float t0 = (float)i / RINGS;
        float t1 = (float)(i + 1) / RINGS;
        float cx0 = x0 + dx*t0, cy0 = y0 + dy*t0, cz0 = z0 + dz*t0;
        float cx1 = x0 + dx*t1, cy1 = y0 + dy*t1, cz1 = z0 + dz*t1;
        float rr0 = r0 + (r1 - r0) * t0;
        float rr1 = r0 + (r1 - r0) * t1;
        float streak = 0.5f + 0.5f * sinf(t0*20.0f - timeAcc*18.0f + streakPhase);
        glColor4f(0.55f + streak * 0.20f, 0.80f + streak * 0.15f, 0.98f, alpha);
        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= SIDES; s++) {
            float a = s * 2.0f * M_PI / SIDES;
            float ca = cosf(a), sa = sinf(a);
            float nx = ax*ca + bx*sa, ny = ay*ca + by*sa, nz = az*ca + bz*sa;
            glNormal3f(nx, ny, nz);
            glVertex3f(cx0 + nx*rr0, cy0 + ny*rr0, cz0 + nz*rr0);
            glVertex3f(cx1 + nx*rr1, cy1 + ny*rr1, cz1 + nz*rr1);
        }
        glEnd();
    }
    glDisable(GL_BLEND);
}

void drawTurbineWheel(float cx, float cy, float cz, float angle, float flowFactor,
                      float radialScale = 1.0f) {
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(1.0f, radialScale, radialScale);
    glColor3f(0.28f, 0.30f, 0.34f);
    glPushMatrix(); glRotatef(90, 0, 1, 0); cylinder(TURBINE_R * 0.20f, 2.2f); glPopMatrix();
    glColor3f(0.72f, 0.82f, 0.94f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 1.0f, 0, 0);
        glRotatef(90, 0, 1, 0);
        glutSolidTorus(0.18f, TURBINE_R * 0.90f, 20, 48);
        glPopMatrix();
    }
    glPushMatrix();
    glRotatef(angle, 1, 0, 0);
    const int NB = 12;
    for (int i = 0; i < NB; i++) {
        glPushMatrix();
        glRotatef(i * (360.0f / NB), 1, 0, 0);
        float bladeR = TURBINE_R * 0.76f;
        glColor3f(0.30f, 0.72f, 0.96f);
        glPushMatrix();
        glTranslatef(0, bladeR, 0.14f);
        glRotatef(-22, 1, 0, 0);
        glScalef(0.46f, 0.90f, 0.32f);
        glutSolidSphere(1.0, 16, 12);
        glPopMatrix();
        glColor3f(0.18f, 0.48f, 0.78f);
        glPushMatrix();
        glTranslatef(0, bladeR, -0.14f);
        glRotatef(22, 1, 0, 0);
        glScalef(0.46f, 0.90f, 0.32f);
        glutSolidSphere(1.0, 16, 12);
        glPopMatrix();
        glPopMatrix();
    }
    glColor3f(0.98f, 0.74f, 0.16f);
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glRotatef(i * 90.0f, 1, 0, 0);
        glPushMatrix();
        glTranslatef(0, TURBINE_R * 0.48f, 0);
        glScalef(0.35f, TURBINE_R * 0.80f, 0.12f); drawCube(); glPopMatrix();
        glPopMatrix();
    }
    glPopMatrix();
    glColor3f(1.0f, 0.78f, 0.12f);
    glutSolidSphere(0.58f, 24, 18);
    glColor3f(0.92f, 0.24f, 0.12f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 1.08f, 0, 0);
        glutSolidSphere(0.48f, 20, 16);
        glPopMatrix();
    }
    glPopMatrix();
}

void drawPenstock() {
    glColor3f(0.42f, 0.42f, 0.46f);
    drawSolidPipe(-12.5f, MANIFOLD_Y, MANIFOLD_Z,
                   12.5f, MANIFOLD_Y, MANIFOLD_Z,
                   MANIFOLD_R, MANIFOLD_R, 16, true);
    glColor3f(0.50f, 0.50f, 0.52f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * 12.5f, MANIFOLD_Y - 0.3f, MANIFOLD_Z);
        glScalef(0.5f, 1.2f, 0.5f); drawCube(); glPopMatrix();
    }
    for (int i = 0; i < NTURBINES; i++) {
        float x = TURBINE_XS[i];
        glColor3f(0.35f, 0.35f, 0.40f);
        drawSolidPipe(x, MANIFOLD_Y, MANIFOLD_Z,
                      x, NOZZLE_EXIT_Y + 0.30f, MANIFOLD_Z,
                      PENSTOCK_R, PENSTOCK_R * 0.9f, 12, true);
        glColor3f(0.30f, 0.30f, 0.34f);
        drawSolidPipe(x, NOZZLE_EXIT_Y + 0.30f, MANIFOLD_Z,
                      x, NOZZLE_EXIT_Y + 0.15f, NOZZLE_Z,
                      PENSTOCK_R * 0.9f, 0.25f, 12, true);
        glColor3f(0.24f, 0.24f, 0.27f);
        drawSolidPipe(x, NOZZLE_EXIT_Y + 0.15f, NOZZLE_Z,
                      x, NOZZLE_EXIT_Y, NOZZLE_Z,
                      0.25f, 0.15f, 12, true);
        glColor3f(0.65f, 0.20f, 0.15f);
        glPushMatrix();
        glTranslatef(x, MANIFOLD_Y + 0.35f, MANIFOLD_Z);
        glRotatef(90, 1, 0, 0);
        cylinder(0.25f, 0.10f);
        glPopMatrix();
    }
}

void drawGeneratorUnit(float cx, float cy, float cz, float angle) {
    float genY = cy + TURBINE_R * CITY_WHEEL_RADIAL_SCALE + 1.0f;
    glColor3f(0.32f, 0.32f, 0.36f);
    glPushMatrix();
    glTranslatef(cx, cy + TURBINE_R * CITY_WHEEL_RADIAL_SCALE + 0.5f, cz);
    glScalef(0.25f, 1.0f, 0.25f); drawCube(); glPopMatrix();
    glColor3f(0.48f, 0.50f, 0.55f);
    glPushMatrix();
    glTranslatef(cx, genY, cz);
    glRotatef(90, 0, 1, 0);
    cylinder(0.85f, 1.5f);
    glPopMatrix();
    glColor3f(0.40f, 0.42f, 0.46f);
    glPushMatrix(); glTranslatef(cx - 0.75f, genY, cz); glRotatef(90, 0, 1, 0); disc(0.85f); glPopMatrix();
    glPushMatrix(); glTranslatef(cx + 0.75f, genY, cz); glRotatef(-90, 0, 1, 0); disc(0.85f); glPopMatrix();
    glPushMatrix();
    glTranslatef(cx + 0.76f, genY, cz);
    glRotatef(-90, 0, 1, 0);
    glRotatef(angle, 0, 0, 1);
    glColor3f(0.85f, 0.75f, 0.20f);
    for (int k = 0; k < 3; k++) {
        glPushMatrix();
        glRotatef(k * 120.0f, 0, 0, 1);
        glTranslatef(0.40f, 0, 0);
        glScalef(0.55f, 0.08f, 0.08f);
        drawCube(); glPopMatrix();
    }
    glPopMatrix();
    if (turbineFlow > 0.15f) {
        float blink = 0.5f + 0.5f * sinf(timeAcc * 6.0f);
        glDisable(GL_LIGHTING);
        glColor3f(0.15f + blink * 0.85f, 0.06f, 0.04f);
        glPushMatrix(); glTranslatef(cx, genY + 0.95f, cz); sphere(0.12f); glPopMatrix();
        setSceneLightingEnabled();
    }
}

void drawSplashAt(float cx, float cy, float cz, float flow, float phaseOffset) {
    if (flow < 0.08f) return;
    const int NP = 26;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < NP; i++) {
        float seed = i * 7.31f + phaseOffset;
        float life = fmodf(timeAcc * 1.6f + seed, 1.0f);
        float speed = 1.6f + fmodf(seed * 2.7f, 1.0f) * 2.2f;
        float angle = fmodf(seed * 5.13f, 1.0f) * 2.0f * M_PI;
        float elev  = 0.4f + fmodf(seed * 3.71f, 1.0f) * 1.4f;
        float r     = life * speed * 0.55f;
        float px = cx + cosf(angle) * r;
        float pz = cz + sinf(angle) * r * 0.7f;
        float py = cy + elev * life - 2.2f * life * life;
        float alpha = (1.0f - life) * 0.70f * flow;
        glColor4f(0.80f, 0.90f, 1.00f, alpha);
        glPushMatrix();
        glTranslatef(px, py, pz);
        sphere(0.07f * (1.0f - life * 0.5f));
        glPopMatrix();
    }
    glColor4f(0.90f, 0.96f, 1.00f, 0.55f * flow);
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(0.9f, 0.35f, 0.9f);
    sphere(0.55f);
    glPopMatrix();
    glDisable(GL_BLEND);
}

void drawGateSpillwaySheet() {
    if (gateOpen < 0.10f) return;
    float flow = fmaxf(turbineFlow, roadFlood * 0.75f);
    if (flow < 0.05f) return;
    float topY = GATE_Y + 0.1f;
    float botY = MANIFOLD_Y + 0.3f;
    float topZ = GATE_Z + 1.2f;
    float botZ = MANIFOLD_Z;
    float halfW = 5.0f;
    const int COLS = 28;
    const int ROWS = 8;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < COLS; i++) {
        for (int j = 0; j < ROWS; j++) {
            float t0 = (float)i / COLS;
            float t1 = (float)(i + 1) / COLS;
            float u0 = (float)j / ROWS;
            float u1 = (float)(j + 1) / ROWS;
            float x00 = -halfW + t0 * 2.0f * halfW;
            float x10 = -halfW + t1 * 2.0f * halfW;
            float w0 = sinf(t0 * 14.0f + timeAcc * 6.0f) * 0.15f;
            float w1 = sinf(t1 * 14.0f + timeAcc * 6.0f) * 0.15f;
            float z00 = topZ + (botZ - topZ) * u0 + w0 * u0 * 0.35f;
            float z01 = topZ + (botZ - topZ) * u1 + w0 * u1 * 0.35f;
            float z10 = topZ + (botZ - topZ) * u0 + w1 * u0 * 0.35f;
            float z11 = topZ + (botZ - topZ) * u1 + w1 * u1 * 0.35f;
            float y0 = topY + (botY - topY) * u0;
            float y1 = topY + (botY - topY) * u1;
            float edge = 1.0f - fabsf(t0 * 2.0f - 1.0f) * 0.35f;
            float streak = 0.5f + 0.5f * sinf(t0 * 18.0f + u0 * 12.0f + timeAcc * 10.0f);
            float a = (0.60f + 0.30f * edge) * flow;
            glColor4f(0.48f + streak * 0.25f, 0.78f + streak * 0.15f, 0.98f, a);
            glBegin(GL_QUADS);
            glNormal3f(0, 0.7f, 0.5f);
            glVertex3f(x00, y0, z00); glVertex3f(x10, y0, z10);
            glVertex3f(x10, y1, z11); glVertex3f(x00, y1, z01);
            glEnd();
        }
    }
    glDisable(GL_BLEND);
}

void drawTailracePool() {
    if (turbineFlow < 0.05f && riverLevel <= 0.36f) return;
    float riverSurfaceY = RIVER_Y + ((riverLevel - 0.35f) / 0.5f) * 1.5f;
    float tailY = riverSurfaceY + 0.10f + turbineFlow * 0.15f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(DEEP_BLUE_R + 0.10f, DEEP_BLUE_G + 0.20f, DEEP_BLUE_B, 0.75f);
    drawWaterPlane(-13.5f, TURBINE_Z - 1.0f, 13.5f, TURBINE_Z + 4.0f, tailY,
                   0.75f, 0.10f, 1.6f, 5.0f, true, true, 1.0f);
    for (int i = 0; i < NSIDE_CITY_GATES; ++i) {
        float x = SIDE_CITY_GATE_XS[i];
        drawWaterPlane(x - 11.0f, TURBINE_Z - 1.0f, x + 11.0f, TURBINE_Z + 4.0f,
                       tailY, 0.75f, 0.10f, 1.6f, 5.0f, true, true, 1.0f);
    }
    glDisable(GL_BLEND);
    if (turbineFlow > 0.10f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int i = 0; i < NTURBINES; i++) {
            float x = TURBINE_XS[i];
            float pulse = 0.6f + 0.4f * sinf(timeAcc * 4.0f + i * 1.1f);
            glColor4f(0.90f, 0.96f, 1.0f, 0.45f * pulse * turbineFlow);
            glPushMatrix();
            glTranslatef(x, tailY + 0.02f, TURBINE_Z + 0.4f);
            glScalef(1.6f, 0.06f, 1.4f);
            sphere(0.9f);
            glPopMatrix();
        }
        glDisable(GL_BLEND);
    }
}

void drawHydroSystem() {
    drawPenstock();
    for (int i = 0; i < NTURBINES; i++) {
        float cx = TURBINE_XS[i];
        float cy = TURBINE_WHEEL_Y;
        float cz = TURBINE_Z;
        if (comparisonShaderProgram) beginComparisonShading(cityWheelPhongOn);
        drawTurbineWheel(cx, cy, cz, turbineWheelAngles[i], turbineFlow,
                         CITY_WHEEL_RADIAL_SCALE);
        if (comparisonShaderProgram) endComparisonShading();
        drawGeneratorUnit(cx, cy, cz, turbineGenAngles[i]);
    }
    for (int gate = 0; gate < NSIDE_CITY_GATES; ++gate) {
        for (int turbine = 0; turbine < NTURBINES; ++turbine) {
            float cx = SIDE_CITY_GATE_XS[gate] + TURBINE_XS[turbine];
            float cy = TURBINE_WHEEL_Y;
            float cz = TURBINE_Z;
            glColor3f(0.42f, 0.42f, 0.46f);
            drawSolidPipe(cx, MANIFOLD_Y, GATE_Z + 2.0f,
                          cx, NOZZLE_EXIT_Y, NOZZLE_Z,
                          PENSTOCK_R, PENSTOCK_R * 0.85f, 12, true);
            if (comparisonShaderProgram) beginComparisonShading(cityWheelPhongOn);
            drawTurbineWheel(cx, cy, cz, sideTurbineWheelAngles[gate][turbine],
                             turbineFlow, CITY_WHEEL_RADIAL_SCALE);
            if (comparisonShaderProgram) endComparisonShading();
            drawGeneratorUnit(cx, cy, cz, sideTurbineGenAngles[gate][turbine]);
        }
    }
    drawGateSpillwaySheet();
    if (gateOpen > 0.05f && turbineFlow > 0.05f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int gate = 0; gate < NSIDE_CITY_GATES; ++gate) {
            float x = SIDE_CITY_GATE_XS[gate];
            float pulse = sinf(timeAcc * 7.0f + gate * 2.0f) * 0.12f;
            glColor4f(0.52f, 0.82f, 0.98f, 0.72f * turbineFlow);
            glBegin(GL_QUADS);
            glNormal3f(0.0f, 0.75f, 0.55f);
            glVertex3f(x - 10.0f, GATE_Y + 0.1f, GATE_Z + 1.2f);
            glVertex3f(x + 10.0f, GATE_Y + 0.1f, GATE_Z + 1.2f);
            glVertex3f(x + 10.0f, TURBINE_WHEEL_Y + pulse, TURBINE_Z - 1.0f);
            glVertex3f(x - 10.0f, TURBINE_WHEEL_Y - pulse, TURBINE_Z - 1.0f);
            glEnd();
        }
        glDisable(GL_BLEND);
    }
    if (turbineFlow > 0.05f) {
        for (int i = 0; i < NTURBINES; i++) {
            float x = TURBINE_XS[i];
            float alpha = 0.90f * turbineFlow;
            drawWaterJetSegment(
                x, NOZZLE_EXIT_Y, NOZZLE_Z,
                x, TURBINE_WHEEL_TOP_Y + 0.05f, TURBINE_Z,
                0.16f, 0.26f, alpha, i * 1.7f);
            drawSplashAt(x, TURBINE_WHEEL_TOP_Y + 0.10f, TURBINE_Z,
                         turbineFlow, i * 2.3f);
        }
        for (int gate = 0; gate < NSIDE_CITY_GATES; ++gate) {
            for (int turbine = 0; turbine < NTURBINES; ++turbine) {
                float x = SIDE_CITY_GATE_XS[gate] + TURBINE_XS[turbine];
                float phase = 8.0f + gate * 3.0f + turbine * 1.7f;
                drawWaterJetSegment(
                    x, NOZZLE_EXIT_Y, NOZZLE_Z,
                    x, TURBINE_WHEEL_TOP_Y + 0.05f, TURBINE_Z,
                    0.16f, 0.26f, 0.90f * turbineFlow, phase);
                drawSplashAt(x, TURBINE_WHEEL_TOP_Y + 0.10f, TURBINE_Z,
                             turbineFlow, phase + 1.0f);
            }
        }
    }
    drawTailracePool();
}

// =====================================================================
//  RIVER  (expanded, rain-modulated waves, barrage-controlled levels)
// =====================================================================
float upstreamSurfaceY() {
    // The full upstream pool reaches just above the barrage deck.
    return RIVER_Y + 1.0f + upstreamLevel * 5.7f;
}
float downstreamSurfaceY() {
    // Both basins share a level scale so equal levels produce zero turbine head.
    float y = RIVER_Y + 1.0f + riverLevel * 5.7f;
    float up = upstreamSurfaceY();
    if (y > up) y = up;
    return y;
}

// Rain-driven surface agitation for the river
struct RiverWaveParams {
    float amp, freq, speed;
    float foamR, foamG, foamB;
};
inline float barragePierX(int i);
inline float barrageGateLeftX(int i);

static RiverWaveParams riverWaves() {
    RiverWaveParams p;
    // Calm when dry, choppy when raining; extra chop if barrage is discharging
    float storm = fmaxf(rainIntensity, barrageFlow * 0.7f);
    p.amp   = 0.05f + storm * 0.55f;             // 0.05 ... 0.60
    p.freq  = 0.35f + storm * 1.6f;               // 0.35 ... 1.95
    p.speed = 0.4f  + storm * 4.0f;               // slow swell -> fast chop
    p.foamR = 0.85f + storm * 0.10f;
    p.foamG = 0.92f + storm * 0.05f;
    p.foamB = 1.00f;
    return p;
}

struct RayVector { float x, y, z; };

static RayVector rayAdd(RayVector a, RayVector b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static RayVector rayScale(RayVector a, float scale) {
    return {a.x * scale, a.y * scale, a.z * scale};
}

static float rayDot(RayVector a, RayVector b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static RayVector rayNormalize(RayVector v) {
    float length = sqrtf(rayDot(v, v));
    if (length < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return rayScale(v, 1.0f / length);
}

static bool intersectRayBox(RayVector origin, RayVector direction,
                            RayVector low, RayVector high, float& distance,
                            RayVector& normal) {
    const float epsilon = 0.001f;
    if (rayDot(direction, direction) < 1e-12f) return false;
    float nearT = -INFINITY;
    float farT = INFINITY;
    RayVector nearNormal = {0, 1, 0};
    RayVector farNormal = {0, 1, 0};
    const float origins[] = {origin.x, origin.y, origin.z};
    const float directions[] = {direction.x, direction.y, direction.z};
    const float minimums[] = {low.x, low.y, low.z};
    const float maximums[] = {high.x, high.y, high.z};
    for (int axis = 0; axis < 3; axis++) {
        if (minimums[axis] >= maximums[axis]) return false;
        if (fabsf(directions[axis]) < 1e-6f) {
            if (origins[axis] < minimums[axis] || origins[axis] > maximums[axis])
                return false;
            continue;
        }
        float t1 = (minimums[axis] - origins[axis]) / directions[axis];
        float t2 = (maximums[axis] - origins[axis]) / directions[axis];
        float sign = -1.0f;
        if (t1 > t2) {
            float swap = t1; t1 = t2; t2 = swap;
            sign = 1.0f;
        }
        RayVector axisNormal = axis == 0 ? RayVector{sign, 0, 0} :
                               axis == 1 ? RayVector{0, sign, 0} : RayVector{0, 0, sign};
        if (t1 > nearT) {
            nearT = t1;
            nearNormal = axisNormal;
        }
        if (t2 < farT) {
            farT = t2;
            farNormal = rayScale(axisNormal, -1.0f);
        }
        if (nearT > farT) return false;
    }
    if (farT < epsilon) return false;
    if (nearT >= epsilon) {
        distance = nearT;
        normal = nearNormal;
    } else {
        distance = farT;
        normal = farNormal;
    }
    return true;
}

static bool intersectRaySphere(RayVector origin, RayVector direction, RayVector center,
                               float radius, float& distance, RayVector& normal) {
    if (radius <= 0.0f) return false;
    float directionLengthSquared = rayDot(direction, direction);
    if (directionLengthSquared < 1e-12f) return false;
    direction = rayScale(direction, 1.0f / sqrtf(directionLengthSquared));
    RayVector offset = {origin.x - center.x, origin.y - center.y, origin.z - center.z};
    float b = rayDot(offset, direction);
    float c = rayDot(offset, offset) - radius*radius;
    float discriminant = b*b - c;
    if (discriminant < 0.0f) return false;
    float t = -b - sqrtf(discriminant);
    if (t < 0.001f) t = -b + sqrtf(discriminant);
    if (t < 0.001f) return false;
    distance = t;
    normal = rayNormalize(rayAdd(offset, rayScale(direction, t)));
    return true;
}

static void traceRiverReflection(RayVector origin, RayVector direction,
                                 float& red, float& green, float& blue) {
    float nearest = 2000.0f;
    RayVector hitNormal = {0, 1, 0};
    float materialR = 0.5f, materialG = 0.5f, materialB = 0.5f;
    bool hit = false;
    for (const Building& building : buildings) {
        RayVector low = {building.x - building.w*0.5f, cityYatZ(building.z),
                         building.z - building.d*0.5f};
        RayVector high = {building.x + building.w*0.5f, low.y + building.h,
                          building.z + building.d*0.5f};
        float t; RayVector n;
        if (intersectRayBox(origin, direction, low, high, t, n) && t < nearest) {
            nearest = t; hitNormal = n;
            materialR = building.r; materialG = building.g; materialB = building.b;
            hit = true;
        }
    }
    for (const Tree& tree : trees) {
        RayVector center = {tree.x, cityYatZ(tree.z) + tree.h*0.85f, tree.z};
        float t; RayVector n;
        if (intersectRaySphere(origin, direction, center, tree.r, t, n) && t < nearest) {
            nearest = t; hitNormal = n;
            materialR = 0.16f; materialG = 0.43f; materialB = 0.18f;
            hit = true;
        }
    }
    for (int i = 0; i <= BARRAGE_NGATES; i++) {
        float x = barragePierX(i);
        RayVector low = {x - BARRAGE_PIER_W*0.5f, BARRAGE_BASE_Y, BARRAGE_Z - 2.0f};
        RayVector high = {x + BARRAGE_PIER_W*0.5f, BARRAGE_DECK_Y + 0.3f, BARRAGE_Z + 3.5f};
        float t; RayVector n;
        if (intersectRayBox(origin, direction, low, high, t, n) && t < nearest) {
            nearest = t; hitNormal = n;
            materialR = 0.56f; materialG = 0.55f; materialB = 0.52f;
            hit = true;
        }
    }
    float gateHeight = BARRAGE_DECK_Y - BARRAGE_BASE_Y;
    float gateBottom = BARRAGE_BASE_Y + barrageGateOpen * (gateHeight - 0.5f);
    for (int i = 0; i < BARRAGE_NGATES; i++) {
        float x0 = barrageGateLeftX(i);
        RayVector low = {x0 + 0.15f, gateBottom, BARRAGE_Z - 0.18f};
        RayVector high = {x0 + BARRAGE_GATE_W - 0.15f, gateBottom + gateHeight, BARRAGE_Z + 0.18f};
        float t; RayVector n;
        if (intersectRayBox(origin, direction, low, high, t, n) && t < nearest) {
            nearest = t; hitNormal = n;
            materialR = 0.38f; materialG = 0.40f; materialB = 0.45f;
            hit = true;
        }
    }

    if (!hit) {
        if (direction.y < -0.08f) {
            red = 0.11f; green = 0.15f; blue = 0.12f;
        } else {
            float horizon = 1.0f - fminf(1.0f, fmaxf(0.0f, direction.y));
            float cloud = cloudCover;
            red = (0.24f*(1.0f-cloud) + 0.48f*cloud) * (1.0f - horizon*0.35f);
            green = (0.48f*(1.0f-cloud) + 0.52f*cloud) * (1.0f - horizon*0.28f);
            blue = (0.82f*(1.0f-cloud) + 0.58f*cloud) * (1.0f - horizon*0.12f);
        }
        return;
    }

    RayVector hitPoint = rayAdd(origin, rayScale(direction, nearest));
    RayVector lightDirection = rayNormalize({40.0f - hitPoint.x, 100.0f - hitPoint.y,
                                              30.0f - hitPoint.z});
    float diffuse = fmaxf(0.0f, rayDot(hitNormal, lightDirection));
    float lightScale = 1.0f - cloudCover * 0.55f;
    float ambientScale = ambientLightingOn ? lightScale : 0.0f;
    float diffuseScale = diffuseLightingOn ? lightScale : 0.0f;
    float specularScale = specularLightingOn ? lightScale : 0.0f;
    RayVector viewDirection = rayScale(direction, -1.0f);
    float specular = 0.0f;
    if (specularScale > 0.0f && diffuse > 0.0f) {
        RayVector reflectedLight = rayNormalize(rayAdd(
            rayScale(hitNormal, 2.0f * rayDot(hitNormal, lightDirection)),
            rayScale(lightDirection, -1.0f)));
        specular = powf(fmaxf(0.0f, rayDot(reflectedLight, viewDirection)), 48.0f) * 0.5f;
    }
    red = materialR * (0.35f * ambientScale + 0.90f * diffuse * diffuseScale) +
          0.90f * specular * specularScale;
    green = materialG * (0.37f * ambientScale + 0.90f * diffuse * diffuseScale) +
            0.92f * specular * specularScale;
    blue = materialB * (0.42f * ambientScale + 0.95f * diffuse * diffuseScale) +
           specular * specularScale;
}

static RayVector riverSurfaceNormal(float x, float z, float yTop, RiverWaveParams wave) {
    const float e = 0.12f;
    float dx = (ripple(x + e, z, timeAcc, wave.amp, wave.freq, wave.speed) -
                ripple(x - e, z, timeAcc, wave.amp, wave.freq, wave.speed)) / (2.0f*e);
    float dz = (ripple(x, z + e, timeAcc, wave.amp, wave.freq, wave.speed) -
                ripple(x, z - e, timeAcc, wave.amp, wave.freq, wave.speed)) / (2.0f*e);
    (void)yTop;
    return rayNormalize({-dx, 1.0f, -dz});
}

static void updateRiverReflectionTexture() {
    if (!riverReflectionTexture) {
        glGenTextures(1, &riverReflectionTexture);
        glBindTexture(GL_TEXTURE_2D, riverReflectionTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    } else {
        glBindTexture(GL_TEXTURE_2D, riverReflectionTexture);
    }

    RiverWaveParams wave = riverWaves();
    float yaw = camYaw * M_PI/180.0f;
    float pitch = camPitch * M_PI/180.0f;
    RayVector eye = {
        camTargetX + camDist*cosf(pitch)*sinf(yaw),
        camTargetY + camDist*sinf(pitch),
        camTargetZ + camDist*cosf(pitch)*cosf(yaw)
    };
    std::vector<unsigned char> pixels(RIVER_REFLECTION_WIDTH * RIVER_REFLECTION_HEIGHT * 3);
    for (int row = 0; row < RIVER_REFLECTION_HEIGHT; row++) {
        float v = (float)row / (RIVER_REFLECTION_HEIGHT - 1);
        float z = RIVER_NEAR + v * (RIVER_FAR - RIVER_NEAR);
        float yTop = z < BARRAGE_Z ? downstreamSurfaceY() : upstreamSurfaceY();
        for (int column = 0; column < RIVER_REFLECTION_WIDTH; column++) {
            float u = (float)column / (RIVER_REFLECTION_WIDTH - 1);
            float x = -120.0f + u * 240.0f;
            float y = yTop + ripple(x, z, timeAcc, wave.amp, wave.freq, wave.speed);
            RayVector point = {x, y, z};
            RayVector view = rayNormalize({eye.x - x, eye.y - y, eye.z - z});
            RayVector normal = riverSurfaceNormal(x, z, yTop, wave);
            RayVector reflected = rayNormalize(rayAdd(rayScale(normal, 2.0f*rayDot(view, normal)),
                                                       rayScale(view, -1.0f)));
            RayVector rayOrigin = rayAdd(point, rayScale(normal, 0.04f));
            float rr, gg, bb;
            traceRiverReflection(rayOrigin, reflected, rr, gg, bb);
            float viewNormal = fminf(1.0f, fmaxf(0.0f, rayDot(view, normal)));
            float fresnel = 0.58f + 0.42f * powf(1.0f - viewNormal, 3.0f);
            float waterR = DEEP_BLUE_R*0.55f + 0.08f;
            float waterG = DEEP_BLUE_G*0.55f + 0.11f;
            float waterB = DEEP_BLUE_B*0.55f + 0.15f;
            rr = waterR*(1.0f-fresnel) + rr*fresnel;
            gg = waterG*(1.0f-fresnel) + gg*fresnel;
            bb = waterB*(1.0f-fresnel) + bb*fresnel;
            int index = (row * RIVER_REFLECTION_WIDTH + column) * 3;
            pixels[index] = (unsigned char)(fminf(1.0f, fmaxf(0.0f, rr)) * 255.0f);
            pixels[index + 1] = (unsigned char)(fminf(1.0f, fmaxf(0.0f, gg)) * 255.0f);
            pixels[index + 2] = (unsigned char)(fminf(1.0f, fmaxf(0.0f, bb)) * 255.0f);
        }
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (riverReflectionTexture) {
        static bool textureAllocated = false;
        if (!textureAllocated) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, RIVER_REFLECTION_WIDTH,
                         RIVER_REFLECTION_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);
            textureAllocated = true;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RIVER_REFLECTION_WIDTH,
                            RIVER_REFLECTION_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);
        }
    }
}

// Draws one river strip with wave amplitude/frequency from rain+flow
void drawRiverStrip(float x1, float z1, float x2, float z2, float yTop,
                    float stormMix, float alpha, float skyTint)
{
    RiverWaveParams P = riverWaves();
    // per-cell, we expand amplitude with a z-based storm progress so
    // the pool near the barrage is choppier than the far end
    const int NX = 48, NZ = 36;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (riverRayTracingOn)
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    GLfloat wspec[] = {0.85f, 0.92f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, wspec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 80.0f);
    for (int i = 0; i < NX; i++) {
        for (int j = 0; j < NZ; j++) {
            float xa = x1 + (x2 - x1) * i / NX;
            float xb = x1 + (x2 - x1) * (i + 1) / NX;
            float za = z1 + (z2 - z1) * j / NZ;
            float zb = z1 + (z2 - z1) * (j + 1) / NZ;
            float ya = yTop + ripple(xa, za, timeAcc, P.amp, P.freq, P.speed);
            float yb = yTop + ripple(xb, za, timeAcc, P.amp, P.freq, P.speed);
            float yc = yTop + ripple(xb, zb, timeAcc, P.amp, P.freq, P.speed);
            float yd = yTop + ripple(xa, zb, timeAcc, P.amp, P.freq, P.speed);
            float nx = (ya - yb) + (yd - yc);
            float nz = (ya - yd) + (yb - yc);
            float ny = (xb - xa) * (zb - za) * 0.5f;
            float len = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-5f;
            nx /= len; ny /= len; nz /= len;
            float avg = (ya + yb + yc + yd) * 0.25f;
            float tH = (avg - yTop) / (P.amp + 1e-5f) * 0.5f + 0.5f;
            float tF = 1.0f - ny; if (tF < 0) tF = 0;
            float t = tH * 0.65f + tF * 0.55f;
            t *= skyTint;
            if (t < 0) t = 0; if (t > 1) t = 1;
            float r, g, b; waterColorAt(t, r, g, b);
            // Deepen colour when calm; brighten with foam when stormy
            r = r * (1 - stormMix*0.15f) + P.foamR * stormMix * 0.15f;
            g = g * (1 - stormMix*0.15f) + P.foamG * stormMix * 0.15f;
            b = b * (1 - stormMix*0.15f) + P.foamB * stormMix * 0.15f;
            glColor4f(r, g, b, alpha);
            glBegin(GL_QUADS);
            glNormal3f(nx, ny, nz);
            glTexCoord2f((xa + 120.0f)/240.0f, (za - RIVER_NEAR)/(RIVER_FAR - RIVER_NEAR));
            glVertex3f(xa, ya, za);
            glTexCoord2f((xb + 120.0f)/240.0f, (za - RIVER_NEAR)/(RIVER_FAR - RIVER_NEAR));
            glVertex3f(xb, yb, za);
            glTexCoord2f((xb + 120.0f)/240.0f, (zb - RIVER_NEAR)/(RIVER_FAR - RIVER_NEAR));
            glVertex3f(xb, yc, zb);
            glTexCoord2f((xa + 120.0f)/240.0f, (zb - RIVER_NEAR)/(RIVER_FAR - RIVER_NEAR));
            glVertex3f(xa, yd, zb);
            glEnd();
            // Whitecap crests — only when stormy
            if (stormMix > 0.25f && tH > 0.78f) {
                float capAlpha = (tH - 0.78f) / 0.22f * (0.30f * stormMix) * alpha;
                glColor4f(0.95f, 0.98f, 1.0f, capAlpha);
                glBegin(GL_QUADS);
                glNormal3f(nx, ny, nz);
                glVertex3f(xa, ya + 0.03f, za);
                glVertex3f(xb, yb + 0.03f, za);
                glVertex3f(xb, yc + 0.03f, zb);
                glVertex3f(xa, yd + 0.03f, zb);
                glEnd();
            }
        }
    }
    GLfloat dspec[] = {0.20f, 0.20f, 0.20f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, dspec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 16.0f);
    if (riverRayTracingOn)
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_BLEND);
}

void drawRiver() {
    if (riverRayTracingOn) updateRiverReflectionTexture();

    // River bed (dark)
    glColor3f(0.16f, 0.14f, 0.11f);
    quadY(-120, RIVER_NEAR, 120, RIVER_FAR, RIVER_Y - 2.5f);

    float downY = downstreamSurfaceY();
    float upY   = upstreamSurfaceY();
    float storm = fmaxf(rainIntensity, barrageFlow * 0.7f);

    if (riverRayTracingOn) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, riverReflectionTexture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    // Downstream pool — visible chop when rain or barrage flow
    drawRiverStrip(-120, RIVER_NEAR, 120, BARRAGE_Z - 2.3f, downY,
                   storm, 0.92f, 1.00f);

    // Upstream pool — calmer (sheltered by barrage), tinted greener
    float upStorm = storm * 0.55f;
    drawRiverStrip(-120, BARRAGE_Z + 3.4f, 120, RIVER_FAR, upY,
                   upStorm, 0.92f, 0.70f);

    if (riverRayTracingOn) glDisable(GL_TEXTURE_2D);

    // The "head" — a visibly different water-colour band upstream of the barrage
    // (a bluish band you can see rising/falling with the reservoir)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.10f, 0.34f, 0.55f, 0.35f);
    drawWaterPlane(-120, BARRAGE_Z + 3.5f, 120, BARRAGE_Z + 8.0f, upY + 0.02f,
                   0.35f, 0.05f, 0.9f, 1.2f, true, false, 0.4f);
    glDisable(GL_BLEND);
}

// =====================================================================
//  BARRAGE
// =====================================================================
inline float barragePierX(int i) {
    return BARRAGE_X0 + BARRAGE_PIER_W*0.5f + i*(BARRAGE_GATE_W + BARRAGE_PIER_W);
}
inline float barrageGateLeftX(int i) {
    return BARRAGE_X0 + BARRAGE_PIER_W + i*(BARRAGE_GATE_W + BARRAGE_PIER_W);
}

static void drawBarrageOverloadSign();

void drawBarragePier(int i) {
    float px = barragePierX(i);
    float cy = (BARRAGE_DECK_Y + BARRAGE_BASE_Y) * 0.5f;
    float h  = BARRAGE_DECK_Y - BARRAGE_BASE_Y;
    float c = cloudCover;
    glColor3f(0.62f - c*0.22f, 0.60f - c*0.22f, 0.57f - c*0.22f);
    glPushMatrix();
    glTranslatef(px, cy, BARRAGE_Z);
    glScalef(BARRAGE_PIER_W, h, 4.0f);
    drawCube(); glPopMatrix();

    glColor3f(0.58f - c*0.20f, 0.56f - c*0.20f, 0.53f - c*0.20f);
    glPushMatrix();
    glTranslatef(px, cy, BARRAGE_Z + 2.35f);
    glScalef(BARRAGE_PIER_W*0.9f, h, 1.2f);
    drawCube(); glPopMatrix();

    glPushMatrix();
    glTranslatef(px, BARRAGE_BASE_Y + 0.8f, BARRAGE_Z + 2.95f);
    glScalef(BARRAGE_PIER_W*0.7f, 1.6f, 1.4f);
    drawCube(); glPopMatrix();

    glColor3f(0.68f - c*0.22f, 0.66f - c*0.22f, 0.62f - c*0.22f);
    glPushMatrix();
    glTranslatef(px, BARRAGE_DECK_Y + 0.15f, BARRAGE_Z);
    glScalef(BARRAGE_PIER_W + 0.3f, 0.4f, 4.5f);
    drawCube(); glPopMatrix();
}

void drawBarrageGate(int i) {
    float gx0 = barrageGateLeftX(i);
    float gx1 = gx0 + BARRAGE_GATE_W;
    float gcx = (gx0 + gx1) * 0.5f;
    float h = BARRAGE_DECK_Y - BARRAGE_BASE_Y;
    float gateBottom = BARRAGE_BASE_Y + barrageGateOpen * (h - 0.5f);
    float gateTop    = gateBottom + h;

    glColor3f(0.48f, 0.48f, 0.52f);
    glPushMatrix();
    glTranslatef(gcx, (gateBottom + gateTop) * 0.5f, BARRAGE_Z);
    glScalef(BARRAGE_GATE_W - 0.3f, gateTop - gateBottom, 0.35f);
    drawCube(); glPopMatrix();

    glColor3f(0.38f, 0.38f, 0.42f);
    int nRib = 5;
    for (int r = 0; r < nRib; r++) {
        float ry = gateBottom + (gateTop - gateBottom) * (r + 1) / (nRib + 1);
        glPushMatrix();
        glTranslatef(gcx, ry, BARRAGE_Z + 0.25f);
        glScalef(BARRAGE_GATE_W - 0.5f, 0.22f, 0.14f);
        drawCube(); glPopMatrix();
    }

    glColor3f(0.30f, 0.30f, 0.33f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(gcx + s*(BARRAGE_GATE_W*0.5f - 0.15f),
                     (gateBottom + gateTop) * 0.5f, BARRAGE_Z);
        glScalef(0.15f, gateTop - gateBottom, 0.5f);
        drawCube(); glPopMatrix();
    }

    glDisable(GL_LIGHTING);
    glColor3f(0.20f, 0.20f, 0.22f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int s = -1; s <= 1; s += 2) {
        glVertex3f(gcx + s*1.5f, gateTop, BARRAGE_Z);
        glVertex3f(gcx + s*1.5f, BARRAGE_DECK_Y + 7.0f, BARRAGE_Z);
    }
    glEnd();
    glLineWidth(1.0f);
    setSceneLightingEnabled();
}

void drawBarrageGantry(int i) {
    float gcx = barrageGateLeftX(i) + BARRAGE_GATE_W * 0.5f;
    float topY = BARRAGE_DECK_Y + 7.2f;
    float c = cloudCover;
    glColor3f(0.42f - c*0.14f, 0.42f - c*0.14f, 0.45f - c*0.14f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(gcx + s*(BARRAGE_GATE_W*0.35f), BARRAGE_DECK_Y + 3.5f, BARRAGE_Z);
        glScalef(0.35f, 7.0f, 0.35f);
        drawCube(); glPopMatrix();
    }
    glPushMatrix();
    glTranslatef(gcx, topY, BARRAGE_Z);
    glScalef(BARRAGE_GATE_W*0.8f, 0.45f, 0.55f);
    drawCube(); glPopMatrix();
    glColor3f(0.82f, 0.72f, 0.18f);
    glPushMatrix();
    glTranslatef(gcx, topY - 0.7f, BARRAGE_Z);
    glScalef(1.4f, 0.7f, 0.9f);
    drawCube(); glPopMatrix();
    glColor3f(0.55f, 0.55f, 0.60f);
    glPushMatrix();
    glTranslatef(gcx, topY - 0.7f, BARRAGE_Z + 0.55f);
    glRotatef(90, 0, 1, 0);
    cylinder(0.20f, 1.6f);
    glPopMatrix();
}

static void drawBarrageOverloadSign() {
    const float signY = BARRAGE_DECK_Y + 4.6f;
    const float signZ = BARRAGE_Z + 3.25f;
    float pulse = 0.72f + 0.28f * (0.5f + 0.5f * sinf(timeAcc * 5.0f));
    bool gateOpeningOrOpen = targetBarrageGate > 0.5f || barrageGateOpen > 0.03f;
    float signalR = gateOpeningOrOpen ? 0.05f : 1.0f;
    float signalG = gateOpeningOrOpen ? 1.0f : 0.08f;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor3f(0.25f, 0.28f, 0.34f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 5.8f, BARRAGE_DECK_Y + 1.7f, signZ);
        glScalef(0.18f, 3.4f, 0.20f);
        drawCube();
        glPopMatrix();
    }

    glColor4f(0.015f, 0.025f, 0.07f, 0.96f);
    glBegin(GL_QUADS);
    glVertex3f(-7.0f, signY - 1.6f, signZ);
    glVertex3f( 7.0f, signY - 1.6f, signZ);
    glVertex3f( 7.0f, signY + 1.6f, signZ);
    glVertex3f(-7.0f, signY + 1.6f, signZ);
    glEnd();

    glLineWidth(7.0f);
    glColor4f(signalR, signalG, 0.02f, 0.28f * pulse);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-7.0f, signY - 1.6f, signZ + 0.02f);
    glVertex3f( 7.0f, signY - 1.6f, signZ + 0.02f);
    glVertex3f( 7.0f, signY + 1.6f, signZ + 0.02f);
    glVertex3f(-7.0f, signY + 1.6f, signZ + 0.02f);
    glEnd();
    glLineWidth(3.0f);
    glColor4f(signalR, signalG, 0.025f, pulse);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-7.0f, signY - 1.6f, signZ + 0.03f);
    glVertex3f( 7.0f, signY - 1.6f, signZ + 0.03f);
    glVertex3f( 7.0f, signY + 1.6f, signZ + 0.03f);
    glVertex3f(-7.0f, signY + 1.6f, signZ + 0.03f);
    glEnd();

    glPushMatrix();
    glTranslatef(-4.75f, signY - 0.45f, signZ + 0.05f);
    glScalef(0.010f, 0.010f, 0.010f);
    glColor3f(signalR, signalG, 0.03f);
    const char* message = gateOpeningOrOpen ? "CLOSE GATE" : "OPEN GATE";
    for (const char* c = message; *c; ++c)
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix();
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    setSceneLightingEnabled();
}

void drawBarrageWheel(float cx, float cy, float cz, float angle, float flow,
                      float radialScale = 1.0f) {
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(1.0f, radialScale, radialScale);

    // Shaft (along X, spans the gate bay)
    glColor3f(0.28f, 0.30f, 0.34f);
    glPushMatrix(); glRotatef(90, 0, 1, 0); cylinder(0.22f, BARRAGE_GATE_W - 0.4f); glPopMatrix();

    // Rounded rims and hubs give the shading comparison smooth surfaces.
    for (int s = -1; s <= 1; s += 2) {
        glColor3f(0.60f, 0.68f, 0.76f);
        glPushMatrix();
        glTranslatef(s * (BARRAGE_GATE_W*0.5f - 0.5f), 0, 0);
        glRotatef(90, 0, 1, 0);
        glutSolidTorus(0.20f, BARRAGE_WHEEL_R * 0.88f, 20, 48);
        glPopMatrix();
    }

    // Rounded paddles spin around the axle and catch a sharp specular highlight.
    glPushMatrix();
    glRotatef(angle, 1, 0, 0);
    const int NB = 12;
    for (int i = 0; i < NB; i++) {
        glPushMatrix();
        glRotatef(i * (360.0f / NB), 1, 0, 0);
        float midR = BARRAGE_WHEEL_R * 0.62f;
        glTranslatef(0, midR, 0);
        float tint = 0.55f + 0.20f * flow;
        glColor3f(tint * 0.72f, tint * 0.86f, tint + 0.12f);
        glPushMatrix();
        glScalef((BARRAGE_GATE_W - 1.4f) * 0.5f, 0.30f, 0.30f);
        glutSolidSphere(1.0, 24, 20);
        glPopMatrix();
        glPopMatrix();
    }
    glPopMatrix();

    // Bright spherical hub makes the per-pixel highlight easy to spot.
    if (flow > 0.15f) {
        float blink = 0.5f + 0.5f * sinf(timeAcc * 8.0f);
        glColor3f(0.90f, 0.75f + blink*0.15f, 0.15f);
    } else {
        glColor3f(0.86f, 0.70f, 0.22f);
    }
    glutSolidSphere(0.58f, 32, 24);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * (BARRAGE_GATE_W*0.5f - 0.5f), 0, 0);
        glutSolidSphere(0.42f, 28, 20);
        glPopMatrix();
    }
    glPopMatrix();
}

void drawBarrageWheelSpokes(float cx, float cy, float cz, float angle,
                            float radialScale = 1.0f) {
    glPushMatrix();
    glTranslatef(cx - BARRAGE_GATE_W*0.5f + 0.5f, cy, cz);
    glScalef(1.0f, radialScale, radialScale);
    glRotatef(-90, 0, 1, 0);
    glRotatef(angle, 1, 0, 0);
    glColor3f(0.66f, 0.75f, 0.84f);
    for (int k = 0; k < 6; k++) {
        glPushMatrix();
        glRotatef(k * 60.0f, 1, 0, 0);
        glTranslatef(0, BARRAGE_WHEEL_R * 0.48f, 0);
        glScalef(0.14f, BARRAGE_WHEEL_R * 0.42f, 0.14f);
        glutSolidSphere(1.0, 20, 16);
        glPopMatrix();
    }
    glPopMatrix();
}

// ---- Realistic falling water:  from gate bottom -> turbine top -> tailrace
void drawBarrageFallingWater() {
    if (barrageGateOpen < 0.03f) return;
    float upY   = upstreamSurfaceY();
    float downY = downstreamSurfaceY();

    float h = BARRAGE_DECK_Y - BARRAGE_BASE_Y;
    float gateBottom = BARRAGE_BASE_Y + barrageGateOpen * (h - 0.5f);

    // Where the water starts (under the gate) and ends (tailrace)
    float sheetTop = fminf(gateBottom, upY - 0.05f);
    float sheetBot = downY - 0.05f;
    if (sheetTop <= sheetBot) return;

    float wheelTop = BARRAGE_WHEEL_Y + BARRAGE_WHEEL_R * BARRAGE_WHEEL_SCALE;
    float flow = barrageFlow;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < BARRAGE_NGATES; i++) {
        float gx0 = barrageGateLeftX(i) + 0.3f;
        float gx1 = gx0 + BARRAGE_GATE_W - 0.6f;

        // ----- (A) Sheet from gate bottom to turbine top -----
        float midY = wheelTop + 0.05f;
        if (sheetTop > midY) {
            const int COLS = 16;
            const int ROWS = 10;
            for (int cx = 0; cx < COLS; cx++) {
                float xa = gx0 + (gx1-gx0)*cx/COLS;
                float xb = gx0 + (gx1-gx0)*(cx+1)/COLS;
                for (int cy = 0; cy < ROWS; cy++) {
                    float t0 = (float)cy / ROWS;
                    float t1 = (float)(cy+1) / ROWS;
                    float ya = sheetTop + (midY - sheetTop) * t0;
                    float yb = sheetTop + (midY - sheetTop) * t1;
                    float wobA = 0.12f * sinf(xa*2.2f + timeAcc*11.0f);
                    float wobB = 0.12f * sinf(xb*2.2f + timeAcc*11.0f);
                    float streakA = 0.5f + 0.5f * sinf(xa*3.5f + ya*5.0f + timeAcc*15.0f);
                    float streakB = 0.5f + 0.5f * sinf(xb*3.5f + ya*5.0f + timeAcc*15.0f);
                    float aA = (0.70f + 0.25f * streakA) * barrageGateOpen * flow;
                    float aB = (0.70f + 0.25f * streakB) * barrageGateOpen * flow;
                    float zBase = BARRAGE_Z - 0.15f;
                    glColor4f(0.55f + streakA*0.30f, 0.82f + streakA*0.12f, 0.98f, aA);
                    glBegin(GL_QUADS);
                    glNormal3f(0, 0, -1);
                    glVertex3f(xa, ya, zBase + wobA);
                    glVertex3f(xb, ya, zBase + wobB);
                    glVertex3f(xb, yb, zBase + wobB);
                    glVertex3f(xa, yb, zBase + wobA);
                    glEnd();
                }
            }
        }

        // ----- (B) Impact splash on turbine (particles) -----
        if (flow > 0.10f && wheelTop < sheetTop) {
            const int NP = 32;
            for (int k = 0; k < NP; k++) {
                float seed = k * 5.71f + i * 1.3f;
                float life = fmodf(timeAcc * 1.5f + seed, 1.0f);
                float speed = 2.2f + fmodf(seed*1.7f, 1.0f) * 2.5f;
                float ang = fmodf(seed * 4.31f, 1.0f) * 2.0f * M_PI;
                float elev = 1.0f + fmodf(seed*2.11f, 1.0f) * 1.8f;
                float r = life * speed * 0.65f;
                float px = gx0 + (gx1-gx0) * 0.5f + cosf(ang) * r;
                float pz = BARRAGE_WHEEL_Z + sinf(ang) * r * 0.55f;
                float py = wheelTop + elev * life - 2.6f * life * life;
                float alpha = (1.0f - life) * 0.75f * flow;
                glColor4f(0.85f, 0.94f, 1.00f, alpha);
                glPushMatrix();
                glTranslatef(px, py, pz);
                sphere(0.09f * (1.0f - life * 0.55f));
                glPopMatrix();
            }
            // foam burst at impact line
            glColor4f(0.95f, 0.99f, 1.00f, 0.65f * flow);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(gx0, wheelTop + 0.06f, BARRAGE_WHEEL_Z - 0.7f);
            glVertex3f(gx1, wheelTop + 0.06f, BARRAGE_WHEEL_Z - 0.7f);
            glVertex3f(gx1, wheelTop + 0.06f, BARRAGE_WHEEL_Z + 0.7f);
            glVertex3f(gx0, wheelTop + 0.06f, BARRAGE_WHEEL_Z + 0.7f);
            glEnd();
        }

        // ----- (C) Sheet from turbine bottom to tailrace -----
        float wheelBot = BARRAGE_WHEEL_Y - BARRAGE_WHEEL_R * BARRAGE_WHEEL_SCALE;
        if (wheelBot > downY && flow > 0.05f) {
            const int COLS = 16;
            const int ROWS = 8;
            for (int cx = 0; cx < COLS; cx++) {
                float xa = gx0 + (gx1-gx0)*cx/COLS;
                float xb = gx0 + (gx1-gx0)*(cx+1)/COLS;
                for (int cy = 0; cy < ROWS; cy++) {
                    float t0 = (float)cy / ROWS;
                    float t1 = (float)(cy+1) / ROWS;
                    float ya = wheelBot + (downY - wheelBot) * t0;
                    float yb = wheelBot + (downY - wheelBot) * t1;
                    float wobA = 0.18f * sinf(xa*1.8f + timeAcc*8.0f);
                    float wobB = 0.18f * sinf(xb*1.8f + timeAcc*8.0f);
                    float streakA = 0.5f + 0.5f * sinf(xa*2.5f + ya*3.5f + timeAcc*12.0f);
                    float aA = (0.55f + 0.25f * streakA) * flow;
                    float zBase = BARRAGE_WHEEL_Z + 0.35f;
                    glColor4f(0.60f + streakA*0.25f, 0.85f + streakA*0.10f, 0.98f, aA);
                    glBegin(GL_QUADS);
                    glNormal3f(0, 0, 1);
                    glVertex3f(xa, ya, zBase + wobA);
                    glVertex3f(xb, ya, zBase + wobB);
                    glVertex3f(xb, yb, zBase + wobB);
                    glVertex3f(xa, yb, zBase + wobA);
                    glEnd();
                }
            }
        }
    }
    glDisable(GL_BLEND);
}

void drawBarrageStillingBasin() {
    if (barrageFlow < 0.05f) return;
    float downY = downstreamSurfaceY();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float z0 = BARRAGE_Z - 9.5f;
    float z1 = BARRAGE_Z - 1.8f;
    const int SX = 48, SZ = 10;
    for (int ix = 0; ix < SX; ix++) {
        float xa = BARRAGE_X0 + BARRAGE_SPAN * ix / SX;
        float xb = BARRAGE_X0 + BARRAGE_SPAN * (ix+1) / SX;
        for (int iz = 0; iz < SZ; iz++) {
            float za = z0 + (z1 - z0) * iz / SZ;
            float zb = z0 + (z1 - z0) * (iz+1) / SZ;
            float churn = 0.20f + 0.35f * barrageFlow;
            float y00 = downY + churn * (0.5f + 0.5f * sinf(xa*3.0f + za*4.0f - timeAcc*9.0f));
            float y10 = downY + churn * (0.5f + 0.5f * sinf(xb*3.0f + za*4.0f - timeAcc*9.0f));
            float y11 = downY + churn * (0.5f + 0.5f * sinf(xb*3.0f + zb*4.0f - timeAcc*9.0f));
            float y01 = downY + churn * (0.5f + 0.5f * sinf(xa*3.0f + zb*4.0f - timeAcc*9.0f));
            float foam = 0.5f + 0.5f * sinf(xa*4.0f + za*3.0f + timeAcc*13.0f);
            glColor4f(0.75f + foam*0.22f, 0.88f + foam*0.10f, 1.0f, 0.60f * barrageFlow);
            glBegin(GL_QUADS);
            glNormal3f(0,1,0);
            glVertex3f(xa, y00, za); glVertex3f(xb, y10, za);
            glVertex3f(xb, y11, zb); glVertex3f(xa, y01, zb);
            glEnd();
        }
    }
    glDisable(GL_BLEND);
}

void drawBarrageStructure() {
    float c = cloudCover;
    glColor3f(0.30f - c*0.10f, 0.30f - c*0.10f, 0.28f - c*0.10f);
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);
    float ax0 = BARRAGE_X0 - 4.0f, ax1 = BARRAGE_X0 + BARRAGE_SPAN + 4.0f;
    glVertex3f(ax0, BARRAGE_BASE_Y - 0.05f, BARRAGE_Z - 9.0f);
    glVertex3f(ax1, BARRAGE_BASE_Y - 0.05f, BARRAGE_Z - 9.0f);
    glVertex3f(ax1, BARRAGE_BASE_Y - 0.05f, BARRAGE_Z + 6.0f);
    glVertex3f(ax0, BARRAGE_BASE_Y - 0.05f, BARRAGE_Z + 6.0f);
    glEnd();

    for (int i = 0; i <= BARRAGE_NGATES; i++) drawBarragePier(i);
    for (int i = 0; i < BARRAGE_NGATES; i++) drawBarrageGate(i);

    glColor3f(0.58f - c*0.20f, 0.56f - c*0.20f, 0.53f - c*0.20f);
    glPushMatrix();
    glTranslatef(0, BARRAGE_DECK_Y + 0.35f, BARRAGE_Z);
    glScalef(BARRAGE_SPAN + 2.0f, 0.5f, 5.5f);
    drawCube(); glPopMatrix();

    glColor3f(0.35f, 0.35f, 0.38f);
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(0, BARRAGE_DECK_Y + 1.1f, BARRAGE_Z + s*2.7f);
        glScalef(BARRAGE_SPAN + 2.0f, 0.08f, 0.08f);
        drawCube(); glPopMatrix();
    }
    for (float x = BARRAGE_X0; x <= BARRAGE_X0 + BARRAGE_SPAN; x += 4.0f) {
        for (int s = -1; s <= 1; s += 2) {
            glPushMatrix();
            glTranslatef(x, BARRAGE_DECK_Y + 0.85f, BARRAGE_Z + s*2.7f);
            glScalef(0.10f, 1.0f, 0.10f);
            drawCube(); glPopMatrix();
        }
    }

    for (int i = 0; i < BARRAGE_NGATES; i++) drawBarrageGantry(i);

    glColor3f(0.68f - c*0.22f, 0.66f - c*0.22f, 0.62f - c*0.22f);
    glPushMatrix();
    glTranslatef(BARRAGE_X0 + BARRAGE_SPAN + 6.0f, BARRAGE_DECK_Y + 3.0f, BARRAGE_Z);
    glScalef(6.0f, 6.0f, 5.0f);
    drawCube(); glPopMatrix();
    glColor3f(0.50f - c*0.18f, 0.50f - c*0.18f, 0.52f - c*0.18f);
    glPushMatrix();
    glTranslatef(BARRAGE_X0 + BARRAGE_SPAN + 6.0f, BARRAGE_DECK_Y + 6.3f, BARRAGE_Z);
    glScalef(6.6f, 0.5f, 5.6f);
    drawCube(); glPopMatrix();
    glDisable(GL_LIGHTING);
    glColor3f(0.55f, 0.72f, 0.90f);
    glPushMatrix();
    glTranslatef(BARRAGE_X0 + BARRAGE_SPAN + 6.0f, BARRAGE_DECK_Y + 3.6f, BARRAGE_Z - 2.55f);
    glScalef(4.5f, 1.2f, 0.10f);
    drawCube(); glPopMatrix();
    setSceneLightingEnabled();

    for (int i = 0; i < 8; i++) {
        float fy = BARRAGE_BASE_Y + 0.4f + i * (upstreamSurfaceY() - BARRAGE_BASE_Y) / 8.0f;
        float fx = BARRAGE_X0 - 5.0f;
        float fz = BARRAGE_Z - 3.0f + i * 0.6f;
        glColor3f(0.50f - c*0.18f, 0.50f - c*0.18f, 0.48f - c*0.18f);
        glPushMatrix();
        glTranslatef(fx, fy, fz);
        glScalef(3.0f, 0.5f, 1.8f);
        drawCube(); glPopMatrix();
    }
    if (barrageOverloaded || targetBarrageGate > 0.5f || barrageGateOpen > 0.03f)
        drawBarrageOverloadSign();
}

void drawBarrageWheels() {
    for (int i = 0; i < BARRAGE_NGATES; i++) {
        float gcx = barrageGateLeftX(i) + BARRAGE_GATE_W * 0.5f;
        bool gouraudWheel = i < BARRAGE_NGATES / 2;
        bool useGouraud = wheelGouraudOn && (!wheelPhongOn || gouraudWheel);
        bool usePhong = wheelPhongOn && (!wheelGouraudOn || !gouraudWheel);
        bool shadingEnabled = comparisonShaderProgram && (useGouraud || usePhong);
        if (shadingEnabled) beginComparisonShading(usePhong);
        drawBarrageWheel(gcx, BARRAGE_WHEEL_Y, BARRAGE_WHEEL_Z,
                         barrageWheelAngles[i], barrageFlow, BARRAGE_WHEEL_SCALE);
        drawBarrageWheelSpokes(gcx, BARRAGE_WHEEL_Y, BARRAGE_WHEEL_Z,
                               barrageWheelAngles[i], BARRAGE_WHEEL_SCALE);
        if (shadingEnabled) endComparisonShading();
    }
}

void drawBarrage() {
    drawBarrageStructure();
    drawBarrageWheels();
    drawBarrageFallingWater();
    drawBarrageStillingBasin();
}

// =====================================================================
//  POWER LINES
// =====================================================================
void drawPowerLines() {
    glDisable(GL_LIGHTING);
    if (powerOutput > 0.05f) glColor3f(1.0f, 0.85f, 0.30f);
    else                     glColor3f(0.12f, 0.12f, 0.12f);
    glLineWidth(1.6f);
    const float poleX = -6.9f;
    const float wireXs[] = { -5.8f, -4.6f, -3.4f };
    const float poleZs[] = { 20.0f, 12.0f, 1.0f, -10.0f, -22.0f, -38.0f, -48.0f };
    const int poleCount = sizeof(poleZs) / sizeof(poleZs[0]);
    const float poleHeight = 15.0f;

    glColor3f(0.30f, 0.25f, 0.18f);
    for (int i = 0; i < poleCount; i++) {
        float z = poleZs[i];
        float baseY = cityYatZ(z);
        glPushMatrix();
        glTranslatef(poleX, baseY + poleHeight * 0.5f, z);
        glScalef(0.24f, poleHeight, 0.24f);
        drawCube(); glPopMatrix();
        glPushMatrix();
        glTranslatef((poleX + wireXs[2]) * 0.5f, baseY + poleHeight - 0.5f, z);
        glScalef(wireXs[2] - poleX + 0.35f, 0.22f, 0.32f);
        drawCube(); glPopMatrix();
    }
    if (powerOutput > 0.05f) glColor3f(1.0f, 0.85f, 0.30f);
    else                     glColor3f(0.12f, 0.12f, 0.12f);
    for (int wire = 0; wire < 3; wire++) {
        for (int span = 0; span < poleCount - 1; span++) {
            float z0 = poleZs[span];
            float z1 = poleZs[span + 1];
            float y0 = cityYatZ(z0) + poleHeight - 0.5f;
            float y1 = cityYatZ(z1) + poleHeight - 0.5f;
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= 12; i++) {
                float t = (float)i / 12.0f;
                float z = z0 + (z1 - z0) * t;
                float y = y0 + (y1 - y0) * t - sinf(t * M_PI) * 0.55f;
                glVertex3f(wireXs[wire], y, z);
            }
            glEnd();
        }
    }
    glLineWidth(1.0f);
    setSceneLightingEnabled();
}

// =====================================================================
//  BUILDINGS / TREES / STREETLIGHTS
// =====================================================================
void drawBuilding(const Building& b) {
    float c = cloudCover;
    float baseY = cityYatZ(b.z);
    bool isMall = (b.type == 3);
    float cr = b.r * (1 - c*0.35f);
    float cg = b.g * (1 - c*0.35f);
    float cb = b.b * (1 - c*0.35f);
    glColor3f(cr, cg, cb);
    glPushMatrix();
    glTranslatef(b.x, baseY + b.h/2.0f, b.z);
    glScalef(b.w, b.h, b.d); drawCube(); glPopMatrix();
    glColor3f(cr*0.85f, cg*0.85f, cb*0.85f);
    glPushMatrix();
    glTranslatef(b.x, baseY + b.h + 0.35f, b.z);
    glScalef(b.w + 0.4f, 0.7f, b.d + 0.4f); drawCube(); glPopMatrix();
    glDisable(GL_LIGHTING);
    if (isMall) {
        float on = lightIntensity;
        float gl_r = 0.015f + 0.95f*on;
        float gl_g = 0.025f + 0.55f*on;
        float gl_b = 0.045f + 0.20f*on;
        glColor3f(gl_r, gl_g, gl_b);
        glPushMatrix();
        glTranslatef(b.x, baseY + b.h*0.55f, b.z + b.d*0.5f + 0.06f);
        glScalef(b.w * 0.92f, b.h * 0.55f, 0.10f); drawCube(); glPopMatrix();
        glColor3f(0.015f + on, 0.02f + on*0.20f, 0.04f + on*0.75f);
        glPushMatrix();
        glTranslatef(b.x, baseY + b.h*0.55f, b.z - b.d*0.5f - 0.06f);
        glScalef(b.w * 0.92f, b.h * 0.55f, 0.10f); drawCube(); glPopMatrix();
    }
    float floorH = b.h / b.floors;
    for (int f = 0; f < b.floors; f++) {
        float yBase = baseY + f * floorH + floorH * 0.30f;
        float yTop  = yBase + floorH * 0.45f;
        int segs = (int)(b.w / 2.2f); if (segs < 1) segs = 1;
        for (int s = 0; s < segs; s++) {
            float stagger = fmodf((float)(abs((int)(b.x*10.0f)) + f*7 + s*13 +
                                             b.type*17) * 0.137f, 0.48f);
            float on = lightIntensity > stagger
                ? (lightIntensity - stagger) / (1.0f - stagger)
                : 0.0f;
            float warm = 0.75f + 0.25f * sinf((float)(s + f*3 + b.type*5));
            glColor3f(0.015f + on,
                      0.020f + on * (0.58f + 0.30f*warm),
                      0.035f + on * (0.18f + 0.20f*(1.0f-warm)));
            float xw = b.x - b.w*0.5f + 0.8f + s * (b.w / segs);
            glPushMatrix();
            glTranslatef(xw + 0.6f, (yBase+yTop)*0.5f, b.z + b.d*0.5f + 0.05f);
            glScalef(b.w/segs - 0.7f, yTop - yBase, 0.08f); drawCube(); glPopMatrix();
            glPushMatrix();
            glTranslatef(xw + 0.6f, (yBase+yTop)*0.5f, b.z - b.d*0.5f - 0.05f);
            glScalef(b.w/segs - 0.7f, yTop - yBase, 0.08f); drawCube(); glPopMatrix();
        }
    }
    setSceneLightingEnabled();
}
void drawBuildings() { for (auto& b : buildings) drawBuilding(b); }

void drawTree(const Tree& t) {
    float baseY = cityYatZ(t.z);
    glColor3f(0.32f, 0.20f, 0.10f);
    glPushMatrix();
    glTranslatef(t.x, baseY + t.h * 0.35f, t.z);
    glScalef(0.22f, t.h * 0.7f, 0.22f); drawCube(); glPopMatrix();
    glColor3f(0.15f, 0.42f, 0.16f);
    glPushMatrix(); glTranslatef(t.x, baseY + t.h * 0.85f, t.z); sphere(t.r); glPopMatrix();
    glColor3f(0.20f, 0.52f, 0.20f);
    glPushMatrix(); glTranslatef(t.x + 0.2f, baseY + t.h * 0.75f, t.z - 0.15f); sphere(t.r * 0.8f); glPopMatrix();
    glPushMatrix(); glTranslatef(t.x - 0.2f, baseY + t.h * 0.95f, t.z + 0.1f); sphere(t.r * 0.75f); glPopMatrix();
}
void drawTrees() { for (auto& t : trees) drawTree(t); }

struct LightColor { float r, g, b; };
const LightColor LIGHT_PALETTE[6] = {
    {1.00f, 1.00f, 1.00f}, {1.00f, 0.85f, 0.30f}, {0.30f, 0.60f, 1.00f},
    {1.00f, 0.20f, 0.25f}, {0.30f, 1.00f, 0.45f}, {1.00f, 0.55f, 0.90f},
};
void drawStreetlight(float x, float z, const LightColor& col) {
    float baseY = cityYatZ(z);
    glColor3f(0.22f, 0.22f, 0.24f);
    glPushMatrix(); glTranslatef(x, baseY + 2.5f, z); glScalef(0.16f, 5.0f, 0.16f); drawCube(); glPopMatrix();
    glPushMatrix(); glTranslatef(x + 0.7f, baseY + 4.9f, z); glScalef(1.4f, 0.14f, 0.14f); drawCube(); glPopMatrix();
    glPushMatrix(); glTranslatef(x + 1.4f, baseY + 4.8f, z); glScalef(0.55f, 0.25f, 0.35f); drawCube(); glPopMatrix();
    float on = lightIntensity;
    if (on < 0.05f) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(col.r, col.g, col.b, 1.0f * on);
    glPushMatrix(); glTranslatef(x + 1.4f, baseY + 4.7f, z); sphere(0.38f); glPopMatrix();
    glColor4f(col.r, col.g, col.b, 0.35f * on);
    glPushMatrix(); glTranslatef(x + 1.4f, baseY + 4.7f, z); sphere(0.85f); glPopMatrix();
    glColor4f(col.r, col.g, col.b, 0.18f * on);
    glPushMatrix(); glTranslatef(x + 1.4f, baseY + 4.7f, z); sphere(1.6f); glPopMatrix();
    glColor4f(col.r, col.g, col.b, 0.35f * on);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(x + 1.4f, baseY + 0.08f, z);
    for (int i = 0; i <= 24; i++) {
        float a = i * 2.0f * M_PI / 24.0f;
        glVertex3f(x + 1.4f + cosf(a)*5.0f, baseY + 0.08f, z + sinf(a)*5.0f);
    }
    glEnd();
    glDisable(GL_BLEND);
}
void drawStreetlights() {
    int idx = 0;
    for (float z = -135.0f; z <= 8.0f; z += 12.0f) {
        drawStreetlight(-44.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(-32.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
    }
    for (float z = -135.0f; z <= 8.0f; z += 12.0f) {
        drawStreetlight(-6.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight( 6.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
    }
    for (float z = -135.0f; z <= 8.0f; z += 12.0f) {
        drawStreetlight(-70.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight( 70.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(-94.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight( 94.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
    }
    for (float z = -135.0f; z <= 8.0f; z += 12.0f) {
        drawStreetlight( 32.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight( 44.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
    }
    for (float x = -110.0f; x <= 110.0f; x += 22.0f) {
        drawStreetlight(x, -34.0f, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(x, -22.0f, LIGHT_PALETTE[idx % 6]); idx++;
    }
    for (float x = -110.0f; x <= 110.0f; x += 22.0f) {
        drawStreetlight(x, -10.0f, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(x,   2.0f, LIGHT_PALETTE[idx % 6]); idx++;
    }
    for (float x = -110.0f; x <= 110.0f; x += 22.0f) {
        drawStreetlight(x, -58.0f, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(x, -82.0f, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(x, -106.0f, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight(x, -130.0f, LIGHT_PALETTE[idx % 6]); idx++;
    }
}

void drawRain() {
    if (rainIntensity < 0.03f) return;
    glDisable(GL_LIGHTING);
    glColor4f(0.72f, 0.82f, 0.95f, 0.60f * rainIntensity);
    glLineWidth(1.3f);
    glBegin(GL_LINES);
    for (auto& r : rainDrops) {
        glVertex3f(r.x, r.y, r.z);
        glVertex3f(r.x + windX*0.25f, r.y - 1.6f, r.z + 0.15f);
    }
    glEnd();
    glLineWidth(1.0f);
    setSceneLightingEnabled();
}

// =====================================================================
//  HUD
// =====================================================================
void drawHUD() {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, 1100, 0, 750, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glColor3f(1,1,1);
    auto text = [](float x, float y, const char* s) {
        glRasterPos2f(x, y);
        while (*s) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *s++);
    };
    char buf[300];
    glColor4f(0,0,0,0.55f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glVertex2f(8, 630); glVertex2f(520, 630); glVertex2f(520, 740); glVertex2f(8, 740);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(1,1,1);
    snprintf(buf, sizeof(buf), "Rain %s (%.2f)  Clouds %.2f  Wind %.2f",
             rainOn?"ON":"OFF", rainIntensity, cloudCover, windX);
    text(14, 722, buf);
    glColor3f(SKY_BLUE_R, SKY_BLUE_G, SKY_BLUE_B);
    float upY = upstreamSurfaceY();
    float dnY = downstreamSurfaceY();
    snprintf(buf, sizeof(buf), "UPSTREAM %+.2f   DOWNSTREAM %+.2f   HEAD %.2f",
             upY, dnY, upY - dnY);
    text(14, 704, buf);
    glColor3f(1,1,1);
    snprintf(buf, sizeof(buf), "Upstream pool: %5.1f%%    Downstream lvl: %5.1f%%",
             upstreamLevel * 100.0f, riverLevel * 100.0f);
    text(14, 686, buf);
    if (barrageFlow > 0.15f) glColor3f(0.4f, 1.0f, 0.4f);
    else                     glColor3f(1.0f, 0.7f, 0.4f);
    snprintf(buf, sizeof(buf), "BARRAGE GATE %.2f   FLOW %.2f   %d/%d wheels turning",
             barrageGateOpen, barrageFlow,
             (int)(barrageFlow * BARRAGE_NGATES), BARRAGE_NGATES);
    text(14, 668, buf);
    if (powerOutput < 0.1f && barragePower < 0.1f) glColor3f(1.0f, 0.5f, 0.4f);
    else                                            glColor3f(0.5f, 1.0f, 0.5f);
    float totalMW = powerOutput * NCITY_TURBINES * 30.0f + barragePower * BARRAGE_NGATES * 20.0f;
    snprintf(buf, sizeof(buf), "POWER: %6.1f MW   Pelton: %.0f%%   Barrage: %.0f%%   Lights: %.2f",
             totalMW, powerOutput*100.0f, barragePower*100.0f, lightIntensity);
    text(14, 650, buf);
    glEnable(GL_DEPTH_TEST); setSceneLightingEnabled();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

static void drawTechniqueButton(float x, float y, float width, float height,
                                const char* label, bool selected) {
    if (selected) glColor4f(0.08f, 0.48f, 0.68f, 0.98f);
    else glColor4f(0.18f, 0.23f, 0.30f, 0.96f);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + width, y);
    glVertex2f(x + width, y + height); glVertex2f(x, y + height);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    float textX = x + (width - (float)strlen(label) * 8.0f) * 0.5f;
    float textY = y + (height - 13.0f) * 0.5f + 2.0f;
    glRasterPos2f(textX, textY);
    for (const char* c = label; *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
}

static void drawRenderTechniqueControls() {
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                 GL_TEXTURE_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1100, 0, 750, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!showRenderControls) {
        drawTechniqueButton(932, 706, 160, 30, "CONTROLS +", false);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
        return;
    }

    glColor4f(0.035f, 0.055f, 0.085f, 0.94f);
    glBegin(GL_QUADS);
    glVertex2f(808, 288); glVertex2f(1092, 288);
    glVertex2f(1092, 742); glVertex2f(808, 742);
    glEnd();
    glColor3f(0.92f, 0.96f, 1.0f);
    glRasterPos2f(820, 721);
    const char* title = "SCENE CONTROLS";
    for (const char* c = title; *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c);
    drawTechniqueButton(990, 710, 90, 25, "HIDE", false);

    const float leftX = 818.0f, rightX = 954.0f, buttonW = 128.0f;
    const float buttonH = 28.0f;
    drawTechniqueButton(leftX, 681, buttonW, buttonH, ambientLightingOn ? "AMBIENT: ON" : "AMBIENT: OFF", ambientLightingOn);
    drawTechniqueButton(rightX, 681, buttonW, buttonH, diffuseLightingOn ? "DIFFUSE: ON" : "DIFFUSE: OFF", diffuseLightingOn);
    drawTechniqueButton(leftX, 639, buttonW, buttonH, specularLightingOn ? "SPECULAR: ON" : "SPECULAR: OFF", specularLightingOn);
    drawTechniqueButton(rightX, 639, buttonW, buttonH, wheelGouraudOn ? "GOURAUD: ON" : "GOURAUD: OFF", wheelGouraudOn);
    drawTechniqueButton(leftX, 597, buttonW, buttonH, wheelPhongOn ? "PHONG: ON" : "PHONG: OFF", wheelPhongOn);
    drawTechniqueButton(rightX, 597, buttonW, buttonH, riverRayTracingOn ? "RAY TRACE: ON" : "RAY TRACE: OFF", riverRayTracingOn);
    drawTechniqueButton(leftX, 555, buttonW, buttonH,
                        targetBarrageGate > 0.5f ? "CLOSE BARRAGE" : "OPEN BARRAGE",
                        targetBarrageGate > 0.5f || barrageOverloaded);
    drawTechniqueButton(rightX, 555, buttonW, buttonH,
                        targetGate > 0.5f ? "CLOSE CITY GATE" : "OPEN CITY GATE",
                        targetGate > 0.5f);
    drawTechniqueButton(leftX, 513, buttonW, buttonH, rainOn ? "RAIN: ON" : "RAIN: OFF", rainOn);
    drawTechniqueButton(rightX, 513, buttonW, buttonH,
                        targetCloud > 0.5f ? "CLOUDS: ON" : "CLOUDS: OFF",
                        targetCloud > 0.5f);
    drawTechniqueButton(leftX, 471, buttonW, buttonH, "CALM PRESET", false);
    drawTechniqueButton(rightX, 471, buttonW, buttonH, "HIGH POOL", targetUpstream > 0.82f);
    drawTechniqueButton(leftX, 429, buttonW, buttonH, "RESET SCENE", false);
    drawTechniqueButton(rightX, 429, buttonW, buttonH, "FOCUS WHEELS", false);
    drawTechniqueButton(leftX, 387, buttonW, buttonH,
                        comparisonShaderProgram
                            ? (cityWheelPhongOn ? "CITY: PHONG" : "CITY: GOURAUD")
                            : "CITY SHADER N/A",
                        comparisonShaderProgram && !cityWheelPhongOn);
    drawTechniqueButton(rightX, 387, buttonW, buttonH, "HUD STATUS", showHUD);
    drawTechniqueButton(leftX, 345, buttonW, buttonH, "RESET VIEW", false);
    drawTechniqueButton(rightX, 345, buttonW, buttonH, "POOL LOW", false);
    drawTechniqueButton(leftX, 303, buttonW, buttonH, "SEE CLOUDS", false);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

static bool handleRenderTechniqueClick(int mouseX, int mouseY) {
    float x = (float)mouseX * 1100.0f / (float)windowWidth;
    float y = 750.0f - (float)mouseY * 750.0f / (float)windowHeight;
    if (x >= 932.0f && x <= 1092.0f && y >= 706.0f && y <= 742.0f) {
        showRenderControls = !showRenderControls;
        return true;
    }
    if (!showRenderControls)
        return false;
    if (x < 808.0f || x > 1092.0f || y < 288.0f || y > 742.0f)
        return false;
    float leftX = 818.0f, rightX = 954.0f;
    bool leftColumn = x >= leftX && x <= leftX + 128.0f;
    bool rightColumn = x >= rightX && x <= rightX + 128.0f;
    if (!leftColumn && !rightColumn) return true;
    int row = -1;
    const float rowY[] = {681, 639, 597, 555, 513, 471, 429, 387, 345, 303};
    for (int i = 0; i < 10; ++i) {
        if (y >= rowY[i] && y <= rowY[i] + 28) { row = i; break; }
    }
    if (row < 0) return true;
    bool left = leftColumn;
    switch (row) {
        case 0: if (left) ambientLightingOn = !ambientLightingOn; else diffuseLightingOn = !diffuseLightingOn; break;
        case 1: if (left) specularLightingOn = !specularLightingOn; else wheelGouraudOn = !wheelGouraudOn; break;
        case 2:
            if (left) wheelPhongOn = !wheelPhongOn;
            else riverRayTracingOn = !riverRayTracingOn;
            break;
        case 3:
            if (left) {
                if (targetBarrageGate > 0.5f) setBarrageGate(false);
                else if (barrageOverloaded) setBarrageGate(true);
            } else targetGate = targetGate > 0.5f ? 0.0f : 1.0f;
            break;
        case 4:
            if (left) {
                rainOn = !rainOn;
                targetRain = rainOn ? 1.0f : 0.0f;
                targetCloud = rainOn ? 0.95f : 0.10f;
            } else targetCloud = targetCloud > 0.5f ? 0.10f : 0.95f;
            break;
        case 5:
            if (left) {
                rainOn = false; targetRain = 0.0f; targetCloud = 0.10f;
                targetDrain = DRAIN_BASE_WATER; targetRoad = 0.0f;
                targetRiver = 0.05f; targetUpstream = 0.82f;
                targetGate = 0.0f; setBarrageGate(false);
            } else targetUpstream = 0.85f;
            break;
        case 6:
            if (left) {
                camYaw = 30.0f; camPitch = 32.0f; camDist = 220.0f;
                camTargetX = 0.0f; camTargetY = 0.0f; camTargetZ = 20.0f;
            } else {
                camYaw = 18.0f; camPitch = 18.0f; camDist = 52.0f;
                camTargetX = 0.0f; camTargetY = TURBINE_WHEEL_Y; camTargetZ = TURBINE_Z;
            }
            break;
        case 7: if (left && comparisonShaderProgram) cityWheelPhongOn = !cityWheelPhongOn; else if (!left) showHUD = !showHUD; break;
        case 8:
            if (left) {
                camYaw = 30.0f; camPitch = 32.0f; camDist = 220.0f;
                camTargetX = 0.0f; camTargetY = 0.0f; camTargetZ = 20.0f;
            } else targetUpstream = 0.05f;
            break;
        case 9:
            if (left) {
                camYaw = 28.0f;
                camPitch = 24.0f;
                camDist = 120.0f;
                camTargetX = 20.0f;
                camTargetY = 131.0f;
                camTargetZ = 0.0f;
            }
            break;
    }
    return true;
}

// =====================================================================
//  UPDATE
// =====================================================================
void update(float dt) {
    timeAcc += dt;
    float k = 1.0f * dt; if (k > 1.0f) k = 1.0f;

    rainIntensity += (targetRain - rainIntensity) * (1.6f * dt);
    drainLevel    += (targetDrain - drainLevel) * k;
    roadFlood     += (targetRoad  - roadFlood)  * (0.8f * dt);
    cloudCover    += (targetCloud - cloudCover) * (1.6f * dt);
    gateOpen      += (targetGate  - gateOpen)  * (0.55f * dt);

    // -------- Barrage gate opening --------
    barrageGateOpen += (targetBarrageGate - barrageGateOpen) * (1.0f * dt);

    // -------- Barrage flow (needs gate + head) --------
    float headNorm = (upstreamSurfaceY() - downstreamSurfaceY()) / 3.0f;
    if (headNorm < 0) headNorm = 0; if (headNorm > 1) headNorm = 1;
    headNorm = sqrtf(headNorm);
    float desiredBarrageFlow = barrageGateOpen * headNorm;
    barrageFlow += (desiredBarrageFlow - barrageFlow) * (0.75f * dt);
    if (desiredBarrageFlow <= 0.001f && barrageFlow <= 0.005f) barrageFlow = 0.0f;

    // -------- UPSTREAM water level --------
    // Keep the downstream pool low while the gate is closed; rain replenishes
    // only the upstream pool, and release water is conserved between the pools.
    if (rainIntensity > 0.05f && targetBarrageGate < 0.5f &&
        barrageGateOpen < 0.05f) {
        targetUpstream = fminf(1.0f, targetUpstream + dt * 0.065f * rainIntensity);
    }
    if (targetBarrageGate > 0.5f && barrageFlow > 0.001f) {
        float transfer = dt * barrageReleaseRate;
        targetUpstream = fmaxf(0.05f, targetUpstream - transfer);
        targetRiver = fminf(1.0f, targetRiver + transfer);
    }
    if (targetRiver > targetUpstream)
        targetRiver = targetUpstream =
            0.5f * (targetRiver + targetUpstream);
    if (targetUpstream - targetRiver < 0.005f &&
        targetBarrageGate > 0.5f) {
        float sharedLevel = 0.5f * (targetUpstream + targetRiver);
        targetUpstream = sharedLevel;
        targetRiver = sharedLevel;
    }
    upstreamLevel += (targetUpstream - upstreamLevel) * (0.55f * dt);
    riverLevel += (targetRiver - riverLevel) * (0.55f * dt);

    barrageOverloaded = upstreamLevel >= BARRAGE_OVERLOAD_LEVEL &&
                        targetBarrageGate < 0.5f && barrageGateOpen < 0.05f;

    // -------- City drains --------
    if (rainIntensity > 0.05f && targetGate < 0.5f) {
        if (targetDrain < 1.0f)
            targetDrain = fminf(1.0f, targetDrain + dt * 0.045f * rainIntensity);
    } else if (rainIntensity <= 0.05f) {
        if (targetDrain > DRAIN_BASE_WATER)
            targetDrain = fmaxf(DRAIN_BASE_WATER, targetDrain - dt * 0.012f);
    }

    if (rainOn && targetGate < 0.5f && drainLevel > DRAIN_SPILL_LEVEL) {
        float spill = (drainLevel - DRAIN_SPILL_LEVEL) / (1.0f - DRAIN_SPILL_LEVEL);
        targetRoad = fminf(1.0f, targetRoad + dt * 0.08f * spill);
    } else if ((!rainOn || targetGate > 0.5f) && targetRoad > 0.0f) {
        targetRoad = fmaxf(0.0f, targetRoad - dt * 0.0167f);
    }

    if (gateOpen > 0.3f && drainLevel > 0.15f) {
        targetDrain = fmaxf(DRAIN_BASE_WATER,
                            targetDrain - dt * ((1.0f - DRAIN_BASE_WATER) / 60.0f) * gateOpen);
        targetRoad  = fmaxf(0.0f, targetRoad - dt * 0.0167f * gateOpen);
    }

    // -------- City Pelton turbines --------
    float gateFactor  = gateOpen;
    float waterFactor = fmaxf(0.0f, drainLevel - DRAIN_BASE_WATER) * 1.6f;
    float desiredFlow = gateFactor * waterFactor;
    if (desiredFlow > 1.0f) desiredFlow = 1.0f;
    turbineFlow += (desiredFlow - turbineFlow) * (0.80f * dt);
    if (desiredFlow <= 0.001f && turbineFlow <= 0.005f) turbineFlow = 0.0f;

    for (int i = 0; i < NTURBINES; i++) {
        float rateW = 340.0f * (0.92f + 0.04f * i);
        turbineWheelAngles[i] += turbineFlow * rateW * dt;
        while (turbineWheelAngles[i] > 360.0f) turbineWheelAngles[i] -= 360.0f;
        float rateG = 680.0f * (0.92f + 0.04f * i);
        turbineGenAngles[i] += turbineFlow * rateG * dt;
        while (turbineGenAngles[i] > 360.0f) turbineGenAngles[i] -= 360.0f;
    }
    for (int gate = 0; gate < NSIDE_CITY_GATES; ++gate) {
        for (int turbine = 0; turbine < NTURBINES; ++turbine) {
            float rateW = 340.0f * (0.92f + 0.04f * turbine + 0.08f * (gate + 1));
            sideTurbineWheelAngles[gate][turbine] += turbineFlow * rateW * dt;
            while (sideTurbineWheelAngles[gate][turbine] > 360.0f)
                sideTurbineWheelAngles[gate][turbine] -= 360.0f;
            sideTurbineGenAngles[gate][turbine] += turbineFlow * rateW * 2.0f * dt;
            while (sideTurbineGenAngles[gate][turbine] > 360.0f)
                sideTurbineGenAngles[gate][turbine] -= 360.0f;
        }
    }
    powerOutput += (turbineFlow - powerOutput) * (0.65f * dt);

    // -------- Barrage wheels --------
    for (int i = 0; i < BARRAGE_NGATES; i++) {
        float rate = 200.0f * (0.90f + 0.05f * i);
        barrageWheelAngles[i] += barrageFlow * rate * dt;
        while (barrageWheelAngles[i] > 360.0f) barrageWheelAngles[i] -= 360.0f;
    }
    barragePower += (barrageFlow - barragePower) * (0.65f * dt);

    // -------- Lights driven by combined power --------
    float totalPower = powerOutput * NCITY_TURBINES * 30.0f + barragePower * BARRAGE_NGATES * 20.0f;
    float combined = fminf(1.0f, totalPower / 160.0f);
    float targetLights = combined;
    lightIntensity += (targetLights - lightIntensity) * (0.22f * dt);

    // -------- Rain drops --------
    if (rainIntensity > 0.05f) {
        for (auto& r : rainDrops) {
            r.y -= r.speed * dt;
            r.x += windX * dt * 3.0f;
            if (r.y < -10.0f) {
                r.y = RAIN_Y_MIN + (rand()%10000)/100.0f;
                r.x = RAIN_X_MIN + (rand()%24000)/100.0f;
                r.z = RAIN_Z_MIN + (rand()%21000)/100.0f;
            }
        }
    }
}

// =====================================================================
//  DISPLAY
// =====================================================================
void display() {
    float c = cloudCover;
    float clearR = 0.78f*(1-c) + 0.22f*c;
    float clearG = 0.85f*(1-c) + 0.24f*c;
    float clearB = 0.95f*(1-c) + 0.30f*c;
    glClearColor(clearR, clearG, clearB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawSkyDome();
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(50.0, 1100.0/750.0, 0.1, 1500.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float yaw = camYaw * M_PI/180.0f;
    float pit = camPitch * M_PI/180.0f;
    float cx = camTargetX + camDist*cosf(pit)*sinf(yaw);
    float cy = camTargetY + camDist*sinf(pit);
    float cz = camTargetZ + camDist*cosf(pit)*cosf(yaw);
    gluLookAt(cx, cy, cz, camTargetX, camTargetY, camTargetZ, 0,1,0);
    setSceneLightingEnabled();

    float ambScale = 1.0f - cloudCover * 0.55f;
    GLfloat lpos[] = { 40.0f, 100.0f, 30.0f, 1.0f };
    GLfloat amb[] = {
        ambientLightingOn ? 0.35f*ambScale : 0.0f,
        ambientLightingOn ? 0.37f*ambScale : 0.0f,
        ambientLightingOn ? 0.42f*ambScale : 0.0f,
        1.0f
    };
    GLfloat dif[] = {
        diffuseLightingOn ? 0.90f*ambScale : 0.0f,
        diffuseLightingOn ? 0.90f*ambScale : 0.0f,
        diffuseLightingOn ? 0.95f*ambScale : 0.0f,
        1.0f
    };
    GLfloat spec[] = {
        specularLightingOn ? 0.90f*ambScale : 0.0f,
        specularLightingOn ? 0.92f*ambScale : 0.0f,
        specularLightingOn ? 1.00f*ambScale : 0.0f,
        1.0f
    };
    GLfloat globalAmbient[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
    GLfloat modelView[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
    comparisonLightPositionEye[0] = modelView[0]*lpos[0] + modelView[4]*lpos[1] +
                                    modelView[8]*lpos[2] + modelView[12];
    comparisonLightPositionEye[1] = modelView[1]*lpos[0] + modelView[5]*lpos[1] +
                                    modelView[9]*lpos[2] + modelView[13];
    comparisonLightPositionEye[2] = modelView[2]*lpos[0] + modelView[6]*lpos[1] +
                                    modelView[10]*lpos[2] + modelView[14];
    for (int i = 0; i < 3; i++) {
        comparisonAmbientColor[i] = amb[i];
        comparisonDiffuseColor[i] = dif[i];
        comparisonSpecularColor[i] = spec[i];
    }

    markDrainOpeningsInStencil();
    drawGround();
    drawGrassPatches();
    drawCityFlood();
    drawRoads();
    drawGoliAndDrains();
    drawDrains();
    drawRoadFlood();
    drawRoadFlowToGate();
    drawDrainWater();
    drawGoliWater();
    drawDrainOverflow();
    drawGateCollectionPool();
    drawGate();
    drawSideCityGates();
    drawRoadAndDrainFlowToTurbine();

    // River + barrage (draw river first, then barrage structure on top)
    drawRiver();
    drawBarrage();

    drawHydroSystem();
    drawBuildings();
    drawPowerLines();
    drawTrees();
    drawStreetlights();
    drawClouds();
    drawRain();

    if (showHUD) drawHUD();
    drawRenderTechniqueControls();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(50.0, (float)w/h, 0.1, 1500.0);
    glMatrixMode(GL_MODELVIEW);
}

void zoomAtCursor(int x, int y, float zoomFactor) {
    float oldDistance = camDist;
    camDist = fmaxf(CAMERA_MIN_DISTANCE,
                    fminf(CAMERA_MAX_DISTANCE, camDist * zoomFactor));
    float distanceRatio = camDist / oldDistance;

    GLdouble modelView[16], projection[16];
    GLint viewport[4];
    GLfloat depth = 1.0f;
    GLdouble worldX, worldY, worldZ;
    int screenY = windowHeight - y - 1;
    glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glReadPixels(x, screenY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    if (depth < 1.0f &&
        gluUnProject(x, screenY, depth, modelView, projection, viewport,
                     &worldX, &worldY, &worldZ)) {
        camTargetX += (float)(worldX - camTargetX) * (1.0f - distanceRatio);
        camTargetY += (float)(worldY - camTargetY) * (1.0f - distanceRatio);
        camTargetZ += (float)(worldZ - camTargetZ) * (1.0f - distanceRatio);
    }
}

void mouseButton(int b, int s, int x, int y) {
    if (b == GLUT_LEFT_BUTTON && s == GLUT_DOWN && handleRenderTechniqueClick(x, y)) {
        glutPostRedisplay();
        return;
    }
    lastMouseX = x; lastMouseY = y;
    if (b == GLUT_LEFT_BUTTON && s == GLUT_DOWN) {
        GLdouble modelView[16], projection[16];
        GLint viewport[4];
        GLfloat depth = 1.0f;
        GLdouble worldX, worldY, worldZ;
        glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
        glGetDoublev(GL_PROJECTION_MATRIX, projection);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glReadPixels(x, windowHeight - y - 1, 1, 1, GL_DEPTH_COMPONENT,
                     GL_FLOAT, &depth);
        if (depth < 1.0f &&
            gluUnProject(x, windowHeight - y - 1, depth, modelView,
                         projection, viewport, &worldX, &worldY, &worldZ)) {
            camTargetX = (float)worldX;
            camTargetY = (float)worldY;
            camTargetZ = (float)worldZ;
            int clickTime = glutGet(GLUT_ELAPSED_TIME);
            bool doubleClick = clickTime - lastFocusClickTime <= 400 &&
                abs(x - lastFocusClickX) <= 12 &&
                abs(y - lastFocusClickY) <= 12;
            if (doubleClick)
                camDist = fmaxf(CAMERA_MIN_DISTANCE, camDist * 0.5f);
            lastFocusClickTime = clickTime;
            lastFocusClickX = x;
            lastFocusClickY = y;
        }
    }
    if (b == GLUT_LEFT_BUTTON)  lmb = (s == GLUT_DOWN);
    if (b == GLUT_RIGHT_BUTTON) rmb = (s == GLUT_DOWN);
    if (b == GLUT_MIDDLE_BUTTON) mmb = (s == GLUT_DOWN);
    if (b == 3) zoomAtCursor(x, y, 0.82f);
    if (b == 4) zoomAtCursor(x, y, 1.22f);
    glutPostRedisplay();
}
void mouseMove(int x, int y) {
    int dx = x - lastMouseX, dy = y - lastMouseY;
    lastMouseX = x; lastMouseY = y;
    if (lmb) {
        camYaw   -= dx * 0.4f;
        camPitch -= dy * 0.4f;
        if (camPitch < 3.0f)  camPitch = 3.0f;
        if (camPitch > 89.0f) camPitch = 89.0f;
    }
    if (rmb || mmb) {
        float scale = camDist * 0.0022f;
        camTargetX -= dx * scale * cosf(camYaw * M_PI/180.0f);
        camTargetZ += dx * scale * sinf(camYaw * M_PI/180.0f);
        camTargetZ -= dy * scale * cosf(camYaw * M_PI/180.0f);
        camTargetX -= dy * scale * sinf(camYaw * M_PI/180.0f);
    }
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 27: exit(0); break;
        case 'h': case 'H': showHUD = !showHUD; break;
        case ' ': targetGate = (targetGate > 0.5f) ? 0.0f : 1.0f; break;
        case 'g': case 'G':
            if (targetBarrageGate > 0.5f)
                setBarrageGate(false);
            else if (barrageOverloaded)
                setBarrageGate(true);
            break;
        case 'r': case 'R':
            rainOn = !rainOn;
            targetRain  = rainOn ? 1.0f : 0.0f;
            targetCloud = rainOn ? 0.95f : 0.10f;
            break;
        case 'c': case 'C': targetCloud = (targetCloud > 0.5f) ? 0.10f : 0.95f; break;
        case 'd': case 'D': rainOn = true;  targetRain = 1.0f; targetCloud = 0.95f; break;
        case 'e': case 'E': rainOn = false; targetRain = 0.0f; targetCloud = 0.10f; break;
        case 'f': case 'F':
            rainOn = !rainOn;
            targetRain  = rainOn ? 1.0f : 0.0f;
            targetCloud = rainOn ? 0.95f : 0.10f;
            break;
        case 'v': case 'V': targetUpstream = 0.85f; break;
        case 'b': case 'B': targetUpstream = 0.05f; break;
        case '1':
            rainOn = false; targetRain = 0.0f; targetCloud = 0.10f;
            targetDrain = DRAIN_BASE_WATER; targetRoad = 0.0f; targetRiver = 0.05f;
            targetGate = 0.0f; targetUpstream = 0.82f; setBarrageGate(false);
            break;
        case '2':
            rainOn = true; targetRain = 1.0f; targetCloud = 0.95f;
            targetRiver = 0.05f; targetUpstream = 0.82f;
            targetGate = 0.0f; setBarrageGate(false);
            break;
        case '3':
            rainOn = true; targetRain = 1.0f; targetCloud = 0.95f;
            targetRiver = 0.05f; targetUpstream = 0.82f;
            targetGate = 1.0f; setBarrageGate(false);
            break;
        case '4':
            rainOn = true; targetRain = 1.0f; targetCloud = 0.95f;
            targetGate = 0.0f; setBarrageGate(false);
            targetRiver = 0.05f; targetUpstream = 0.85f;
            break;
        case '0':
            rainOn = false; targetRain = 0.0f; targetCloud = 0.10f;
            targetDrain = DRAIN_BASE_WATER; targetRoad = 0.0f; targetRiver = 0.05f;
            targetGate = 0.0f; targetUpstream = 0.82f; setBarrageGate(false);
            camYaw = 30; camPitch = 32; camDist = 220;
            camTargetX = 0; camTargetY = 0; camTargetZ = 20; break;
        case 'a': case 'A': camYaw -= 4.0f; break;
        case 'z': case 'Z': camYaw += 4.0f; break;
        case 'w': case 'W': camPitch += 3.0f; if (camPitch>89) camPitch=89; break;
        case 's': case 'S': camPitch -= 3.0f; if (camPitch<3)  camPitch=3;  break;
        case 'q': case 'Q': camDist = fmaxf(CAMERA_MIN_DISTANCE, camDist * 0.82f); break;
        case 'x': case 'X': camDist = fminf(CAMERA_MAX_DISTANCE, camDist * 1.22f); break;
    }
    glutPostRedisplay();
}

void specialKey(int k, int, int) {
    float step = camDist * 0.05f;
    switch (k) {
        case GLUT_KEY_LEFT:  camTargetX -= step; break;
        case GLUT_KEY_RIGHT: camTargetX += step; break;
        case GLUT_KEY_UP:    camTargetZ -= step; break;
        case GLUT_KEY_DOWN:  camTargetZ += step; break;
        case GLUT_KEY_PAGE_UP:   camTargetY += 1.0f; break;
        case GLUT_KEY_PAGE_DOWN: camTargetY -= 1.0f; break;
    }
    glutPostRedisplay();
}

void timer(int) {
    static int last = glutGet(GLUT_ELAPSED_TIME);
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - last)/1000.0f; last = now;
    if (dt > 0.1f) dt = 0.1f;
    update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv) {
    srand((unsigned)time(0));
    buildDrains(); buildBuildings(); buildTrees(); initRain();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    glutInitWindowSize(1100, 750);
    glutCreateWindow("Dhaka Metro - Hydro Powerhouse + Teesta Barrage");
    glEnable(GL_DEPTH_TEST);
    setSceneLightingEnabled();
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    GLfloat defaultSpecular[] = {0.28f, 0.30f, 0.34f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!initComparisonShader()) {
        wheelGouraudOn = false;
        wheelPhongOn = false;
    }
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMove);
    glutPassiveMotionFunc(mouseMove);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutTimerFunc(0, timer, 0);
    printf("Click the upper-right buttons for lighting, Gouraud/Phong wheels, ray tracing and rain.\n");
    printf("H: HUD | R: rain | SPACE: city gate | G: barrage gate | 4: barrage demo\n");
    printf("Watch the upstream pool rise with rain, then drop when G opens.\n");
    printf("ESC: quit\n");
    glutMainLoop();
    return 0;
}