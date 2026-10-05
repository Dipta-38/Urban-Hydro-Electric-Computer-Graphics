#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float cityYatZ(float z) {
    float t = (z - (-35.0f)) / (15.0f - (-35.0f));
    if (t < 0) t = 0; if (t > 1) t = 1;
    return 4.0f - t * 3.5f;
}

const float GATE_Y    =  0.5f;
const float TURBINE_Y = -1.5f;
const float RIVER_Y   = -3.5f;
const float CITY_BACK_Z  = -35.0f;
const float CITY_FRONT_Z =  15.0f;
const float GATE_Z       =  15.0f;
const float TURBINE_Z    =  20.0f;
const float RIVER_NEAR   =  TURBINE_Z - 1.5f;
const float RIVER_FAR    =  60.0f;

const float DEEP_BLUE_R = 0.02f, DEEP_BLUE_G = 0.24f, DEEP_BLUE_B = 0.72f;
const float SKY_BLUE_R  = 0.55f, SKY_BLUE_G  = 0.80f, SKY_BLUE_B  = 1.00f;

float camYaw   = 35.0f;
float camPitch = 28.0f;
float camDist  = 95.0f;
float camTargetX = 0.0f, camTargetY = 0.0f, camTargetZ = -5.0f;

int lastMouseX = 0, lastMouseY = 0;
bool lmb = false, rmb = false;
bool showHUD = false;

bool  rainOn = false;
float rainIntensity = 0.0f, targetRain = 0.0f;
float drainLevel    = 0.02f, targetDrain = 0.02f;
float roadFlood     = 0.0f,  targetRoad  = 0.0f;
float riverLevel    = 0.35f, targetRiver = 0.35f;
float cloudCover    = 0.10f, targetCloud = 0.10f;
float gateOpen      = 0.0f,  targetGate  = 0.0f;

float turbineFlow = 0.0f;
float powerOutput = 0.0f;
float lightIntensity = 0.0f;
float turbineAngle = 0.0f;
float generatorAngle = 0.0f;
float timeAcc = 0.0f;
float windX = 0.5f;
const float ROAD_W = 16.0f;
const float KERB_W = 0.6f;
const float KERB_H = 0.18f;
const float DRAIN_W = 2.4f;
const float DRAIN_DEPTH = 0.65f;
const float DRAIN_BASE_WATER = 0.28f;

struct Road { float x1, x2, z1, z2; bool vertical; };
const Road roads[] = {
    { -8.0f,  8.0f, -33.0f, 13.0f, true },
    { -30.0f, 30.0f, -12.0f, -1.0f, false },
};
const int NROADS = sizeof(roads)/sizeof(roads[0]);

struct Drain { float x1, x2, z1, z2; bool vertical; float roadEdge; };
std::vector<Drain> drains;

void buildDrains() {
    drains.clear();

    auto addDrain = [&](float x1, float x2, float z1, float z2, bool vertical, float roadEdge) {
        if (x2 <= x1 || z2 <= z1) return;
        Drain drain;
        drain.x1 = x1; drain.x2 = x2;
        drain.z1 = z1; drain.z2 = z2;
        drain.vertical = vertical;
        drain.roadEdge = roadEdge;
        drains.push_back(drain);
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
                Road crossing = roads[j];
                if (crossing.vertical || crossing.x1 > r.x2 || crossing.x2 < r.x1) continue;
                float crossingStart = crossing.z1 - KERB_W;
                float crossingEnd = crossing.z2 + KERB_W;
                if (crossingStart > segmentStart)
                    addVerticalPair(segmentStart, fminf(crossingStart, r.z2));
                segmentStart = fmaxf(segmentStart, crossingEnd);
            }
            addVerticalPair(segmentStart, r.z2);
        } else {
            auto addHorizontalPair = [&](float x1, float x2) {
                addDrain(x1, x2, r.z1 - KERB_W - DRAIN_W, r.z1 - KERB_W, false, r.z1);
                addDrain(x1, x2, r.z2 + KERB_W, r.z2 + KERB_W + DRAIN_W, false, r.z2);
            };

            float segmentStart = r.x1;
            for (int j = 0; j < NROADS; j++) {
                Road crossing = roads[j];
                if (!crossing.vertical || crossing.z1 > r.z2 || crossing.z2 < r.z1) continue;
                float crossingStart = crossing.x1 - KERB_W;
                float crossingEnd = crossing.x2 + KERB_W;
                if (crossingStart > segmentStart)
                    addHorizontalPair(segmentStart, fminf(crossingStart, r.x2));
                segmentStart = fmaxf(segmentStart, crossingEnd);
            }
            addHorizontalPair(segmentStart, r.x2);
        }
    }
}

struct Building { float x, z, w, d, h, r, g, b; int floors; };
std::vector<Building> buildings;

void buildBuildings() {
    struct B { float x,z,w,d,h,r,g,b; int fl; };
    B list[] = {
        {-24, -22, 12, 8, 7, 0.55f,0.52f,0.48f, 3},
        {-42,  -2, 12, 8, 8, 0.50f,0.50f,0.54f, 3},
        { 24, -22, 12, 8, 7, 0.54f,0.50f,0.46f, 3},
        { 42,  -2, 12, 8, 9, 0.46f,0.48f,0.52f, 4},
    };
    for (auto& b : list) {
        Building bb;
        bb.x=b.x; bb.z=b.z; bb.w=b.w; bb.d=b.d; bb.h=b.h;
        bb.r=b.r; bb.g=b.g; bb.b=b.b; bb.floors=b.fl;
        buildings.push_back(bb);
    }
}

struct RainDrop { float x, y, z, speed; };
std::vector<RainDrop> rainDrops;

void initRain() {
    for (int i = 0; i < 2200; i++) {
        RainDrop r;
        r.x = (rand()%14000)/100.0f - 70.0f;
        r.y = (rand()%7000)/100.0f - 10.0f;
        r.z = (rand()%14000)/100.0f - 70.0f;
        r.speed = 26.0f + (rand()%100)/6.0f;
        rainDrops.push_back(r);
    }
}

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
    float current = sinf(z * freq - t * speed);
    float crossWave = sinf((x + z) * freq * 0.55f - t * speed * 0.65f);
    float smallWave = sinf(x * freq * 1.7f + t * speed * 1.3f);
    return amp * (current * 0.62f + crossWave * 0.25f + smallWave * 0.13f);
}

void waterColorAt(float t, float& r, float& g, float& b) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    r = DEEP_BLUE_R * (1 - t) + SKY_BLUE_R * t;
    g = DEEP_BLUE_G * (1 - t) + SKY_BLUE_G * t;
    b = DEEP_BLUE_B * (1 - t) + SKY_BLUE_B * t;
}

void drawWaterPlane(float x1, float z1, float x2, float z2, float yTop,
                    float alpha,
                    float ripAmp, float ripFreq, float ripSpeed,
                    bool mixByHeight, bool mixByFresnel,
                    float skyBlueAmount)
{
    const int NX = 24;
    const int NZ = 24;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLfloat waterSpecular[] = {0.82f, 0.90f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, waterSpecular);
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
            float h = avg - yTop;
            float tH = h / (ripAmp + 1e-5f) * 0.5f + 0.5f;
            float tF = 1.0f - ny;
            if (tF < 0) tF = 0;
            
            float t = 0.0f;
            if (mixByHeight)  t += tH * 0.7f;
            if (mixByFresnel) t += tF * 0.5f;
            t *= skyBlueAmount;
            if (t < 0) t = 0; if (t > 1) t = 1;
            
            float r, g, b;
            waterColorAt(t, r, g, b);
            glColor4f(r, g, b, alpha);
            
            glBegin(GL_QUADS);
            glNormal3f(nx, ny, nz);
            glVertex3f(xa, ya, za);
            glVertex3f(xb, yb, za);
            glVertex3f(xb, yc, zb);
            glVertex3f(xa, yd, zb);
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
    GLfloat defaultSpecular[] = {0.20f, 0.20f, 0.20f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defaultSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 16.0f);
    glDisable(GL_BLEND);
}

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
    glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
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
    glDisable(GL_BLEND); glEnable(GL_LIGHTING);
}

void drawGround() {
    float c = cloudCover;
    const int SEG = 40;
    float zmin = CITY_BACK_Z - 5.0f;
    float zmax = CITY_FRONT_Z + 2.0f;
    for (int i = 0; i < SEG; i++) {
        float z0 = zmin + (zmax - zmin) * i / SEG;
        float z1 = zmin + (zmax - zmin) * (i + 1) / SEG;
        float y0 = cityYatZ(z0);
        float y1 = cityYatZ(z1);
        glColor3f(0.30f - c*0.14f, 0.32f - c*0.15f, 0.28f - c*0.12f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0.25f);
        glVertex3f(-60, y0, z0);
        glVertex3f( 60, y0, z0);
        glVertex3f( 60, y1, z1);
        glVertex3f(-60, y1, z1);
        glEnd();
    }
}

void drawCityFlood() {
    if (roadFlood <= 0.01f) return;

    const float xMin = -60.0f, xMax = 60.0f;
    const float zMin = CITY_BACK_Z - 5.0f, zMax = CITY_FRONT_Z + 2.0f;
    const int NX = 80, NZ = 38;
    const float spread = 2.0f + roadFlood * 58.0f;
    const float fadeWidth = 5.0f;

    auto floodCoverage = [&](float x, float z) {
        float nearest = 1000.0f;
        for (int i = 0; i < NROADS; i++) {
            const Road& road = roads[i];
            float dx = x < road.x1 ? road.x1 - x : (x > road.x2 ? x - road.x2 : 0.0f);
            float dz = z < road.z1 ? road.z1 - z : (z > road.z2 ? z - road.z2 : 0.0f);
            nearest = fminf(nearest, sqrtf(dx * dx + dz * dz));
        }

        float curveZ = fmaxf(-32.0f, fminf(12.0f, z));
        float outsideCurveZ = z < -32.0f ? -32.0f - z : (z > 12.0f ? z - 12.0f : 0.0f);
        for (int side = -1; side <= 1; side += 2) {
            float curveX = side * (-26.0f + sinf(curveZ * 0.07f) * 9.0f);
            float dx = fmaxf(0.0f, fabsf(x - curveX) - 4.5f);
            nearest = fminf(nearest, sqrtf(dx * dx + outsideCurveZ * outsideCurveZ));
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
            float alphaScale = 0.30f + roadFlood * 0.35f;
            float y00 = cityYatZ(za) + 0.045f + ripple(xa, za, timeAcc, 0.025f, 1.1f, 2.0f);
            float y10 = cityYatZ(za) + 0.045f + ripple(xb, za, timeAcc, 0.025f, 1.1f, 2.0f);
            float y11 = cityYatZ(zb) + 0.045f + ripple(xb, zb, timeAcc, 0.025f, 1.1f, 2.0f);
            float y01 = cityYatZ(zb) + 0.045f + ripple(xa, zb, timeAcc, 0.025f, 1.1f, 2.0f);

            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xa, za) * alphaScale);
            glNormal3f(0, 1, 0.25f); glVertex3f(xa, y00, za);
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xb, za) * alphaScale);
            glNormal3f(0, 1, 0.25f); glVertex3f(xb, y10, za);
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xb, zb) * alphaScale);
            glNormal3f(0, 1, 0.25f); glVertex3f(xb, y11, zb);
            glColor4f(0.10f, 0.34f, 0.82f, floodCoverage(xa, zb) * alphaScale);
            glNormal3f(0, 1, 0.25f); glVertex3f(xa, y01, zb);
        }
    }
    glEnd();
    glDisable(GL_BLEND);
}

void drawRoads() {
    float c = cloudCover;
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        const int SEG = 24;
        if (r.vertical) {
            for (int s = 0; s < SEG; s++) {
                float za = r.z1 + (r.z2 - r.z1) * s / SEG;
                float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / SEG;
                float ya = cityYatZ(za);
                float yb = cityYatZ(zb);
                glColor3f(0.13f - c*0.05f, 0.13f - c*0.05f, 0.14f - c*0.05f);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(r.x1, ya, za);
                glVertex3f(r.x2, ya, za);
                glVertex3f(r.x2, yb, zb);
                glVertex3f(r.x1, yb, zb);
                glEnd();
            }
            glColor3f(0.85f, 0.75f, 0.15f);
            float cx = (r.x1 + r.x2) * 0.5f;
            for (float z = r.z1 + 1.0f; z < r.z2 - 1.0f; z += 4.0f) {
                float ya = cityYatZ(z) + 0.01f;
                float yb = cityYatZ(z + 2.5f) + 0.01f;
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0.25f);
                glVertex3f(cx - 0.14f, ya, z);
                glVertex3f(cx + 0.14f, ya, z);
                glVertex3f(cx + 0.14f, yb, z + 2.5f);
                glVertex3f(cx - 0.14f, yb, z + 2.5f);
                glEnd();
            }
        } else {
            const int ROAD_SEGMENTS = 16;
            glColor3f(0.13f - c*0.05f, 0.13f - c*0.05f, 0.14f - c*0.05f);
            for (int s = 0; s < ROAD_SEGMENTS; s++) {
                float za = r.z1 + (r.z2 - r.z1) * s / ROAD_SEGMENTS;
                float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / ROAD_SEGMENTS;
                float ya = cityYatZ(za);
                float yb = cityYatZ(zb);
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
        }
    }
    
    for (int i = 0; i < NROADS; i++) {
        Road r = roads[i];
        if (r.vertical) {
            const int SEG = 24;
            for (int s = 0; s < SEG; s++) {
                float za = r.z1 + (r.z2 - r.z1) * s / SEG;
                float zb = r.z1 + (r.z2 - r.z1) * (s + 1) / SEG;
                float ya = cityYatZ(za) + KERB_H;
                float yb = cityYatZ(zb) + KERB_H;
                
                glColor3f(0.70f - c*0.20f, 0.68f - c*0.20f, 0.64f - c*0.20f);
                glBegin(GL_QUADS);
                glNormal3f(0,1,0.25f);
                glVertex3f(r.x1 - KERB_W, ya, za);
                glVertex3f(r.x1,         ya, za);
                glVertex3f(r.x1,         yb, zb);
                glVertex3f(r.x1 - KERB_W, yb, zb);
                glEnd();
                
                glColor3f(0.52f - c*0.16f, 0.50f - c*0.16f, 0.46f - c*0.16f);
                glBegin(GL_QUADS);
                glNormal3f(-1, 0.3f, 0);
                glVertex3f(r.x1 - KERB_W, ya, za);
                glVertex3f(r.x1 - KERB_W, yb, zb);
                glVertex3f(r.x1 - KERB_W, yb - KERB_H - DRAIN_DEPTH, zb);
                glVertex3f(r.x1 - KERB_W, ya - KERB_H - DRAIN_DEPTH, za);
                glEnd();
                
                glColor3f(0.70f - c*0.20f, 0.68f - c*0.20f, 0.64f - c*0.20f);
                glBegin(GL_QUADS);
                glNormal3f(0,1,0.25f);
                glVertex3f(r.x2,         ya, za);
                glVertex3f(r.x2 + KERB_W, ya, za);
                glVertex3f(r.x2 + KERB_W, yb, zb);
                glVertex3f(r.x2,         yb, zb);
                glEnd();
                
                glColor3f(0.52f - c*0.16f, 0.50f - c*0.16f, 0.46f - c*0.16f);
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
}

void drawDrainChannel(const Drain& d) {
    float c = cloudCover;
    if (d.vertical) {
        const int SEG = 24;
        for (int s = 0; s < SEG; s++) {
            float za = d.z1 + (d.z2 - d.z1) * s / SEG;
            float zb = d.z1 + (d.z2 - d.z1) * (s + 1) / SEG;
            float yTopA = cityYatZ(za);
            float yTopB = cityYatZ(zb);
            float yBotA = yTopA - DRAIN_DEPTH;
            float yBotB = yTopB - DRAIN_DEPTH;
            bool roadOnRight = (d.roadEdge > (d.x1 + d.x2) * 0.5f);
            
            glColor3f(0.42f - c*0.14f, 0.40f - c*0.14f, 0.36f - c*0.14f);
            glBegin(GL_QUADS);
            if (roadOnRight) {
                glNormal3f(-1, 0.3f, 0);
                glVertex3f(d.x2, yTopA, za); glVertex3f(d.x2, yTopB, zb);
                glVertex3f(d.x2, yBotB, zb); glVertex3f(d.x2, yBotA, za);
            } else {
                glNormal3f(1, 0.3f, 0);
                glVertex3f(d.x1, yTopA, za); glVertex3f(d.x1, yTopB, zb);
                glVertex3f(d.x1, yBotB, zb); glVertex3f(d.x1, yBotA, za);
            }
            glEnd();
            
            glColor3f(0.36f - c*0.12f, 0.34f - c*0.12f, 0.30f - c*0.12f);
            glBegin(GL_QUADS);
            if (roadOnRight) {
                glNormal3f(1, 0.3f, 0);
                glVertex3f(d.x1, yTopA, za); glVertex3f(d.x1, yTopB, zb);
                glVertex3f(d.x1, yBotB, zb); glVertex3f(d.x1, yBotA, za);
            } else {
                glNormal3f(-1, 0.3f, 0);
                glVertex3f(d.x2, yTopA, za); glVertex3f(d.x2, yTopB, zb);
                glVertex3f(d.x2, yBotB, zb); glVertex3f(d.x2, yBotA, za);
            }
            glEnd();
            
            glColor3f(0.06f, 0.06f, 0.07f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0.25f);
            glVertex3f(d.x1, yBotA, za); glVertex3f(d.x2, yBotA, za);
            glVertex3f(d.x2, yBotB, zb); glVertex3f(d.x1, yBotB, zb);
            glEnd();

            // Narrow cast-iron cross bars leave the flowing water visible below.
            if (s % 2 == 0) {
                float z = (za + zb) * 0.5f;
                float y = cityYatZ(z) + 0.035f;
                glColor3f(0.16f - c*0.04f, 0.17f - c*0.04f, 0.18f - c*0.04f);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0);
                glVertex3f(d.x1, y, z - 0.055f); glVertex3f(d.x2, y, z - 0.055f);
                glVertex3f(d.x2, y, z + 0.055f); glVertex3f(d.x1, y, z + 0.055f);
                glEnd();
            }
        }
    } else {
        float yTop = cityYatZ(d.roadEdge);
        float yBot = yTop - DRAIN_DEPTH;
        glColor3f(0.42f - c*0.14f, 0.40f - c*0.14f, 0.36f - c*0.14f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0.3f, 1);
        glVertex3f(d.x1, yTop, d.z1); glVertex3f(d.x2, yTop, d.z1);
        glVertex3f(d.x2, yBot, d.z1); glVertex3f(d.x1, yBot, d.z1);
        glEnd();
        
        glColor3f(0.36f - c*0.12f, 0.34f - c*0.12f, 0.30f - c*0.12f);
        glBegin(GL_QUADS);
        glNormal3f(0, 0.3f, -1);
        glVertex3f(d.x1, yTop, d.z2); glVertex3f(d.x2, yTop, d.z2);
        glVertex3f(d.x2, yBot, d.z2); glVertex3f(d.x1, yBot, d.z2);
        glEnd();
        
        glColor3f(0.06f, 0.06f, 0.07f);
        quadY(d.x1, d.z1, d.x2, d.z2, yBot);

        // Repeated transverse bars form an open, road-side storm grate.
        glColor3f(0.16f - c*0.04f, 0.17f - c*0.04f, 0.18f - c*0.04f);
        for (float x = d.x1 + 0.55f; x < d.x2; x += 1.1f) {
            float y = cityYatZ(d.roadEdge) + 0.035f;
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(x - 0.045f, y, d.z1); glVertex3f(x + 0.045f, y, d.z1);
            glVertex3f(x + 0.045f, y, d.z2); glVertex3f(x - 0.045f, y, d.z2);
            glEnd();
        }
    }
}

void drawDrains() { for (auto& d : drains) drawDrainChannel(d); }

void drawDrainWater() {
    float waterLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    for (auto& d : drains) {
        if (d.vertical) {
            const int SEG = 20;
            for (int s = 0; s < SEG; s++) {
                float za = d.z1 + (d.z2 - d.z1) * s / SEG;
                float zb = d.z1 + (d.z2 - d.z1) * (s + 1) / SEG;
                float yTopA = cityYatZ(za);
                float yTopB = cityYatZ(zb);
                float extra = (za - CITY_BACK_Z) / (CITY_FRONT_Z - CITY_BACK_Z) * 0.08f;
                float wLvl = fminf(1.0f, waterLevel + extra);
                float yA = yTopA - DRAIN_DEPTH + wLvl * (DRAIN_DEPTH - 0.05f);
                float yB = yTopB - DRAIN_DEPTH + wLvl * (DRAIN_DEPTH - 0.05f);
                
                const int SUB = 5;
                for (int k = 0; k < SUB; k++) {
                    float xa = d.x1 + (d.x2 - d.x1) * k / SUB;
                    float xb = d.x1 + (d.x2 - d.x1) * (k + 1) / SUB;
                    float y1 = yA + ripple(xa, za, timeAcc, 0.035f, 1.6f, 3.0f);
                    float y2 = yA + ripple(xb, za, timeAcc, 0.035f, 1.6f, 3.0f);
                    float y3 = yB + ripple(xb, zb, timeAcc, 0.035f, 1.6f, 3.0f);
                    float y4 = yB + ripple(xa, zb, timeAcc, 0.035f, 1.6f, 3.0f);
                    
                    float nx = (y1 - y2) + (y4 - y3);
                    float nz = (y1 - y4) + (y2 - y3);
                    float ny = 0.3f;
                    float len = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-5f;
                    nx /= len; ny /= len; nz /= len;
                    
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    // DEEP BLUE WATER FOR DRAINS
                    glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.95f);
                    glBegin(GL_QUADS);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xa, y1, za); glVertex3f(xb, y2, za);
                    glVertex3f(xb, y3, zb); glVertex3f(xa, y4, zb);
                    glEnd();
                    glDisable(GL_BLEND);
                }
            }
        } else {
            float yTop = cityYatZ(d.roadEdge);
            float wY = yTop - DRAIN_DEPTH + waterLevel * (DRAIN_DEPTH - 0.05f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.95f);
            const int SEG = 32;
            for (int s = 0; s < SEG; s++) {
                float xa = d.x1 + (d.x2 - d.x1) * s / SEG;
                float xb = d.x1 + (d.x2 - d.x1) * (s + 1) / SEG;
                float ya = wY + 0.035f * sinf(fabsf(xa) * 0.45f - timeAcc * 2.8f);
                float yb = wY + 0.035f * sinf(fabsf(xb) * 0.45f - timeAcc * 2.8f);
                glBegin(GL_QUADS);
                glNormal3f(0, 1, 0);
                glVertex3f(xa, ya, d.z1 + 0.05f);
                glVertex3f(xb, yb, d.z1 + 0.05f);
                glVertex3f(xb, yb, d.z2 - 0.05f);
                glVertex3f(xa, ya, d.z2 - 0.05f);
                glEnd();
            }
            glDisable(GL_BLEND);
        }
    }
}

void drawDrainOverflow() {
    if (drainLevel <= 0.90f) return;
    float spill = fminf(1.0f, (drainLevel - 0.90f) * 10.0f);
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
                    
                    float nx = (y1 - y2) + (y4 - y3);
                    float nz = (y1 - y4) + (y2 - y3);
                    float ny = 0.35f;
                    float len = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-5f;
                    nx /= len; ny /= len; nz /= len;
                    
                    // Keep road flooding blue; reserve deep blue for the drains.
                    float baseR = 0.10f, baseG = 0.34f, baseB = 0.82f;
                    float whiteMix = 0.08f + roadFlood * 0.16f;
                    float rr = baseR * (1.0f - whiteMix) + 1.0f * whiteMix;
                    float gg = baseG * (1.0f - whiteMix) + 1.0f * whiteMix;
                    float bb = baseB * (1.0f - whiteMix) + 1.0f * whiteMix;
                    
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glColor4f(rr, gg, bb, 0.75f + roadFlood * 0.20f);
                    glBegin(GL_QUADS);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xa, y1, za); glVertex3f(xb, y2, za);
                    glVertex3f(xb, y3, zb); glVertex3f(xa, y4, zb);
                    glEnd();
                    glDisable(GL_BLEND);
                    
                }
            }
        } else {
            const int SEG_X = 24;
            const int SEG_Z = 8;
            float baseR = 0.10f, baseG = 0.34f, baseB = 0.82f;
            float whiteMix = 0.08f + roadFlood * 0.16f;
            float rr = baseR * (1.0f - whiteMix) + 1.0f * whiteMix;
            float gg = baseG * (1.0f - whiteMix) + 1.0f * whiteMix;
            float bb = baseB * (1.0f - whiteMix) + 1.0f * whiteMix;
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (int sx = 0; sx < SEG_X; sx++) {
                float xa = r.x1 + (r.x2 - r.x1) * sx / SEG_X;
                float xb = r.x1 + (r.x2 - r.x1) * (sx + 1) / SEG_X;
                for (int sz = 0; sz < SEG_Z; sz++) {
                    float za = r.z1 + (r.z2 - r.z1) * sz / SEG_Z;
                    float zb = r.z1 + (r.z2 - r.z1) * (sz + 1) / SEG_Z;
                    float ya = cityYatZ(za) + 0.03f + roadFlood * 0.05f;
                    float yb = cityYatZ(zb) + 0.03f + roadFlood * 0.05f;
                    float y1 = ya + ripple(xa, za, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y2 = ya + ripple(xb, za, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y3 = yb + ripple(xb, zb, timeAcc, 0.04f, 1.2f, 2.5f);
                    float y4 = yb + ripple(xa, zb, timeAcc, 0.04f, 1.2f, 2.5f);
                    float nx = (y1 - y2) + (y4 - y3);
                    float nz = (y1 - y4) + (y2 - y3);
                    float ny = 0.35f;
                    float len = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-5f;
                    nx /= len; ny /= len; nz /= len;
                    glColor4f(rr, gg, bb, 0.75f + roadFlood * 0.20f);
                    glBegin(GL_QUADS);
                    glNormal3f(nx, ny, nz);
                    glVertex3f(xa, y1, za); glVertex3f(xb, y2, za);
                    glVertex3f(xb, y3, zb); glVertex3f(xa, y4, zb);
                    glEnd();

                }
            }
            glDisable(GL_BLEND);
        }
    }
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

void drawFlowRibbon(float startX, float startZ, float startY,
                    float endX, float endZ, float endY,
                    float startWidth, float endWidth, float alpha, bool drainFlow = false) {
    const int SEG = 24;
    float dx = endX - startX;
    float dz = endZ - startZ;
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 1e-5f) return;
    float sideX = -dz / length;
    float sideZ = dx / length;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (drainFlow)
        glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, alpha);
    else
        glColor4f(0.10f, 0.34f, 0.82f, alpha);
    for (int i = 0; i < SEG; i++) {
        float t0 = (float)i / SEG;
        float t1 = (float)(i + 1) / SEG;
        float cx0 = startX + dx * t0;
        float cz0 = startZ + dz * t0;
        float cx1 = startX + dx * t1;
        float cz1 = startZ + dz * t1;
        float w0 = startWidth + (endWidth - startWidth) * t0;
        float w1 = startWidth + (endWidth - startWidth) * t1;
        float y0 = startY + (endY - startY) * t0;
        float y1 = startY + (endY - startY) * t1;
        float leftY0 = y0 + ripple(cx0 - sideX * w0, cz0 - sideZ * w0, timeAcc, 0.04f, 1.2f, 2.5f);
        float rightY0 = y0 + ripple(cx0 + sideX * w0, cz0 + sideZ * w0, timeAcc, 0.04f, 1.2f, 2.5f);
        float leftY1 = y1 + ripple(cx1 - sideX * w1, cz1 - sideZ * w1, timeAcc, 0.04f, 1.2f, 2.5f);
        float rightY1 = y1 + ripple(cx1 + sideX * w1, cz1 + sideZ * w1, timeAcc, 0.04f, 1.2f, 2.5f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(cx0 - sideX * w0, leftY0, cz0 - sideZ * w0);
        glVertex3f(cx0 + sideX * w0, rightY0, cz0 + sideZ * w0);
        glVertex3f(cx1 + sideX * w1, rightY1, cz1 + sideZ * w1);
        glVertex3f(cx1 - sideX * w1, leftY1, cz1 - sideZ * w1);
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

        float curvedEndZ = 12.0f;
        float curvedEndX = -26.0f + sinf(curvedEndZ * 0.07f) * 9.0f;
        drawFlowRibbon(curvedEndX, curvedEndZ, cityYatZ(curvedEndZ) + 0.12f,
                       0.0f, outletZ, outletY, 2.2f, 0.6f, 0.72f * roadFlow);
        drawFlowRibbon(-curvedEndX, curvedEndZ, cityYatZ(curvedEndZ) + 0.12f,
                       0.0f, outletZ, outletY, 2.2f, 0.6f, 0.72f * roadFlow);
    }

    float waterLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    for (auto& d : drains) {
        if (d.vertical) {
            float sourceX = (d.x1 + d.x2) * 0.5f;
            float sourceZ = d.z2;
            float sourceY = cityYatZ(sourceZ) - DRAIN_DEPTH + waterLevel * (DRAIN_DEPTH - 0.05f);
            drawFlowRibbon(sourceX, sourceZ, sourceY, 0.0f, outletZ, outletY,
                           0.75f, 0.45f, 0.38f + 0.42f * waterLevel, true);
        } else {
            float sourceX = (d.x1 + d.x2) * 0.5f;
            float sourceZ = (d.z1 + d.z2) * 0.5f;
            float sourceY = cityYatZ(d.roadEdge) - DRAIN_DEPTH + waterLevel * (DRAIN_DEPTH - 0.05f);
            drawFlowRibbon(sourceX, sourceZ, sourceY, 0.0f, outletZ, outletY,
                           0.6f, 0.45f, 0.38f + 0.42f * waterLevel, true);
        }
    }

    float curvedEndZ = 12.0f;
    float curvedEndX = -26.0f + sinf(curvedEndZ * 0.07f) * 9.0f;
    float tangentX = (-26.0f + sinf((curvedEndZ + 0.1f) * 0.07f) * 9.0f) - curvedEndX;
    float tangentLength = sqrtf(tangentX * tangentX + 0.01f);
    float normalX = -0.1f / tangentLength;
    float normalZ = tangentX / tangentLength;
    for (int curveSide = -1; curveSide <= 1; curveSide += 2) {
        for (int side = -1; side <= 1; side += 2) {
            float offset = side * (4.5f + 0.5f + DRAIN_W * 0.5f);
            float sourceX = curveSide * (curvedEndX + normalX * offset);
            float sourceZ = curvedEndZ + normalZ * offset;
            float sourceY = cityYatZ(sourceZ) - DRAIN_DEPTH + waterLevel * (DRAIN_DEPTH - 0.05f);
            drawFlowRibbon(sourceX, sourceZ, sourceY, 0.0f, outletZ, outletY,
                           0.55f, 0.45f, 0.38f + 0.42f * waterLevel, true);
        }
    }
}

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
    glPushMatrix();
    glTranslatef(-19.0f, poolY - 0.2f, 12.0f);
    glScalef(0.8f, 0.4f, 6.0f); drawCube(); glPopMatrix();
    glPushMatrix();
    glTranslatef(19.0f, poolY - 0.2f, 12.0f);
    glScalef(0.8f, 0.4f, 6.0f); drawCube(); glPopMatrix();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.18f, 0.46f, 0.92f, 0.82f * fill);
    for (int side = -1; side <= 1; side += 2) {
        glColor4f(0.18f, 0.46f, 0.92f, 0.82f * fill);
        glBegin(GL_QUADS);
        glVertex3f(side * 15.0f, poolY + 0.05f, 2.0f);
        glVertex3f(side * 16.0f, poolY + 0.05f, 2.0f);
        glVertex3f(side * 2.0f, poolY + 0.05f, 11.5f);
        glVertex3f(side * 1.0f, poolY + 0.05f, 11.5f);
        glEnd();

        glColor4f(0.62f, 0.82f, 1.0f, 0.75f * fill);
        for (int i = 0; i < 6; i++) {
            float t0 = fmodf(timeAcc * 0.35f + i / 6.0f, 1.0f);
            float t1 = fminf(1.0f, t0 + 0.045f);
            float z0 = 2.0f + t0 * 9.5f;
            float z1 = 2.0f + t1 * 9.5f;
            float innerX0 = side * (15.0f + (2.0f - 15.0f) * t0);
            float outerX0 = side * (16.0f + (1.0f - 16.0f) * t0);
            float innerX1 = side * (15.0f + (2.0f - 15.0f) * t1);
            float outerX1 = side * (16.0f + (1.0f - 16.0f) * t1);
            glBegin(GL_QUADS);
            glVertex3f(innerX0, poolY + 0.08f, z0);
            glVertex3f(outerX0, poolY + 0.08f, z0);
            glVertex3f(outerX1, poolY + 0.08f, z1);
            glVertex3f(innerX1, poolY + 0.08f, z1);
            glEnd();
        }
    }
    glDisable(GL_BLEND);
}

void drawBuilding(const Building& b) {
    float c = cloudCover;
    float baseY = cityYatZ(b.z);
    glColor3f(b.r * (1 - c*0.35f), b.g * (1 - c*0.35f), b.b * (1 - c*0.35f));
    glPushMatrix();
    glTranslatef(b.x, baseY + b.h/2.0f, b.z);
    glScalef(b.w, b.h, b.d); drawCube(); glPopMatrix();
    
    glColor3f(b.r*0.85f*(1-c*0.35f), b.g*0.85f*(1-c*0.35f), b.b*0.85f*(1-c*0.35f));
    glPushMatrix();
    glTranslatef(b.x, baseY + b.h + 0.35f, b.z);
    glScalef(b.w + 0.4f, 0.7f, b.d + 0.4f); drawCube(); glPopMatrix();
    
    float floorH = b.h / b.floors;
    for (int f = 0; f < b.floors; f++) {
        float yBase = baseY + f * floorH + floorH * 0.30f;
        float yTop  = yBase + floorH * 0.45f;
        int segs = (int)(b.w / 2.2f);
        float on = lightIntensity * darkness();
        float cr = 0.50f*(1-on) + 1.00f*on;
        float cg = 0.55f*(1-on) + 0.88f*on;
        float cb = 0.65f*(1-on) + 0.55f*on;
        glColor3f(cr, cg, cb);
        for (int s = 0; s < segs; s++) {
            float xw = b.x - b.w*0.5f + 0.8f + s * (b.w / segs);
            glPushMatrix();
            glTranslatef(xw + 0.6f, (yBase+yTop)*0.5f, b.z + b.d*0.5f + 0.05f);
            glScalef(b.w/segs - 0.7f, yTop - yBase, 0.08f); drawCube(); glPopMatrix();
            glPushMatrix();
            glTranslatef(xw + 0.6f, (yBase+yTop)*0.5f, b.z - b.d*0.5f - 0.05f);
            glScalef(b.w/segs - 0.7f, yTop - yBase, 0.08f); drawCube(); glPopMatrix();
        }
    }
}

void drawBuildings() { for (auto& b : buildings) drawBuilding(b); }

const float DAM_X1 = -25.0f;
const float DAM_X2 =  25.0f;

void drawGate() {
    float c = cloudCover;
    glColor3f(0.66f - c*0.26f, 0.64f - c*0.26f, 0.60f - c*0.26f);
    glPushMatrix();
    glTranslatef(0, GATE_Y - 1.5f, GATE_Z + 0.6f);
    glScalef(DAM_X2 - DAM_X1, 3.0f, 1.2f); drawCube(); glPopMatrix();
    
    glColor3f(0.78f - c*0.28f, 0.76f - c*0.28f, 0.72f - c*0.28f);
    glPushMatrix();
    glTranslatef(0, GATE_Y + 0.1f, GATE_Z + 0.6f);
    glScalef(DAM_X2 - DAM_X1 + 1.0f, 0.3f, 1.6f); drawCube(); glPopMatrix();
    
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
    if (gateOpen > 0.1f) {
        glColor3f(0.2f, 1.0f, 0.3f);
        glPushMatrix(); glTranslatef(-10.0f, GATE_Y + 3.2f, GATE_Z + 0.6f); sphere(0.45f); glPopMatrix();
    } else if (drainLevel > 0.80f) {
        float blink = (sinf(timeAcc * 8.0f) > 0.0f) ? 1.0f : 0.15f;
        glColor3f(1.0f * blink, 0.15f * blink, 0.10f * blink);
        glPushMatrix(); glTranslatef(-10.0f, GATE_Y + 3.2f, GATE_Z + 0.6f); sphere(0.5f); glPopMatrix();
    } else {
        glColor3f(0.28f, 0.06f, 0.06f);
        glPushMatrix(); glTranslatef(-10.0f, GATE_Y + 3.2f, GATE_Z + 0.6f); sphere(0.45f); glPopMatrix();
    }
    glEnable(GL_LIGHTING);
}

const float TURBINE_R = 2.4f;
const float TURBINE_WHEEL_Y = RIVER_Y + 0.45f;

void drawCircularTurbineWheel() {
    glColor3f(0.30f, 0.32f, 0.36f);
    glPushMatrix(); glRotatef(90, 0, 1, 0); cylinder(TURBINE_R * 0.20f, 1.6f); glPopMatrix();
    glColor3f(0.38f, 0.40f, 0.44f);
    glPushMatrix(); glTranslatef(-0.8f, 0, 0); glRotatef(90, 0, 1, 0); disc(TURBINE_R); glPopMatrix();
    glPushMatrix(); glTranslatef( 0.8f, 0, 0); glRotatef(90, 0, 1, 0); disc(TURBINE_R); glPopMatrix();
    
    glPushMatrix();
    glRotatef(turbineAngle, 1, 0, 0);
    const int N = 12;
    for (int i = 0; i < N; i++) {
        glPushMatrix();
        glRotatef(i * (360.0f / N), 1, 0, 0);
        float bladeR = TURBINE_R * 0.78f;
        glColor3f(0.55f, 0.60f, 0.68f);
        glPushMatrix(); glTranslatef(0, bladeR, 0.25f);
        glRotatef(-15, 1, 0, 0); glScalef(1.4f, 1.5f, 0.55f); sphere(0.55f); glPopMatrix();
        glColor3f(0.42f, 0.48f, 0.55f);
        glPushMatrix(); glTranslatef(0, bladeR, -0.25f);
        glRotatef(15, 1, 0, 0); glScalef(1.4f, 1.5f, 0.55f); sphere(0.55f); glPopMatrix();
        glPopMatrix();
    }
    
    glColor3f(0.92f, 0.78f, 0.20f);
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glRotatef(i * 90.0f, 1, 0, 0);
        glPushMatrix();
        glTranslatef(0, TURBINE_R * 0.52f, 0);
        glScalef(0.5f, TURBINE_R * 0.85f, 0.16f); drawCube(); glPopMatrix();
        glPopMatrix();
    }
    glPopMatrix();
    
    glColor3f(0.85f, 0.10f, 0.10f);
    glPushMatrix(); glTranslatef( 0.82f, 0, 0); glRotatef(90, 0, 1, 0);  disc(TURBINE_R * 0.16f); glPopMatrix();
    glPushMatrix(); glTranslatef(-0.82f, 0, 0); glRotatef(-90, 0, 1, 0); disc(TURBINE_R * 0.16f); glPopMatrix();
}

void drawWaterfall() {
    float flow = fmaxf(turbineFlow, roadFlood * 0.75f);
    if (gateOpen < 0.1f || flow < 0.05f) return;
    float topY    = RIVER_Y + 1.3f;
    float bottomY = TURBINE_WHEEL_Y;
    float topZ    = GATE_Z + 1.5f;
    float bottomZ = TURBINE_Z + 2.0f;
    const int COLS = 32;
    float halfW = 1.15f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const int ROWS = 8;
    
    for (int i = 0; i < COLS; i++) {
        for (int j = 0; j < ROWS; j++) {
            float t0 = (float)i / COLS;
            float t1 = (float)(i + 1) / COLS;
            float u0 = (float)j / ROWS;
            float u1 = (float)(j + 1) / ROWS;
            float x00 = -halfW + t0 * 2.0f * halfW;
            float x10 = -halfW + t1 * 2.0f * halfW;
            float x01 = x00, x11 = x10;
            float w0 = sinf(t0 * 15.0f + timeAcc * 8.0f) * 0.15f;
            float w1 = sinf(t1 * 15.0f + timeAcc * 8.0f) * 0.15f;
            float z00 = topZ + (bottomZ - topZ) * u0 + w0 * u0;
            float z01 = topZ + (bottomZ - topZ) * u1 + w0 * u1;
            float z10 = topZ + (bottomZ - topZ) * u0 + w1 * u0;
            float z11 = topZ + (bottomZ - topZ) * u1 + w1 * u1;
            float y0 = topY + (bottomY - topY) * u0;
            float y1 = topY + (bottomY - topY) * u1;
            
            float edge = 1.0f - fabsf(t0 * 2.0f - 1.0f) * 0.35f;
            float streak = 0.5f + 0.5f * sinf(t0 * 20.0f + u0 * 10.0f + timeAcc * 10.0f);
            float m = streak * 0.8f + 0.2f;
            float r = DEEP_BLUE_R * (1 - m) + 0.10f * m;
            float g = DEEP_BLUE_G * (1 - m) + 0.34f * m;
            float b = DEEP_BLUE_B * (1 - m) + 0.82f * m;
            r *= edge; g *= edge; b *= edge;
            float a = (0.65f + 0.30f * edge) * flow;
            glColor4f(r, g, b, a);
            glBegin(GL_QUADS);
            glNormal3f(0, 0.7f, 0.5f);
            glVertex3f(x00, y0, z00); glVertex3f(x10, y0, z10);
            glVertex3f(x11, y1, z11); glVertex3f(x01, y1, z01);
            glEnd();
            
        }
    }
    
    glColor4f(0.10f, 0.34f, 0.82f, 0.90f * flow);
    glBegin(GL_QUADS);
    glVertex3f(-halfW, topY,        topZ); glVertex3f( halfW, topY,        topZ);
    glVertex3f( halfW, topY - 0.35f, topZ); glVertex3f(-halfW, topY - 0.35f, topZ);
    glEnd();
    
    float riverSurfaceY = RIVER_Y + ((riverLevel - 0.35f) / 0.5f) * 1.5f;
    float poolY = riverSurfaceY + 0.05f;
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 8; j++) {
            float xa = -halfW - 0.2f + (2.0f * halfW + 0.4f) * i / 12.0f;
            float xb = -halfW - 0.2f + (2.0f * halfW + 0.4f) * (i + 1) / 12.0f;
            float za = TURBINE_Z - 0.6f + 1.8f * j / 8.0f;
            float zb = TURBINE_Z - 0.6f + 1.8f * (j + 1) / 8.0f;
            float yA = poolY + ripple(xa, za, timeAcc, 0.10f, 1.8f, 5.0f);
            float yB = poolY + ripple(xb, za, timeAcc, 0.10f, 1.8f, 5.0f);
            float yC = poolY + ripple(xb, zb, timeAcc, 0.10f, 1.8f, 5.0f);
            float yD = poolY + ripple(xa, zb, timeAcc, 0.10f, 1.8f, 5.0f);
            float nx = (yA - yB) + (yD - yC);
            float nz = (yA - yD) + (yB - yC);
            float ny = 0.5f;
            float len = sqrtf(nx*nx + ny*ny + nz*nz) + 1e-5f;
            float fres = 1.0f - ny / len;
            if (fres < 0) fres = 0;
            float t = fres * 0.9f + 0.3f;
            float rr, gg, bb;
            waterColorAt(t, rr, gg, bb);
            glColor4f(rr, gg, bb, (0.55f + 0.35f * fres) * flow);
            glBegin(GL_QUADS);
            glNormal3f(nx/len, ny/len, nz/len);
            glVertex3f(xa, yA, za); glVertex3f(xb, yB, za);
            glVertex3f(xb, yC, zb); glVertex3f(xa, yD, zb);
            glEnd();
        }
    }
    
    glColor4f(0.18f, 0.46f, 0.92f, 0.75f * flow);
    for (int i = 0; i < 24; i++) {
        float phase = fmodf(timeAcc * 1.4f + i * 0.17f, 1.0f);
        float px = -halfW + (i % 8) * (2.0f * halfW / 7.0f);
        float pz = TURBINE_Z - 0.2f + (i / 8) * 0.3f;
        float py = poolY + phase * 1.2f;
        glPushMatrix();
        glTranslatef(px, py, pz);
        sphere(0.12f * (1.0f - phase * 0.7f));
        glPopMatrix();
    }
    glDisable(GL_BLEND);
}

void drawTailrace() {
    if (turbineFlow <= 0.03f && riverLevel <= 0.36f) return;
    float riverSurfaceY = RIVER_Y + ((riverLevel - 0.35f) / 0.5f) * 1.5f;
    float turbinePoolY = TURBINE_Y - TURBINE_R * 0.55f;
    float tailY = turbinePoolY + (riverSurfaceY - turbinePoolY) * 0.55f;
    float alpha = 0.58f + turbineFlow * 0.30f;
    drawWaterPlane(-3.8f, TURBINE_Z + 0.2f, 3.8f, RIVER_NEAR + 1.5f,
                   tailY, alpha, 0.08f, 1.5f, 4.0f, true, true, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(SKY_BLUE_R, SKY_BLUE_G, SKY_BLUE_B, 0.72f * (0.35f + turbineFlow));
    glBegin(GL_QUADS);
    glVertex3f(-3.6f, tailY + 0.08f, TURBINE_Z + 0.4f);
    glVertex3f( 3.6f, tailY + 0.08f, TURBINE_Z + 0.4f);
    glVertex3f( 3.6f, riverSurfaceY + 0.08f, RIVER_NEAR + 1.5f);
    glVertex3f(-3.6f, riverSurfaceY + 0.08f, RIVER_NEAR + 1.5f);
    glEnd();
    glDisable(GL_BLEND);
}

void drawTurbineSupport() {
    glColor3f(0.32f, 0.32f, 0.35f);
    for (int i = -1; i <= 1; i += 2) {
        glPushMatrix();
        glTranslatef(i * 3.2f, TURBINE_Y + 0.2f, TURBINE_Z);
        glScalef(0.4f, 4.5f, 0.4f); drawCube(); glPopMatrix();
    }
    glPushMatrix();
    glTranslatef(0, TURBINE_Y + 2.4f, TURBINE_Z);
    glScalef(6.8f, 0.3f, 0.3f); drawCube(); glPopMatrix();
}

void drawGeneratorHouse() {
    float c = cloudCover;
    glColor3f(0.55f - c*0.22f, 0.53f - c*0.22f, 0.50f - c*0.22f);
    glPushMatrix();
    glTranslatef(14.0f, TURBINE_Y - 0.5f, TURBINE_Z + 2.0f);
    glScalef(7.0f, 4.0f, 5.0f); drawCube(); glPopMatrix();
    
    glColor3f(0.45f - c*0.18f, 0.45f - c*0.18f, 0.48f - c*0.18f);
    glPushMatrix();
    glTranslatef(14.0f, TURBINE_Y + 1.7f, TURBINE_Z + 2.0f);
    glScalef(7.4f, 0.4f, 5.4f); drawCube(); glPopMatrix();
    
    glColor3f(0.32f, 0.32f, 0.36f);
    glPushMatrix();
    glTranslatef(14.0f, TURBINE_Y + 2.4f, TURBINE_Z + 2.0f);
    glRotatef(90, 0, 0, 1);
    glRotatef(generatorAngle, 0, 1, 0);
    cylinder(0.6f, 2.0f); glPopMatrix();
}

void drawRiver() {
    float t = (riverLevel - 0.35f) / 0.5f;
    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
    float surfaceY = RIVER_Y + t * 1.5f;
    glColor3f(0.18f, 0.16f, 0.12f);
    quadY(-80, RIVER_NEAR, 80, RIVER_FAR, RIVER_Y - 2.0f);
    drawWaterPlane(-80, RIVER_NEAR, 80, RIVER_FAR, surfaceY,
                   0.90f, 0.10f, 0.9f, 1.6f, true, true, 1.0f);
}

void drawPowerLines() {
    glDisable(GL_LIGHTING);
    if (powerOutput > 0.05f) glColor3f(1.0f, 0.85f, 0.30f);
    else                     glColor3f(0.12f, 0.12f, 0.12f);
    glLineWidth(1.8f);
    for (int k = -1; k <= 1; k++) {
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 24; i++) {
            float t = (float)i / 24.0f;
            float x = 14.0f + k * 1.0f;
            float z = (TURBINE_Z + 2.0f) + (-25.0f - (TURBINE_Z + 2.0f)) * t;
            float y = (TURBINE_Y + 2.5f) + (cityYatZ(-25.0f) + 5.0f - (TURBINE_Y + 2.5f)) * t
                      - sinf(t * M_PI) * 2.0f;
            glVertex3f(x, y, z);
        }
        glEnd();
    }
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

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
    
    float on = lightIntensity * (0.35f + darkness() * 0.9f);
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
    for (float z = -30.0f; z <= 10.0f; z += 14.0f) {
        drawStreetlight(-14.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
        drawStreetlight( 14.0f, z, LIGHT_PALETTE[idx % 6]); idx++;
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
    glEnable(GL_LIGHTING);
}

// --- CURVED ROAD (GOLI) WITH DRAINS ON BOTH SIDES ---
void drawCurvedRoadAndDrains() {
    float z1 = -32.0f, z2 = 12.0f;
    float roadHalfW = 4.5f; 
    float drainW = DRAIN_W;
    float kerbW = 0.5f;
    float drainDepth = DRAIN_DEPTH;
    float c = cloudCover;
    
    const int SEG = 80;
    for (int i = 0; i < SEG; i++) {
        float t0 = (float)i / SEG;
        float t1 = (float)(i + 1) / SEG;
        float za = z1 + (z2 - z1) * t0;
        float zb = z1 + (z2 - z1) * t1;
        
        float xa0 = -26.0f + sinf(za * 0.07f) * 9.0f;
        float xa1 = -26.0f + sinf(zb * 0.07f) * 9.0f;
        
        float dx = xa1 - xa0;
        float dz = zb - za;
        float len = sqrtf(dx*dx + dz*dz);
        if (len < 1e-5f) continue;
        float nx = -dz / len;
        float nz = dx / len;
        
        float ya = cityYatZ(za);
        float yb = cityYatZ(zb);
        
        float xl0 = xa0 - nx * roadHalfW;
        float zl0 = za  - nz * roadHalfW;
        float xr0 = xa0 + nx * roadHalfW;
        float zr0 = za  + nz * roadHalfW;
        
        float xl1 = xa1 - nx * roadHalfW;
        float zl1 = zb  - nz * roadHalfW;
        float xr1 = xa1 + nx * roadHalfW;
        float zr1 = zb  + nz * roadHalfW;
        
        glColor3f(0.14f - c*0.05f, 0.14f - c*0.05f, 0.15f - c*0.05f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(xl0, ya, zl0); glVertex3f(xr0, ya, zr0);
        glVertex3f(xr1, yb, zr1); glVertex3f(xl1, yb, zl1);
        glEnd();

        if (roadFlood > 0.01f) {
            const int WATER_SUB = 8;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (int k = 0; k < WATER_SUB; k++) {
                float u0 = (float)k / WATER_SUB;
                float u1 = (float)(k + 1) / WATER_SUB;
                float x00 = xl0 + (xr0 - xl0) * u0;
                float z00 = zl0 + (zr0 - zl0) * u0;
                float x10 = xl0 + (xr0 - xl0) * u1;
                float z10 = zl0 + (zr0 - zl0) * u1;
                float x01 = xl1 + (xr1 - xl1) * u0;
                float z01 = zl1 + (zr1 - zl1) * u0;
                float x11 = xl1 + (xr1 - xl1) * u1;
                float z11 = zl1 + (zr1 - zl1) * u1;
                float baseY0 = ya + 0.035f + roadFlood * 0.05f;
                float baseY1 = yb + 0.035f + roadFlood * 0.05f;
                float wy0 = baseY0 + ripple(x00, z00, timeAcc, 0.04f, 1.2f, 2.5f);
                float wy1 = baseY0 + ripple(x10, z10, timeAcc, 0.04f, 1.2f, 2.5f);
                float wy2 = baseY1 + ripple(x11, z11, timeAcc, 0.04f, 1.2f, 2.5f);
                float wy3 = baseY1 + ripple(x01, z01, timeAcc, 0.04f, 1.2f, 2.5f);
                float waterNx = (wy0 - wy1) + (wy3 - wy2);
                float waterNz = (wy0 - wy3) + (wy1 - wy2);
                float waterNy = 0.35f;
                float waterNormalLength = sqrtf(waterNx*waterNx + waterNy*waterNy + waterNz*waterNz) + 1e-5f;
                waterNx /= waterNormalLength;
                waterNy /= waterNormalLength;
                waterNz /= waterNormalLength;
                float whiteMix = 0.08f + roadFlood * 0.16f;
                float red = 0.10f * (1.0f - whiteMix) + whiteMix;
                float green = 0.34f * (1.0f - whiteMix) + whiteMix;
                float blue = 0.82f * (1.0f - whiteMix) + whiteMix;
                glColor4f(red, green, blue, 0.75f + roadFlood * 0.20f);
                glBegin(GL_QUADS);
                glNormal3f(waterNx, waterNy, waterNz);
                glVertex3f(x00, wy0, z00); glVertex3f(x10, wy1, z10);
                glVertex3f(x11, wy2, z11); glVertex3f(x01, wy3, z01);
                glEnd();
            }
            glDisable(GL_BLEND);
        }
        
        // Center line
        glColor3f(0.85f, 0.75f, 0.15f);
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(xa0 - nx*0.1f, ya+0.01f, za - nz*0.1f);
        glVertex3f(xa0 + nx*0.1f, ya+0.01f, za + nz*0.1f);
        glVertex3f(xa1 + nx*0.1f, yb+0.01f, zb + nz*0.1f);
        glVertex3f(xa1 - nx*0.1f, yb+0.01f, zb - nz*0.1f);
        glEnd();
        
        for (int side = -1; side <= 1; side += 2) {
            float xEdge0 = (side == -1) ? xl0 : xr0;
            float zEdge0 = (side == -1) ? zl0 : zr0;
            float xEdge1 = (side == -1) ? xl1 : xr1;
            float zEdge1 = (side == -1) ? zl1 : zr1;
            
            float xKerb0 = xEdge0 + nx * kerbW * side;
            float zKerb0 = zEdge0 + nz * kerbW * side;
            float xKerb1 = xEdge1 + nx * kerbW * side;
            float zKerb1 = zEdge1 + nz * kerbW * side;
            
            float xDrain0 = xKerb0 + nx * drainW * side;
            float zDrain0 = zKerb0 + nz * drainW * side;
            float xDrain1 = xKerb1 + nx * drainW * side;
            float zDrain1 = zKerb1 + nz * drainW * side;
            
            glColor3f(0.65f - c*0.20f, 0.63f - c*0.20f, 0.60f - c*0.20f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xEdge0, ya + 0.15f, zEdge0); glVertex3f(xKerb0, ya + 0.15f, zKerb0);
            glVertex3f(xKerb1, yb + 0.15f, zKerb1); glVertex3f(xEdge1, yb + 0.15f, zEdge1);
            glEnd();
            
            glColor3f(0.35f - c*0.12f, 0.33f - c*0.12f, 0.30f - c*0.12f);
            glBegin(GL_QUADS);
            glNormal3f(-nx * side, 0.3f, -nz * side);
            glVertex3f(xKerb0, ya + 0.15f, zKerb0); glVertex3f(xKerb1, yb + 0.15f, zKerb1);
            glVertex3f(xKerb1, yb - drainDepth, zKerb1); glVertex3f(xKerb0, ya - drainDepth, zKerb0);
            glEnd();
            
            glColor3f(0.08f, 0.08f, 0.09f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xKerb0, ya - drainDepth, zKerb0); glVertex3f(xDrain0, ya - drainDepth, zDrain0);
            glVertex3f(xDrain1, yb - drainDepth, zDrain1); glVertex3f(xKerb1, yb - drainDepth, zKerb1);
            glEnd();
            
            glColor3f(0.45f - c*0.15f, 0.43f - c*0.15f, 0.40f - c*0.15f);
            glBegin(GL_QUADS);
            glNormal3f(nx * side, 0.3f, nz * side);
            glVertex3f(xDrain0, ya - drainDepth, zDrain0); glVertex3f(xDrain1, yb - drainDepth, zDrain1);
            glVertex3f(xDrain1, yb + 0.15f, zDrain1); glVertex3f(xDrain0, ya + 0.15f, zDrain0);
            glEnd();
        }
    }
}

void drawCurvedDrainWater() {
    float waterLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    float z1 = -32.0f, z2 = 12.0f;
    float roadHalfW = 4.5f;
    float drainW = DRAIN_W;
    float kerbW = 0.5f;
    float drainDepth = DRAIN_DEPTH;
    
    const int SEG = 40;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // DEEP BLUE WATER
    glColor4f(DEEP_BLUE_R, DEEP_BLUE_G, DEEP_BLUE_B, 0.92f);
    
    for (int i = 0; i < SEG; i++) {
        float t0 = (float)i / SEG;
        float t1 = (float)(i + 1) / SEG;
        float za = z1 + (z2 - z1) * t0;
        float zb = z1 + (z2 - z1) * t1;
        
        float xa0 = -26.0f + sinf(za * 0.07f) * 9.0f;
        float xa1 = -26.0f + sinf(zb * 0.07f) * 9.0f;
        
        float dx = xa1 - xa0;
        float dz = zb - za;
        float len = sqrtf(dx*dx + dz*dz);
        if (len < 1e-5f) continue;
        float nx = -dz / len;
        float nz = dx / len;
        
        float ya = cityYatZ(za);
        float yb = cityYatZ(zb);
        float waterY0 = ya - drainDepth + waterLevel * (drainDepth - 0.1f);
        float waterY1 = yb - drainDepth + waterLevel * (drainDepth - 0.1f);
        
        for (int side = -1; side <= 1; side += 2) {
            float xEdge0 = (side == -1) ? (xa0 - nx * roadHalfW) : (xa0 + nx * roadHalfW);
            float zEdge0 = (side == -1) ? (za  - nz * roadHalfW) : (za  + nz * roadHalfW);
            
            float xKerb0 = xEdge0 + nx * kerbW * side;
            float zKerb0 = zEdge0 + nz * kerbW * side;
            float xDrain0 = xKerb0 + nx * drainW * side;
            float zDrain0 = zKerb0 + nz * drainW * side;
            
            float xEdge1 = (side == -1) ? (xa1 - nx * roadHalfW) : (xa1 + nx * roadHalfW);
            float zEdge1 = (side == -1) ? (zb  - nz * roadHalfW) : (zb  + nz * roadHalfW);
            
            float xKerb1 = xEdge1 + nx * kerbW * side;
            float zKerb1 = zEdge1 + nz * kerbW * side;
            float xDrain1 = xKerb1 + nx * drainW * side;
            float zDrain1 = zKerb1 + nz * drainW * side;
            
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(xKerb0 + nx*0.1f*side, waterY0, zKerb0 + nz*0.1f*side);
            glVertex3f(xDrain0 - nx*0.1f*side, waterY0, zDrain0 - nz*0.1f*side);
            glVertex3f(xDrain1 - nx*0.1f*side, waterY1, zDrain1 - nz*0.1f*side);
            glVertex3f(xKerb1 + nx*0.1f*side, waterY1, zKerb1 + nz*0.1f*side);
            glEnd();
        }
    }
    glDisable(GL_BLEND);
}

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
    char buf[220];
    glColor4f(0,0,0,0.55f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glVertex2f(8, 460); glVertex2f(640, 460); glVertex2f(640, 740); glVertex2f(8, 740);
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(1,1,1);
    text(14, 722, "=== DHAKA NALA + HYDRO (deep blue + sky blue water) ===");
    snprintf(buf, sizeof(buf), "Rain %s (%.2f)   Clouds %.2f", rainOn?"ON":"OFF", rainIntensity, cloudCover);
    text(14, 704, buf);
    glColor3f(SKY_BLUE_R, SKY_BLUE_G, SKY_BLUE_B);
    float visibleDrainLevel = fmaxf(drainLevel, DRAIN_BASE_WATER);
    snprintf(buf, sizeof(buf), "DRAIN FILL: %.0f%%", visibleDrainLevel * 100.0f);
    text(14, 687, buf);
    glColor3f(1,1,1);
    snprintf(buf, sizeof(buf), "ROAD FLOOD: %.0f%%   River %.2f", roadFlood * 100.0f, riverLevel);
    text(14, 670, buf);
    if (gateOpen < 0.15f) glColor3f(1.0f, 0.4f, 0.35f);
    else                  glColor3f(0.4f, 1.0f, 0.4f);
    snprintf(buf, sizeof(buf), "GATE: %s (%.2f)   Turbine flow: %.2f",
             (gateOpen < 0.15f) ? "CLOSED" : (gateOpen > 0.85f ? "OPEN" : "MOVING"), gateOpen, turbineFlow);
    text(14, 653, buf);
    if (powerOutput < 0.1f) glColor3f(1.0f, 0.5f, 0.4f);
    else                    glColor3f(0.5f, 1.0f, 0.5f);
    snprintf(buf, sizeof(buf), "POWER: %.1f MW     City lights: %.2f", powerOutput * 100.0f, lightIntensity);
    text(14, 636, buf);
    glColor3f(1,1,1);
    text(14, 613, "-- CONTROLS --");
    text(14, 596, "SPACE = OPEN / CLOSE DAM GATE");
    text(14, 579, "R = rain   C = clouds   D = fill drains   E = empty drains");
    text(14, 562, "F = flood roads   V = river high   B = river baseline");
    text(14, 545, "1 = CALM   2 = RAIN   3 = RAIN + GATE OPEN   0 = reset");
    text(14, 528, "Mouse: L-drag orbit  R-drag pan  wheel zoom");
    text(14, 511, "WASD pan  Q/X zoom  PgUp/PgDn altitude  ESC quit");
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void update(float dt) {
    timeAcc += dt;
    float k = 1.0f * dt; if (k > 1.0f) k = 1.0f;
    rainIntensity += (targetRain - rainIntensity) * (1.6f * dt);
    drainLevel    += (targetDrain - drainLevel) * k;
    roadFlood     += (targetRoad  - roadFlood)  * (0.8f * dt);
    riverLevel    += (targetRiver - riverLevel) * (0.8f * dt);
    cloudCover    += (targetCloud - cloudCover) * (1.6f * dt);
    gateOpen      += (targetGate  - gateOpen) * (1.8f * dt);
    
    // --- UPDATED LOGIC: DRAIN FILLS FIRST, THEN SPILLS TO ROAD ---
    if (rainIntensity > 0.05f) {
        if (targetDrain < 1.0f) targetDrain = fminf(1.0f, targetDrain + dt * 0.12f);
        if (targetRiver < 0.85f) targetRiver = fminf(0.85f, targetRiver + dt * 0.06f); // River rises with rain
        
        if (drainLevel > 0.90f) {
            float spill = (drainLevel - 0.90f) * 10.0f;
            targetRoad = fminf(1.0f, targetRoad + dt * 0.18f * spill);
        }
    } else {
        if (targetDrain > 0.02f) targetDrain = fmaxf(0.02f, targetDrain - dt * 0.08f);
        if (targetRiver > 0.35f) targetRiver = fmaxf(0.35f, targetRiver - dt * 0.04f);
        
        if (drainLevel < 0.15f && targetRoad > 0.0f) {
            targetRoad = fmaxf(0.0f, targetRoad - dt * 0.06f);
        }
    }
    
    if (gateOpen > 0.3f && drainLevel > 0.15f) {
        targetDrain = fmaxf(0.02f, targetDrain - dt * 0.18f * gateOpen);
        targetRoad  = fmaxf(0.0f,  targetRoad  - dt * 0.15f * gateOpen);
    }
    
    float gateFactor  = gateOpen;
    float waterFactor = fmaxf(0.0f, drainLevel - 0.10f) * 1.6f;
    float desiredFlow = gateFactor * waterFactor;
    if (desiredFlow > 1.0f) desiredFlow = 1.0f;
    turbineFlow += (desiredFlow - turbineFlow) * (2.0f * dt);
    turbineAngle   += turbineFlow * 340.0f * dt;
    if (turbineAngle > 360.0f) turbineAngle -= 360.0f;
    generatorAngle += turbineFlow * 680.0f * dt;
    if (generatorAngle > 360.0f) generatorAngle -= 360.0f;
    powerOutput += (turbineFlow - powerOutput) * (1.5f * dt);
    
    float targetLights = 0.0f;
    if (powerOutput > 0.05f)
        targetLights = fminf(1.0f, (powerOutput - 0.05f) * 1.4f);
    lightIntensity += (targetLights - lightIntensity) * (1.5f * dt);
    
    if (rainIntensity > 0.05f) {
        for (auto& r : rainDrops) {
            r.y -= r.speed * dt;
            r.x += windX * dt * 3.0f;
            if (r.y < -10.0f) {
                r.y = 40.0f + (rand()%800)/100.0f;
                r.x = (rand()%14000)/100.0f - 70.0f;
                r.z = (rand()%14000)/100.0f - 70.0f;
            }
        }
    }
}

void display() {
    float c = cloudCover;
    float clearR = 0.78f*(1-c) + 0.22f*c;
    float clearG = 0.85f*(1-c) + 0.24f*c;
    float clearB = 0.95f*(1-c) + 0.30f*c;
    glClearColor(clearR, clearG, clearB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    drawSkyDome();
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(50.0, 1100.0/750.0, 0.1, 800.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    
    float yaw = camYaw * M_PI/180.0f;
    float pit = camPitch * M_PI/180.0f;
    float cx = camTargetX + camDist*cosf(pit)*sinf(yaw);
    float cy = camTargetY + camDist*sinf(pit);
    float cz = camTargetZ + camDist*cosf(pit)*cosf(yaw);
    gluLookAt(cx, cy, cz, camTargetX, camTargetY, camTargetZ, 0,1,0);
    
    float ambScale = 1.0f - cloudCover * 0.55f;
    GLfloat lpos[] = { 40.0f, 100.0f, 30.0f, 1.0f };
    GLfloat amb[]  = { 0.35f*ambScale, 0.37f*ambScale, 0.42f*ambScale, 1.0f };
    GLfloat dif[]  = { 0.90f*ambScale, 0.90f*ambScale, 0.95f*ambScale, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    
    drawGround();
    drawCityFlood();
    drawRoads();
    drawCurvedRoadAndDrains(); // Draw curved road
    glPushMatrix();
    glScalef(-1.0f, 1.0f, 1.0f);
    drawCurvedRoadAndDrains();
    glPopMatrix();
    drawDrains();
    drawRoadFlood();
    drawRoadFlowToGate();
    drawDrainWater();
    drawCurvedDrainWater(); // Draw water in curved drains
    glPushMatrix();
    glScalef(-1.0f, 1.0f, 1.0f);
    drawCurvedDrainWater();
    glPopMatrix();
    drawDrainOverflow();
    drawGateCollectionPool();
    drawGate();
    drawRoadAndDrainFlowToTurbine();
    drawRiver();
    drawTurbineSupport();
    
    glPushMatrix();
    glTranslatef(0.0f, TURBINE_WHEEL_Y, TURBINE_Z + 0.3f);
    drawCircularTurbineWheel();
    glPopMatrix();
    drawWaterfall();
    
    drawGeneratorHouse();
    drawPowerLines();
    drawBuildings();
    drawStreetlights();
    drawClouds();
    drawRain();
    
    if (showHUD) drawHUD();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(50.0, (float)w/h, 0.1, 800.0);
    glMatrixMode(GL_MODELVIEW);
}

void mouseButton(int b, int s, int x, int y) {
    lastMouseX = x; lastMouseY = y;
    if (b == GLUT_LEFT_BUTTON)  lmb = (s == GLUT_DOWN);
    if (b == GLUT_RIGHT_BUTTON) rmb = (s == GLUT_DOWN);
    if (b == 3) { camDist -= 6.0f; if (camDist < 12.0f) camDist = 12.0f; }
    if (b == 4) { camDist += 6.0f; if (camDist > 500.0f) camDist = 500.0f; }
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
    if (rmb) {
        float scale = camDist * 0.0015f;
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
        case 'r': case 'R':
            rainOn = !rainOn;
            targetRain  = rainOn ? 1.0f : 0.0f;
            targetCloud = rainOn ? 0.95f : 0.10f;
            break;
        case 'c': case 'C': targetCloud = (targetCloud > 0.5f) ? 0.10f : 0.95f; break;
        case 'd': case 'D': targetDrain = 1.0f; break;
        case 'e': case 'E': targetDrain = 0.02f; break;
        case 'f': case 'F': targetRoad = (targetRoad > 0.5f) ? 0.0f : 1.0f; break;
        case 'v': case 'V': targetRiver = 0.85f; break;
        case 'b': case 'B': targetRiver = 0.35f; break;
        case '1':
            rainOn = false; targetRain = 0.0f; targetCloud = 0.10f;
            targetDrain = 0.02f; targetRoad = 0.0f; targetRiver = 0.35f;
            targetGate  = 0.0f; break;
        case '2':
            rainOn = true; targetRain = 1.0f; targetCloud = 0.95f;
            targetRiver = 0.60f; targetGate = 0.0f; break;
        case '3':
            rainOn = true; targetRain = 1.0f; targetCloud = 0.95f;
            targetDrain = 0.95f; targetRiver = 0.85f;
            targetGate  = 1.0f; break;
        case '0':
            rainOn = false; targetRain = 0.0f; targetCloud = 0.10f;
            targetDrain = 0.02f; targetRoad = 0.0f; targetRiver = 0.35f;
            targetGate  = 0.0f;
            camYaw = 35; camPitch = 28; camDist = 95;
            camTargetX = 0; camTargetZ = -5; break;
        case 'a': case 'A': camYaw -= 4.0f; break;
        case 'z': case 'Z': camYaw += 4.0f; break;
        case 'w': case 'W': camPitch += 3.0f; if (camPitch>89) camPitch=89; break;
        case 's': case 'S': camPitch -= 3.0f; if (camPitch<3)  camPitch=3;  break;
        case 'q': case 'Q': camDist -= 6.0f; if (camDist<12) camDist=12; break;
        case 'x': case 'X': camDist += 6.0f; if (camDist>500) camDist=500; break;
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
        case GLUT_KEY_PAGE_DOWN: camTargetY -= 1.0f; if (camTargetY<0) camTargetY=0; break;
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
    buildDrains(); buildBuildings(); initRain();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1100, 750);
    glutCreateWindow("Dhaka Nala — Deep Blue + Sky Blue Water");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMove);
    glutPassiveMotionFunc(mouseMove);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutTimerFunc(0, timer, 0);
    printf("H: HUD | R: rain | Space: gate | 0: reset | Esc: quit\n");
    glutMainLoop();
    return 0;
}
