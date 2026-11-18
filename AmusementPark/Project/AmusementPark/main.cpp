// =====================================================================
//  3D AMUSEMENT PARK  -  C++ / OpenGL / freeglut   (Code::Blocks)
//
//  Keys:   N = toggle Day <-> Sunset      D = Day     S = Sunset
//          SPACE = pause                  ESC = quit
//
//  Code::Blocks linker settings (Project > Build options > Linker settings):
//      -lfreeglut  -lopengl32  -lglu32  -lwinmm  -lgdi32
//  (in this order; make sure freeglut is installed for your MinGW)
// =====================================================================
#include <GL/freeglut.h>
#include <GL/glu.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static const float PI_F = 3.14159265f;
static const float DEG  = 180.0f / 3.14159265f;

// ------------------------- global state ------------------------------
bool  targetSunset = false;   // what the user asked for
bool  isPaused     = false;
float duskRaw = 0.0f;         // linear 0..1 transition progress
float dusk    = 0.0f;         // eased 0 (day) .. 1 (sunset)
float globalTime = 0.0f;
int   winW = 1280, winH = 800;

// fixed camera
float camX = 0.0f,  camY = 14.0f, camZ = 50.0f;
float lookX = 0.0f, lookY = 11.0f, lookZ = -12.0f;

// animation values
float ferrisAngle = 0.0f, carouselAngle = 0.0f, coasterS = 0.0f;
float hotAirY = 12.0f, hotAirX = 24.0f;
float sunPulse = 0.0f, birdWing = 0.0f, limbSwing = 0.0f;

// ------------------------- small helpers -----------------------------
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static void  lerp3(const float* a, const float* b, float t, float* o) { for (int i = 0; i < 3; i++) o[i] = lerpf(a[i], b[i], t); }
static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static void  col3(const float* c) { glColor3f(c[0], c[1], c[2]); }

static void setEmission(float r, float g, float b) { GLfloat e[4] = {r, g, b, 1.0f}; glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e); }
static void clearEmission() { setEmission(0, 0, 0); }

static void box(float w, float h, float d) { glPushMatrix(); glScalef(w, h, d); glutSolidCube(1.0); glPopMatrix(); }
static void cylY(float r, float h, int n = 12) { glPushMatrix(); glRotatef(-90, 1, 0, 0); glutSolidCylinder(r, h, n, 1); glPopMatrix(); }
static void ballAt(float x, float y, float z, float r, int n = 12) { glPushMatrix(); glTranslatef(x, y, z); glutSolidSphere(r, n, n); glPopMatrix(); }

// cylinder between two points
static void limb(float x1, float y1, float z1, float x2, float y2, float z2, float r, int slices = 8) {
    float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f) return;
    glPushMatrix();
    glTranslatef(x1, y1, z1);
    float ax = -dy, ay = dx;
    if (sqrtf(ax * ax + ay * ay) > 1e-5f) glRotatef(acosf(dz / len) * DEG, ax, ay, 0);
    else if (dz < 0) glRotatef(180, 1, 0, 0);
    glutSolidCylinder(r, len, slices, 1);
    glPopMatrix();
}

// additive glow
static void beginGlow() { glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE); }
static void endGlow()   { glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING); }
static void glowHere(float radius, const float* c, float alpha) {
    if (alpha < 0.02f) return;
    beginGlow();
    glColor4f(c[0], c[1], c[2], alpha);        glutSolidSphere(radius, 10, 8);
    glColor4f(c[0], c[1], c[2], alpha * 0.5f); glutSolidSphere(radius * 1.8f, 10, 8);
    endGlow();
}

// emissive light bulb (+ halo at sunset)
static void bulb(float x, float y, float z, float r, const float* c, float level, bool halo = true) {
    glPushMatrix();
    glTranslatef(x, y, z);
    setEmission(c[0] * level, c[1] * level, c[2] * level);
    glColor3f(c[0], c[1], c[2]);
    glutSolidSphere(r, 7, 7);
    clearEmission();
    if (halo && dusk > 0.15f) glowHere(r * 3.0f, c, 0.35f * dusk * level);
    glPopMatrix();
}

static const float BULB_COL[4][3] = {{1.0f, 0.25f, 0.25f}, {1.0f, 0.9f, 0.25f}, {0.3f, 1.0f, 0.45f}, {0.4f, 0.65f, 1.0f}};

// stroke-font sign text (centered, unlit)
static void signText(const char* s, float width, float r, float g, float b) {
    glDisable(GL_LIGHTING);
    glColor3f(r, g, b);
    glLineWidth(3.0f);
    float len = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)s);
    float sc = width / len;
    for (int pass = 0; pass < 2; pass++) {
        glPushMatrix();
        glScalef(sc, sc, sc);
        glTranslatef(-len * 0.5f + pass * 5.0f, -50.0f, 0);
        for (const char* c = s; *c; ++c) glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
        glPopMatrix();
    }
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// striped band / cone / sphere helpers
static void stripedBand(float r, float y0, float h, int n, const float* c1, const float* c2, int skip = -1) {
    glBegin(GL_QUADS);
    for (int i = 0; i < n; i++) {
        if (i == skip) continue;
        float a1 = i * 2 * PI_F / n, a2 = (i + 1) * 2 * PI_F / n, am = (a1 + a2) * 0.5f;
        col3(i % 2 ? c2 : c1);
        glNormal3f(cosf(am), 0, sinf(am));
        glVertex3f(r * cosf(a1), y0, r * sinf(a1));
        glVertex3f(r * cosf(a2), y0, r * sinf(a2));
        glVertex3f(r * cosf(a2), y0 + h, r * sinf(a2));
        glVertex3f(r * cosf(a1), y0 + h, r * sinf(a1));
    }
    glEnd();
}
static void stripedCone(float rb, float yb, float h, int n, const float* c1, const float* c2) {
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < n; i++) {
        float a1 = i * 2 * PI_F / n, a2 = (i + 1) * 2 * PI_F / n, am = (a1 + a2) * 0.5f;
        col3(i % 2 ? c2 : c1);
        glNormal3f(cosf(am) * h, rb, sinf(am) * h);
        glVertex3f(0, yb + h, 0);
        glVertex3f(rb * cosf(a1), yb, rb * sinf(a1));
        glVertex3f(rb * cosf(a2), yb, rb * sinf(a2));
    }
    glEnd();
}
static void stripedSphere(float r, int sl, int st, const float* c1, const float* c2) {
    for (int j = 0; j < sl; j++) {
        float a1 = j * 2 * PI_F / sl, a2 = (j + 1) * 2 * PI_F / sl;
        col3(j % 2 ? c2 : c1);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= st; i++) {
            float p = -PI_F / 2 + PI_F * i / st, cp = cosf(p), sp = sinf(p);
            glNormal3f(cp * cosf(a1), sp, cp * sinf(a1)); glVertex3f(r * cp * cosf(a1), r * sp, r * cp * sinf(a1));
            glNormal3f(cp * cosf(a2), sp, cp * sinf(a2)); glVertex3f(r * cp * cosf(a2), r * sp, r * cp * sinf(a2));
        }
        glEnd();
    }
}

// ------------------------- scene data --------------------------------
struct Balloon { float x, y, z, speedY, speedX, r, g, b; };
struct Cloud   { float x, y, z, speed, scale; };
struct Bird    { float x, y, z, speed, phase; };
struct Bldg    { float x, z, w, h, d, jit; int id; };
struct Tree    { float x, z, s; int pal; };
struct P3      { float x, y, z, t; };
struct TP      { float x, y, z, nx, nz, t; };

std::vector<Balloon> floatingBalloons;
std::vector<Cloud>   clouds;
std::vector<Bird>    birds;
std::vector<Bldg>    cityNear, cityFar;
std::vector<Tree>    trees;

// ------------------------- roller-coaster track ----------------------
static const float CX = 4.0f, CZ = -21.0f, RX = 24.0f, RZ = 9.0f;
static const float LOOP_C = 1.5f, LOOP_W = 0.16f, LOOP_R = 5.5f;
std::vector<P3> trackFine;   // fine polyline
std::vector<float> trackCum; // cumulative length
float trackLen = 1.0f;
std::vector<TP> railPts;     // evenly spaced points for drawing

static void coasterRaw(float t, float& x, float& y, float& z) {
    x = CX + RX * cosf(t);
    z = CZ + RZ * sinf(t);
    y = 4.5f + 2.5f * sinf(2.0f * t);
    float d = t - LOOP_C;
    if (d > -LOOP_W && d < LOOP_W) {                 // vertical loop (follows direction of travel)
        float u = (d + LOOP_W) / (2.0f * LOOP_W);
        float th = u * 2.0f * PI_F;
        float tx = -RX * sinf(LOOP_C), tz = RZ * cosf(LOOP_C);
        float tl = sqrtf(tx * tx + tz * tz); tx /= tl; tz /= tl;
        float nx = tz, nz = -tx;
        float off = LOOP_R * sinf(th);
        float lat = 1.8f * sinf(PI_F * u);
        x += tx * off + nx * lat;
        z += tz * off + nz * lat;
        y += LOOP_R * (1.0f - cosf(th));
    }
}

static void trackAt(float s, P3& p) {
    s = fmodf(s, trackLen); if (s < 0) s += trackLen;
    size_t i = std::upper_bound(trackCum.begin(), trackCum.end(), s) - trackCum.begin();
    if (i == 0) i = 1;
    if (i >= trackFine.size()) i = trackFine.size() - 1;
    float s0 = trackCum[i - 1], s1 = trackCum[i];
    float f = (s1 > s0) ? (s - s0) / (s1 - s0) : 0.0f;
    const P3& a = trackFine[i - 1]; const P3& b = trackFine[i];
    p.x = lerpf(a.x, b.x, f); p.y = lerpf(a.y, b.y, f); p.z = lerpf(a.z, b.z, f); p.t = lerpf(a.t, b.t, f);
}

static void buildTrack() {
    const int N = 4000;
    trackFine.clear(); trackCum.clear();
    for (int i = 0; i <= N; i++) {
        P3 p; p.t = 2.0f * PI_F * i / N;
        coasterRaw(p.t, p.x, p.y, p.z);
        trackFine.push_back(p);
        if (i == 0) trackCum.push_back(0);
        else {
            const P3& q = trackFine[i - 1];
            trackCum.push_back(trackCum[i - 1] + sqrtf((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z)));
        }
    }
    trackLen = trackCum.back();
    railPts.clear();
    int M = (int)(trackLen / 0.8f);
    for (int i = 0; i < M; i++) {
        P3 p; trackAt(trackLen * i / M, p);
        TP q; q.x = p.x; q.y = p.y; q.z = p.z; q.t = p.t;
        float nx = RZ * cosf(p.t), nz = RX * sinf(p.t), nl = sqrtf(nx * nx + nz * nz);
        q.nx = nx / nl; q.nz = nz / nl;
        railPts.push_back(q);
    }
}

// ------------------------- initialisation ----------------------------
void initScene() {
    srand(7);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glShadeModel(GL_SMOOTH);

    // floating balloons
    float colors[5][3] = {{1.0f, 0.25f, 0.3f}, {0.2f, 0.8f, 0.95f}, {1.0f, 0.82f, 0.15f}, {0.95f, 0.45f, 0.75f}, {0.6f, 0.35f, 0.95f}};
    for (int i = 0; i < 12; i++) {
        Balloon b;
        b.x = -28.0f + (rand() % 56); b.y = 3.0f + (rand() % 22); b.z = -10.0f + (rand() % 24);
        b.speedY = 0.02f + (rand() % 10) * 0.0025f; b.speedX = ((rand() % 10) - 5) * 0.003f;
        b.r = colors[i % 5][0]; b.g = colors[i % 5][1]; b.b = colors[i % 5][2];
        floatingBalloons.push_back(b);
    }
    // clouds (far, high in the sky)
    for (int i = 0; i < 8; i++) {
        Cloud c;
        c.x = -75.0f + i * 20.0f; c.y = 30.0f + (i % 3) * 4.0f; c.z = -40.0f - (i % 3) * 9.0f;
        c.speed = 0.02f + (i % 3) * 0.008f; c.scale = 2.0f + (i % 2) * 0.8f;
        clouds.push_back(c);
    }
    // birds
    for (int i = 0; i < 7; i++) {
        Bird b;
        b.x = -45.0f + i * 14.0f; b.y = 27.0f + (i % 3) * 3.0f; b.z = -22.0f - (i % 4) * 4.0f;
        b.speed = 0.07f + (i % 2) * 0.025f; b.phase = i * 0.9f;
        birds.push_back(b);
    }
    // city silhouette (two layers)
    { float x = -150.0f; int id = 0;
      while (x < 150.0f) { Bldg b; b.w = 5.0f + rand() % 6; b.h = 15.0f + rand() % 20; b.d = 5.0f; b.z = -75.0f; b.x = x + b.w / 2; b.jit = ((rand() % 100) / 100.0f - 0.5f) * 0.08f; b.id = id++; cityNear.push_back(b); x += b.w + 0.6f + rand() % 3; }
      x = -160.0f;
      while (x < 160.0f) { Bldg b; b.w = 8.0f + rand() % 8; b.h = 26.0f + rand() % 22; b.d = 8.0f; b.z = -100.0f; b.x = x + b.w / 2; b.jit = ((rand() % 100) / 100.0f - 0.5f) * 0.06f; b.id = id++; cityFar.push_back(b); x += b.w + 1.0f + rand() % 4; }
    }
    // trees: two staggered rows behind the lamps + a few at the sides
    for (float x = -105.0f; x <= 105.0f; x += 5.5f) { Tree t; t.x = x + (rand() % 20) / 10.0f; t.z = -41.0f; t.s = 0.9f + (rand() % 5) * 0.08f; t.pal = rand() % 5; trees.push_back(t); }
    for (float x = -102.0f; x <= 105.0f; x += 6.0f) { Tree t; t.x = x; t.z = -47.0f; t.s = 1.1f + (rand() % 5) * 0.08f; t.pal = rand() % 5; trees.push_back(t); }
    float sideT[6][2] = {{-38, -14}, {-39, 2}, {-37, 16}, {38, -14}, {39, 4}, {37, 17}};
    for (int i = 0; i < 6; i++) { Tree t; t.x = sideT[i][0]; t.z = sideT[i][1]; t.s = 1.0f; t.pal = i % 5; trees.push_back(t); }

    buildTrack();
}

// ------------------------- sky, sun ----------------------------------
static void skyColors(float* hor, float* top) {
    const float dh[3] = {0.74f, 0.89f, 1.00f}, dt[3] = {0.20f, 0.50f, 0.95f};
    const float sh[3] = {1.00f, 0.60f, 0.22f}, st[3] = {0.45f, 0.30f, 0.60f};
    lerp3(dh, sh, dusk, hor); lerp3(dt, st, dusk, top);
}
static void sunPos(float& x, float& y, float& z) { x = 10.0f; y = lerpf(66.0f, 25.0f, dusk); z = -110.0f; }

void drawSkyDome() {
    float hor[3], top[3]; skyColors(hor, top);
    float sx, sy, sz; sunPos(sx, sy, sz);
    float sd[3] = {sx - camX, sy - camY, sz - camZ};
    float sl = sqrtf(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]);
    sd[0] /= sl; sd[1] /= sl; sd[2] /= sl;

    glPushMatrix();
    glTranslatef(camX, camY, camZ);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    const int stacks = 20, slices = 40; const float R = 200.0f;
    for (int i = 0; i < stacks; i++) {
        float e1 = -0.15f + (PI_F / 2 + 0.15f) * i / stacks;
        float e2 = -0.15f + (PI_F / 2 + 0.15f) * (i + 1) / stacks;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lon = 2 * PI_F * j / slices;
            for (int k = 0; k < 2; k++) {
                float e = k == 0 ? e2 : e1;
                float dx = cosf(e) * cosf(lon), dy = sinf(e), dz = cosf(e) * sinf(lon);
                float f = powf(e > 0 ? e / (PI_F / 2) : 0.0f, 0.55f);
                float c[3]; lerp3(hor, top, f, c);
                float dt = dx * sd[0] + dy * sd[1] + dz * sd[2]; if (dt < 0) dt = 0;
                float glow = powf(dt, 10.0f) * (0.25f + 0.5f * dusk) + powf(dt, 3.0f) * 0.35f * dusk;
                glColor3f(clamp01(c[0] + glow), clamp01(c[1] + glow * 0.55f), clamp01(c[2] + glow * 0.18f));
                glVertex3f(R * dx, R * dy, R * dz);
            }
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glPopMatrix();
}

void drawSun() {
    float sx, sy, sz; sunPos(sx, sy, sz);
    float pulse = 0.5f + 0.5f * sinf(sunPulse);
    float r = lerpf(8.0f, 10.0f, dusk);
    const float cd[3] = {1.0f, 0.95f, 0.50f}, cs[3] = {1.0f, 0.45f, 0.12f};
    float c[3]; lerp3(cd, cs, dusk, c);
    glPushMatrix();
    glTranslatef(sx, sy, sz);
    glDisable(GL_LIGHTING);
    glColor3f(c[0], c[1], c[2]);
    glutSolidSphere(r, 28, 28);
    beginGlow();
    for (int k = 0; k < 4; k++) {                           // pulsing halo
        float rad = r * (1.35f + 0.75f * k) * (1.0f + 0.07f * pulse);
        glColor4f(c[0], c[1], c[2], (0.24f - 0.05f * k) * (0.75f + 0.5f * pulse));
        glutSolidSphere(rad, 20, 20);
    }
    glRotatef(sunPulse * 12.0f, 0, 0, 1);                   // rotating rays
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 16; i++) {
        float a = i * 2 * PI_F / 16, w = 0.10f;
        float len = r * ((i % 2) ? 2.2f : 3.0f) * (1.0f + 0.15f * pulse);
        glColor4f(c[0], c[1], c[2], 0.28f);
        glVertex3f(r * 1.0f * cosf(a - w), r * sinf(a - w), 0);
        glVertex3f(r * 1.0f * cosf(a + w), r * sinf(a + w), 0);
        glColor4f(c[0], c[1], c[2], 0.0f);
        glVertex3f(len * cosf(a), len * sinf(a), 0);
    }
    glEnd();
    endGlow();
    glPopMatrix();
}

// ------------------------- far city silhouette ------------------------
void drawCity() {
    float hor[3], top[3]; skyColors(hor, top);
    const float dd[3] = {0.30f, 0.38f, 0.52f}, ds[3] = {0.24f, 0.10f, 0.20f};
    float dark[3]; lerp3(dd, ds, dusk, dark);
    glDisable(GL_LIGHTING);
    for (int layer = 1; layer >= 0; layer--) {
        const std::vector<Bldg>& v = layer ? cityFar : cityNear;
        float m = layer ? 0.62f : 0.30f;
        for (size_t i = 0; i < v.size(); i++) {
            const Bldg& b = v[i];
            glColor3f(clamp01(hor[0] * m + dark[0] * (1 - m) + b.jit), clamp01(hor[1] * m + dark[1] * (1 - m) + b.jit), clamp01(hor[2] * m + dark[2] * (1 - m) + b.jit));
            glPushMatrix();
            glTranslatef(b.x, b.h / 2, b.z);
            glScalef(b.w, b.h, b.d);
            glutSolidCube(1.0);
            glPopMatrix();
            if (b.id % 4 == 0) { glPushMatrix(); glTranslatef(b.x, b.h + 2.0f, b.z); glScalef(0.3f, 4.0f, 0.3f); glutSolidCube(1.0); glPopMatrix(); }
        }
    }
    // lit windows in the evening
    float k = clamp01((dusk - 0.3f) * 1.6f);
    if (k > 0.01f) {
        glBegin(GL_QUADS);
        for (size_t i = 0; i < cityNear.size(); i++) {
            const Bldg& b = cityNear[i];
            int r = 0;
            for (float wy = 3.0f; wy < b.h - 1.5f; wy += 2.8f, r++) {
                int c = 0;
                for (float wx = b.x - b.w / 2 + 1.0f; wx < b.x + b.w / 2 - 0.9f; wx += 1.6f, c++) {
                    if (((b.id * 13 + r * 7 + c * 3) % 5) >= 3) continue;
                    glColor3f(lerpf(0.4f, 1.0f, k), lerpf(0.25f, 0.82f, k), lerpf(0.3f, 0.40f, k));
                    float z = b.z + b.d / 2 + 0.05f;
                    glVertex3f(wx - 0.4f, wy - 0.6f, z); glVertex3f(wx + 0.4f, wy - 0.6f, z);
                    glVertex3f(wx + 0.4f, wy + 0.6f, z); glVertex3f(wx - 0.4f, wy + 0.6f, z);
                }
            }
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);
}

// ------------------------- ground, trees, lamps -----------------------
void drawGround() {
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glColor3f(0.20f, 0.55f, 0.20f);
    glVertex3f(-300, 0, -300); glVertex3f(300, 0, -300); glVertex3f(300, 0, 300); glVertex3f(-300, 0, 300);
    glColor3f(0.45f, 0.43f, 0.42f);                       // promenade where the lamps stand
    glVertex3f(-300, 0.02f, -37); glVertex3f(300, 0.02f, -37); glVertex3f(300, 0.02f, -31); glVertex3f(-300, 0.02f, -31);
    glColor3f(0.64f, 0.57f, 0.50f);                       // park plaza
    glVertex3f(-40, 0.03f, -8); glVertex3f(40, 0.03f, -8); glVertex3f(40, 0.03f, 24); glVertex3f(-40, 0.03f, 24);
    glEnd();
}

void drawTrees() {
    static const float pal[5][3] = {{0.30f, 0.58f, 0.16f}, {0.62f, 0.68f, 0.14f}, {0.85f, 0.72f, 0.15f}, {0.45f, 0.62f, 0.12f}, {0.90f, 0.55f, 0.12f}};
    for (size_t i = 0; i < trees.size(); i++) {
        const Tree& t = trees[i];
        glPushMatrix();
        glTranslatef(t.x, 0, t.z);
        glColor3f(0.42f, 0.26f, 0.12);
        cylY(0.42f * t.s, 3.6f * t.s, 8);
        col3(pal[t.pal]);
        ballAt(0, 5.0f * t.s, 0, 2.7f * t.s, 10);
        ballAt(0, 7.1f * t.s, 0, 1.9f * t.s, 10);
        ballAt(1.6f * t.s, 4.5f * t.s, 0.6f * t.s, 1.7f * t.s, 10);
        ballAt(-1.6f * t.s, 4.7f * t.s, -0.4f * t.s, 1.6f * t.s, 10);
        glPopMatrix();
    }
}

void drawLamp(float x, float z) {
    float level = 0.25f + 0.95f * dusk;
    const float warm[3] = {1.0f, 0.85f, 0.4f};
    glPushMatrix();
    glTranslatef(x, 0, z);
    glColor3f(0.10f, 0.12f, 0.14f);
    glPushMatrix(); glTranslatef(0, 0.25f, 0); box(0.8f, 0.5f, 0.8f); glPopMatrix();
    cylY(0.17f, 4.4f, 8);
    glTranslatef(0, 4.4f, 0);
    box(0.8f, 0.14f, 0.8f);
    glPushMatrix();                                       // glass lantern
    glTranslatef(0, 0.7f, 0); glScalef(0.5f, 0.75f, 0.5f);
    setEmission(1.0f * level, 0.85f * level, 0.4f * level);
    glColor3f(1.0f, 0.92f, 0.55f);
    glutSolidSphere(0.6, 12, 10);
    clearEmission();
    glPopMatrix();
    glColor3f(0.10f, 0.12f, 0.14f);                       // roof + finial
    glPushMatrix(); glTranslatef(0, 1.0f, 0); glRotatef(-90, 1, 0, 0); glutSolidCone(0.6, 0.55, 10, 1); glPopMatrix();
    ballAt(0, 1.65f, 0, 0.1f, 8);
    if (dusk > 0.05f) { glPushMatrix(); glTranslatef(0, 0.7f, 0); glowHere(1.4f, warm, 0.45f * dusk); glPopMatrix(); }
    glPopMatrix();
}
void drawLamps() {
    for (float x = -108.0f; x <= 108.0f; x += 9.0f) drawLamp(x, -34.0f);
    float fl[4][2] = {{-30, 20}, {-12, 21}, {12, 21}, {30, 20}};            // a few lamps in the plaza
    for (int i = 0; i < 4; i++) drawLamp(fl[i][0], fl[i][1]);
}

void drawFlowers() {
    for (float fx = -36.0f; fx <= 36.0f; fx += 2.2f) {
        if (fabsf(fx - 20.0f) < 9.0f || fabsf(fx + 24.0f) < 7.0f) continue;
        glPushMatrix();
        glTranslatef(fx, 0.25f, -8.8f);
        glColor3f(0.95f, 0.25f, 0.45f); glutSolidSphere(0.32, 8, 8);
        glTranslatef(0.7f, 0, 0.3f);
        glColor3f(1.0f, 0.85f, 0.15f); glutSolidSphere(0.30, 8, 8);
        glPopMatrix();
    }
}

// ------------------------- sky objects --------------------------------
void drawClouds() {
    const float c0[3] = {1.0f, 1.0f, 1.0f}, c1[3] = {1.0f, 0.74f, 0.60f};
    float c[3]; lerp3(c0, c1, dusk, c);
    static const float puff[7][5] = {{0, 0, 0, 2.2f, 1.0f}, {1.9f, -0.2f, 0.2f, 1.7f, 0.97f}, {-1.9f, -0.3f, 0.1f, 1.6f, 0.96f}, {0.8f, 0.8f, 0, 1.5f, 1.0f}, {-0.9f, 0.6f, 0.1f, 1.4f, 1.0f}, {3.3f, -0.5f, 0, 1.1f, 0.94f}, {-3.2f, -0.5f, 0, 1.0f, 0.94f}};
    glDisable(GL_LIGHTING);
    for (size_t i = 0; i < clouds.size(); i++) {
        const Cloud& cl = clouds[i];
        glPushMatrix();
        glTranslatef(cl.x, cl.y, cl.z);
        glScalef(cl.scale, cl.scale * 0.6f, cl.scale * 0.7f);
        for (int p = 0; p < 7; p++) {
            glColor3f(c[0] * puff[p][4], c[1] * puff[p][4], c[2] * puff[p][4]);
            ballAt(puff[p][0], puff[p][1], puff[p][2], puff[p][3], 14);
        }
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);
}

void drawBirds() {
    glDisable(GL_LIGHTING);
    glColor3f(0.12f, 0.12f, 0.16f);
    for (size_t i = 0; i < birds.size(); i++) {
        const Bird& bd = birds[i];
        float flap = 38.0f * sinf(globalTime * 10.0f + bd.phase);
        float bob = 0.6f * sinf(globalTime * 2.0f + bd.phase);
        glPushMatrix();
        glTranslatef(bd.x, bd.y + bob, bd.z);
        glScalef(1.8f, 1.8f, 1.8f);
        glPushMatrix(); glScalef(1.0f, 0.35f, 0.35f); glutSolidSphere(0.5, 8, 8); glPopMatrix();   // body
        ballAt(0.5f, 0.05f, 0, 0.16f, 8);                                                           // head
        glPushMatrix(); glTranslatef(0.65f, 0.03f, 0); glRotatef(90, 0, 1, 0); glutSolidCone(0.05, 0.2, 6, 1); glPopMatrix(); // beak
        glPushMatrix(); glTranslatef(-0.55f, 0, 0); box(0.3f, 0.04f, 0.25f); glPopMatrix();           // tail
        for (int side = -1; side <= 1; side += 2) {
            glPushMatrix();
            glRotatef(side * flap, 1, 0, 0);
            glTranslatef(0, 0, side * 0.55f);
            box(0.45f, 0.04f, 1.1f);
            glPopMatrix();
        }
        glPopMatrix();
    }
    glEnable(GL_LIGHTING);
}

void drawHotAirBalloon() {
    const float c1[3] = {0.90f, 0.20f, 0.25f}, c2[3] = {1.0f, 0.88f, 0.30f};
    glPushMatrix();
    glTranslatef(hotAirX, hotAirY, -52.0f);
    glScalef(1.6f, 1.6f, 1.6f);
    glColor3f(0.58f, 0.35f, 0.16f);
    box(1.4f, 1.0f, 1.4f);                                 // basket
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    for (int x = -1; x <= 1; x += 2) for (int z = -1; z <= 1; z += 2) { glVertex3f(x * 0.6f, 0.5f, z * 0.6f); glVertex3f(x * 1.3f, 2.9f, z * 1.3f); }
    glEnd();
    glTranslatef(0, 5.3f, 0);
    glScalef(1.0f, 1.2f, 1.0f);
    stripedSphere(3.2f, 12, 10, c1, c2);                   // striped envelope
    glPopMatrix();
}

void drawFloatingBalloons() {
    for (size_t i = 0; i < floatingBalloons.size(); i++) {
        const Balloon& b = floatingBalloons[i];
        glPushMatrix();
        glTranslatef(b.x, b.y, b.z);
        glDisable(GL_LIGHTING);
        glColor3f(0.85f, 0.85f, 0.85f);
        glBegin(GL_LINES); glVertex3f(0, 0, 0); glVertex3f(0, -1.5f, 0); glEnd();
        glEnable(GL_LIGHTING);
        glColor3f(b.r, b.g, b.b);
        glTranslatef(0, 0.55f, 0);
        glScalef(0.85f, 1.1f, 0.85f);
        glutSolidSphere(0.55, 14, 14);
        glPopMatrix();
    }
}

// ------------------------- circus tent -------------------------------
void drawCircusTent() {
    glPushMatrix();
    glTranslatef(20.0f, 0, -1.0f);
    const int n = 18; const float R = 6.8f, wallH = 4.0f, roofH = 5.5f;
    const float red[3] = {0.85f, 0.12f, 0.15f}, cream[3] = {0.98f, 0.96f, 0.92f};
    float level = 0.25f + 0.85f * dusk;

    stripedBand(R, 0, wallH, n, red, cream, 4);            // wall with an opening (segment 4 faces the camera)
    stripedCone(R + 0.5f, wallH, roofH, n, red, cream);

    // glowing entrance
    float a1 = 4 * 2 * PI_F / n, a2 = 5 * 2 * PI_F / n;
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glColor3f(0.30f, 0.10f, 0.08f);
    setEmission(0.9f * level, 0.5f * level, 0.2f * level);
    glVertex3f((R - 0.1f) * cosf(a1), 0, (R - 0.1f) * sinf(a1)); glVertex3f((R - 0.1f) * cosf(a2), 0, (R - 0.1f) * sinf(a2));
    glVertex3f((R - 0.1f) * cosf(a2), wallH - 0.4f, (R - 0.1f) * sinf(a2)); glVertex3f((R - 0.1f) * cosf(a1), wallH - 0.4f, (R - 0.1f) * sinf(a1));
    glEnd();
    clearEmission();

    // fairy lights: wall top, droop between, and strings up the roof seams
    for (int i = 0; i < n; i++) {
        float a = i * 2 * PI_F / n, am = (i + 0.5f) * 2 * PI_F / n;
        float tw = 0.5f + 0.5f * sinf(globalTime * 5.0f + i * 1.7f);
        float lv = level * (0.65f + 0.35f * tw);
        bulb((R + 0.5f) * cosf(a), wallH - 0.05f, (R + 0.5f) * sinf(a), 0.17f, BULB_COL[i % 4], lv);
        bulb((R + 0.62f) * cosf(am), wallH - 0.38f, (R + 0.62f) * sinf(am), 0.13f, BULB_COL[(i + 2) % 4], lv);
        if (i % 2 == 0)
            for (int k = 1; k <= 3; k++) {
                float f = k * 0.25f, rr = (R + 0.5f) * (1.0f - f);
                bulb(rr * cosf(a), wallH + roofH * f + 0.1f, rr * sinf(a), 0.13f, BULB_COL[(i + k) % 4], lv);
            }
    }
    // pole, flag
    glColor3f(0.95f, 0.78f, 0.20f);
    glPushMatrix(); glTranslatef(0, wallH + roofH, 0); cylY(0.1f, 2.2f, 8); glPopMatrix();
    glColor3f(0.9f, 0.1f, 0.2f);
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 0, 1);
    glVertex3f(0, wallH + roofH + 2.2f, 0); glVertex3f(0, wallH + roofH + 1.2f, 0); glVertex3f(1.5f + 0.2f * sinf(globalTime * 4), wallH + roofH + 1.7f, 0);
    glEnd();
    glPopMatrix();
}

// ------------------------- Ferris wheel ------------------------------
void drawFerrisWheel() {
    glPushMatrix();
    glTranslatef(-24.0f, 0.0f, -4.0f);
    glRotatef(15.0f, 0, 1, 0);
    const float hubY = 13.5f, radius = 9.5f;
    float level = 0.2f + 0.9f * dusk;

    glColor3f(0.12f, 0.55f, 0.62f);                        // A-frame legs
    for (int zs = -1; zs <= 1; zs += 2)
        for (int xs = -1; xs <= 1; xs += 2) {
            glPushMatrix();
            glTranslatef(xs * 4.8f, 0.0f, zs * 2.2f);
            glRotatef(xs * 20.0f, 0, 0, 1);
            glRotatef(-90, 1, 0, 0);
            glutSolidCylinder(0.28, 14.5, 10, 1);
            glPopMatrix();
        }
    glColor3f(0.95f, 0.82f, 0.2f);                         // axle
    glPushMatrix(); glTranslatef(0, hubY, -2.5f); glutSolidCylinder(0.45, 5.0, 16, 1); glPopMatrix();

    glPushMatrix();
    glTranslatef(0, hubY, 0);
    glRotatef(ferrisAngle, 0, 0, 1);
    glColor3f(0.95f, 0.80f, 0.22f);
    for (int zr = -1; zr <= 1; zr += 2) { glPushMatrix(); glTranslatef(0, 0, zr * 1.4f); glutSolidTorus(0.22, radius, 12, 40); glPopMatrix(); }

    const int cabins = 10;
    for (int i = 0; i < cabins; i++) {
        float theta = i * (360.0f / cabins), rad = theta * PI_F / 180.0f;
        glColor3f(0.95f, 0.85f, 0.35f);                    // spokes
        for (int zr = -1; zr <= 1; zr += 2) {
            glPushMatrix(); glRotatef(theta, 0, 0, 1); glTranslatef(0, 0, zr * 1.4f); glRotatef(90, 0, 1, 0); glutSolidCylinder(0.08, radius, 8, 1); glPopMatrix();
        }
        glPushMatrix();                                    // cabin (stays upright)
        glTranslatef(radius * cosf(rad), radius * sinf(rad), 0);
        glRotatef(-ferrisAngle, 0, 0, 1);
        glColor3f((i % 2 == 0) ? 0.15f : 0.95f, (i % 2 == 0) ? 0.65f : 0.78f, (i % 2 == 0) ? 0.72f : 0.22f);
        glTranslatef(0, -1.1f, 0);
        glutSolidCube(1.6f);
        glColor3f(0.98f, 0.95f, 0.88f);
        glTranslatef(0, 1.0f, 0);
        glRotatef(-90, 1, 0, 0);
        glutSolidCone(1.2, 0.7, 12, 1);
        glPopMatrix();
    }
    for (int k = 0; k < 20; k++) {                         // lights on both rims
        float a = k * 2 * PI_F / 20;
        float tw = 0.7f + 0.3f * sinf(globalTime * 6.0f + k);
        for (int zr = -1; zr <= 1; zr += 2)
            bulb(radius * cosf(a), radius * sinf(a), zr * 1.4f + zr * 0.2f, 0.2f, BULB_COL[k % 4], level * tw);
    }
    bulb(0, 0, 3.0f, 0.4f, BULB_COL[1], level);
    glPopMatrix();
    glPopMatrix();
}

// ------------------------- merry-go-round ------------------------------
void drawHorse(int idx) {
    static const float body[4][3] = {{0.97f, 0.95f, 0.9f}, {0.55f, 0.33f, 0.18f}, {0.95f, 0.85f, 0.6f}, {0.25f, 0.2f, 0.2f}};
    const float* c = body[idx % 4];
    glPushMatrix();
    glScalef(0.9f, 0.9f, 0.9f);
    col3(c);
    glPushMatrix(); glScalef(0.38f, 0.42f, 0.95f); glutSolidSphere(1.0, 12, 10); glPopMatrix();        // body
    limb(0, 0.25f, -0.65f, 0, 0.78f, -1.0f, 0.15f);                                                     // neck
    glPushMatrix(); glTranslatef(0, 0.85f, -1.15f); glScalef(0.17f, 0.2f, 0.34f); glutSolidSphere(1.0, 10, 8); glPopMatrix(); // head
    glColor3f(0.9f, 0.7f, 0.2f);
    limb(0, 0.95f, -0.9f, 0, 0.35f, -0.6f, 0.07);                                                      // golden mane
    limb(0, 0.2f, 0.85f, 0, -0.35f, 1.15f, 0.07f);                                                      // tail
    col3(c);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2)
        limb(sx * 0.2f, -0.2f, sz * 0.55f, sx * 0.2f, -0.9f, sz * 0.6f, 0.06f);
    glColor3f(0.8f, 0.1f, 0.15f);
    glPushMatrix(); glTranslatef(0, 0.42f, 0); box(0.55f, 0.1f, 0.6f); glPopMatrix();                   // saddle
    glPopMatrix();
}
void drawChariot() {
    glColor3f(0.9f, 0.2f, 0.45f);
    glPushMatrix(); glTranslatef(0, -0.2f, 0); box(1.1f, 0.7f, 1.8f); glPopMatrix();
    glColor3f(1.0f, 0.85f, 0.2f);
    glPushMatrix(); glTranslatef(0, 0.1f, -0.95f); glutSolidSphere(0.5, 10, 10); glPopMatrix();
    glColor3f(0.7f, 0.1f, 0.3f);
    glPushMatrix(); glTranslatef(0, 0.35f, 0.5f); box(1.0f, 0.7f, 0.15f); glPopMatrix();
}

void drawCarousel() {
    const float gold[3] = {0.95f, 0.78f, 0.25f}, red[3] = {0.85f, 0.12f, 0.2f}, cream[3] = {0.98f, 0.95f, 0.88f};
    float level = 0.25f + 0.85f * dusk;
    glPushMatrix();
    glTranslatef(-3.0f, 0, 2.0f);
    glColor3f(0.8f, 0.2f, 0.3f); cylY(5.2f, 0.35f, 36);             // platform
    glPushMatrix(); glTranslatef(0, 0.35f, 0); glColor3f(0.96f, 0.9f, 0.8f); cylY(4.9f, 0.08f, 36); glPopMatrix();
    col3(gold); glPushMatrix(); glTranslatef(0, 0.35f, 0); cylY(0.8f, 5.0f, 20); glPopMatrix();   // center column

    glRotatef(carouselAngle, 0, 1, 0);                              // rotating part
    glColor3f(0.55f, 0.8f, 0.95f);
    for (int m = 0; m < 6; m++) { glPushMatrix(); glRotatef(m * 60.0f, 0, 1, 0); glTranslatef(0.82f, 2.8f, 0); box(0.05f, 3.0f, 0.9f); glPopMatrix(); } // mirror panels

    for (int i = 0; i < 8; i++) {
        float bob = 1.55f + 0.55f * sinf(carouselAngle * 0.0523f * 1.0f + (i % 2) * PI_F + i * 0.3f);
        glPushMatrix();
        glRotatef(i * 45.0f, 0, 1, 0);
        glTranslatef(3.4f, 0, 0);
        col3(gold); glPushMatrix(); glTranslatef(0, 0.35f, 0); cylY(0.07f, 4.7f, 8); glPopMatrix();     // pole
        glTranslatef(0, bob + 0.9f, 0);
        if (i % 4 == 3) drawChariot(); else drawHorse(i);
        glPopMatrix();
    }
    stripedBand(5.6f, 4.9f, 0.7f, 16, red, cream);                  // roof rim + cone
    stripedCone(5.8f, 5.6f, 2.4f, 16, red, cream);
    col3(gold); glPushMatrix(); glTranslatef(0, 8.0f, 0); glutSolidSphere(0.3, 10, 10); glPopMatrix();

    for (int i = 0; i < 24; i++) {                                  // fairy lights
        float a = i * 2 * PI_F / 24, tw = 0.65f + 0.35f * sinf(globalTime * 6.0f + i * 0.9f);
        bulb(5.8f * cosf(a), 4.85f, 5.8f * sinf(a), 0.15f, BULB_COL[i % 4], level * tw);
    }
    for (int i = 0; i < 16; i++) {
        float a = i * 2 * PI_F / 16, tw = 0.65f + 0.35f * sinf(globalTime * 5.0f + i * 1.3f);
        bulb(5.1f * cosf(a), 0.5f, 5.1f * sinf(a), 0.14f, BULB_COL[(i + 1) % 4], level * tw);
    }
    glPopMatrix();
}

// ------------------------- roller coaster ------------------------------
void drawRollerCoaster() {
    float level = 0.15f + 0.95f * dusk;
    size_t M = railPts.size();
    glColor3f(0.15f, 0.72f, 0.78f);
    for (size_t i = 0; i < M; i++) {
        const TP& a = railPts[i]; const TP& b = railPts[(i + 1) % M];
        for (int sd = -1; sd <= 1; sd += 2)
            limb(a.x + sd * 0.45f * a.nx, a.y, a.z + sd * 0.45f * a.nz, b.x + sd * 0.45f * b.nx, b.y, b.z + sd * 0.45f * b.nz, 0.1f, 6);
    }
    glColor3f(0.35f, 0.25f, 0.2f);                                  // cross ties
    for (size_t i = 0; i < M; i += 2) { const TP& a = railPts[i]; limb(a.x - 0.45f * a.nx, a.y - 0.05f, a.z - 0.45f * a.nz, a.x + 0.45f * a.nx, a.y - 0.05f, a.z + 0.45f * a.nz, 0.05f, 5); }
    glColor3f(0.48f, 0.32f, 0.65f);                                 // supports
    for (size_t i = 0; i < M; i += 5) {
        const TP& a = railPts[i];
        if (fabsf(a.t - LOOP_C) < LOOP_W + 0.06f || a.y > 12.0f) continue;
        glPushMatrix(); glTranslatef(a.x, 0, a.z); cylY(0.17f, a.y - 0.1f, 8); glPopMatrix();
    }
    for (size_t i = 0; i < M; i += 3) {                             // track lights
        const TP& a = railPts[i];
        float tw = 0.7f + 0.3f * sinf(globalTime * 6.0f + i * 0.5f);
        bulb(a.x, a.y + 0.18f, a.z, 0.12f, BULB_COL[(i / 3) % 4], level * tw, dusk > 0.3f);
    }
    // train
    for (int c = 0; c < 5; c++) {
        P3 p1, p2;
        trackAt(coasterS - c * 2.6f, p1);
        trackAt(coasterS - c * 2.6f + 0.9f, p2);
        float fx = p2.x - p1.x, fy = p2.y - p1.y, fz = p2.z - p1.z, fl = sqrtf(fx * fx + fy * fy + fz * fz);
        if (fl < 1e-5f) continue;
        fx /= fl; fy /= fl; fz /= fl;
        float rx = RZ * cosf(p1.t), rz = RX * sinf(p1.t), rl = sqrtf(rx * rx + rz * rz);
        rx /= rl; rz /= rl; float ry = 0;
        float dd = rx * fx + ry * fy + rz * fz;
        rx -= fx * dd; ry -= fy * dd; rz -= fz * dd;
        rl = sqrtf(rx * rx + ry * ry + rz * rz); rx /= rl; ry /= rl; rz /= rl;
        float ux = fy * rz - fz * ry, uy = fz * rx - fx * rz, uz = fx * ry - fy * rx;   // up = forward x right
        GLfloat m[16] = {rx, ry, rz, 0, ux, uy, uz, 0, fx, fy, fz, 0, p1.x, p1.y, p1.z, 1};
        glPushMatrix();
        glMultMatrixf(m);
        glTranslatef(0, 0.45f, 0);
        glColor3f(c == 0 ? 0.95f : 0.9f, c % 2 == 0 ? 0.2f : 0.75f, 0.2f);
        box(1.2f, 0.6f, 2.0f);
        glColor3f(1.0f, 0.88f, 0.3f);
        glPushMatrix(); glTranslatef(0, 0.5f, 0.5f); box(1.2f, 0.4f, 0.12f); glPopMatrix();
        glColor3f(1.0f, 0.75f, 0.58f);
        ballAt(-0.28f, 0.65f, -0.1f, 0.2f, 8); ballAt(0.28f, 0.65f, -0.1f, 0.2f, 8);       // riders
        if (c == 0) { const float w[3] = {1, 1, 0.8f}; glPushMatrix(); glTranslatef(0, 0.1f, 1.05f); glowHere(0.35f, w, 0.8f * dusk); glPopMatrix(); }
        glPopMatrix();
    }
}

// ------------------------- Clown (juggling) ----------------------------
void drawClown() {
    glPushMatrix();
    glTranslatef(4.0f, 0, 15.5f);
    glScalef(1.8f, 1.8f, 1.8f);
    float dance = sinf(limbSwing * 3.0f) * 10.0f;
    for (int side = -1; side <= 1; side += 2) {                       // legs + big shoes
        glPushMatrix();
        glTranslatef(side * 0.22f, 1.1f, 0);
        glRotatef(side * dance, 1, 0, 0);
        glColor3f(0.98f, 0.82f, 0.10f);
        glPushMatrix(); glTranslatef(0, -0.55f, 0); box(0.32f, 1.1f, 0.32f); glPopMatrix();
        glColor3f(side < 0 ? 0.15f : 0.9f, side < 0 ? 0.3f : 0.1f, side < 0 ? 0.95f : 0.15f);
        glPushMatrix(); glTranslatef(0, -1.0f, 0.2f); box(0.4f, 0.24f, 0.75f); glPopMatrix();
        glPopMatrix();
    }
    glColor3f(0.9f, 0.1f, 0.15f);                                      // torso
    glPushMatrix(); glTranslatef(0, 1.75f, 0); box(0.82f, 1.15f, 0.5f); glPopMatrix();
    glColor3f(0.15f, 0.35f, 0.95f);
    for (int k = 0; k < 3; k++) ballAt(0, 2.1f - k * 0.35f, 0.27f, 0.09f, 8);
    glColor3f(1.0f, 1.0f, 1.0f);                                        // ruffle collar
    glPushMatrix(); glTranslatef(0, 2.38f, 0); glRotatef(90, 1, 0, 0); glutSolidTorus(0.1, 0.3, 8, 16); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 2.72f, 0);                         // head
    glColor3f(1.0f, 0.93f, 0.88f); glutSolidSphere(0.34, 18, 18);
    glColor3f(0.95f, 0.05f, 0.05f); ballAt(0, -0.02f, 0.34f, 0.1f, 10);  // red nose
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.12f, 0.1f, 0.3f, 0.04f, 6); ballAt(0.12f, 0.1f, 0.3f, 0.04f, 6);
    glColor3f(0.9f, 0.0f, 0.1f); glPushMatrix(); glTranslatef(0, -0.15f, 0.31f); box(0.28f, 0.05f, 0.05f); glPopMatrix();
    static const float rb[6][3] = {{1, 0.3f, 0.1f}, {1, 0.85f, 0.1f}, {0.2f, 0.8f, 0.3f}, {0.2f, 0.5f, 1}, {0.7f, 0.3f, 0.9f}, {1, 0.4f, 0.7f}};
    float hp[6][3] = {{-0.38f, 0.05f, 0}, {0.38f, 0.05f, 0}, {-0.3f, 0.27f, 0}, {0.3f, 0.27f, 0}, {-0.12f, 0.36f, 0}, {0.12f, 0.36f, 0}};
    for (int k = 0; k < 6; k++) { col3(rb[k]); ballAt(hp[k][0], hp[k][1], hp[k][2], 0.18f, 10); }
    glColor3f(0.15f, 0.35f, 0.95f);
    glPushMatrix(); glTranslatef(0, 0.42f, 0); glRotatef(-90, 1, 0, 0); glutSolidCone(0.2, 0.35, 10, 1); glPopMatrix();
    glPopMatrix();

    const float handX = 0.95f, handY = 2.62f, handZ = 0.35f;
    for (int side = -1; side <= 1; side += 2) {                        // arms
        glColor3f(0.98f, 0.82f, 0.10f);
        limb(side * 0.45f, 2.25f, 0, side * handX, handY, handZ, 0.09f);
        glColor3f(1.0f, 0.78f, 0.62f);
        ballAt(side * handX, handY, handZ, 0.13f, 10);
    }
    static const float bc[3][3] = {{1.0f, 0.1f, 0.1f}, {0.1f, 0.95f, 0.2f}, {0.2f, 0.35f, 1.0f}};
    for (int b = 0; b < 3; b++) {                                      // 3-ball cascade
        float motion = globalTime * 1.5f + b / 3.0f;
        int tn = (int)floorf(motion);
        float t = motion - floorf(motion);
        float sx = (tn % 2 == 0) ? -handX : handX, ex = -sx;
        float bx = sx + (ex - sx) * t;
        float by = handY + 0.15f + 1.7f * 4.0f * t * (1.0f - t);
        col3(bc[b]);
        ballAt(bx, by, handZ, 0.16f, 14);
    }
    glPopMatrix();
}

// ------------------------- Joker with WELCOME sign ---------------------
void drawJoker() {
    glPushMatrix();
    glTranslatef(-12.0f, 0, 15.0f);
    glScalef(1.9f, 1.9f, 1.9f);
    glRotatef(sinf(globalTime * 2.0f) * 2.5f, 0, 0, 1);
    float march = sinf(limbSwing * 2.0f) * 8.0f;
    for (int side = -1; side <= 1; side += 2) {                        // legs
        glPushMatrix();
        glTranslatef(side * 0.22f, 1.1f, 0);
        glRotatef(side * march, 1, 0, 0);
        if (side < 0) glColor3f(0.10f, 0.70f, 0.20f); else glColor3f(0.5f, 0.1f, 0.7f);
        glPushMatrix(); glTranslatef(0, -0.55f, 0); box(0.26f, 1.1f, 0.26f); glPopMatrix();
        glColor3f(0.95f, 0.78f, 0.15f);                                // curly shoes
        glPushMatrix(); glTranslatef(0, -1.0f, 0.1f); glutSolidCone(0.17, 0.65, 8, 1); glPopMatrix();
        glPopMatrix();
    }
    glColor3f(0.5f, 0.1f, 0.7f); glPushMatrix(); glTranslatef(-0.2f, 1.75f, 0); box(0.4f, 1.15f, 0.5f); glPopMatrix();   // two-tone torso
    glColor3f(1.0f, 0.82f, 0.1f); glPushMatrix(); glTranslatef(0.2f, 1.75f, 0); box(0.4f, 1.15f, 0.5f); glPopMatrix();
    glColor3f(0.9f, 0.1f, 0.2f);
    for (int k = 0; k < 3; k++) { glPushMatrix(); glTranslatef(0, 2.05f - k * 0.35f, 0.27f); glRotatef(45, 0, 0, 1); box(0.14f, 0.14f, 0.05f); glPopMatrix(); }
    glColor3f(1, 1, 1);
    glPushMatrix(); glTranslatef(0, 2.38f, 0); glRotatef(90, 1, 0, 0); glutSolidTorus(0.1, 0.3, 8, 16); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 2.72f, 0);                         // head
    glColor3f(1.0f, 0.88f, 0.78f); glutSolidSphere(0.34, 18, 18);
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.12f, 0.1f, 0.3f, 0.04f, 6); ballAt(0.12f, 0.1f, 0.3f, 0.04f, 6);
    glColor3f(1.0f, 0.4f, 0.4f); ballAt(-0.2f, -0.05f, 0.27f, 0.06f, 6); ballAt(0.2f, -0.05f, 0.27f, 0.06f, 6);
    glColor3f(0.9f, 0.0f, 0.1f);
    glPushMatrix(); glTranslatef(0, -0.15f, 0.31f); box(0.3f, 0.05f, 0.05f); glPopMatrix();
    for (int side = -1; side <= 1; side += 2) {                        // jester hat horns
        glPushMatrix();
        glTranslatef(side * 0.18f, 0.28f, 0);
        glRotatef(-side * 55.0f, 0, 0, 1);
        glRotatef(-90, 1, 0, 0);
        if (side < 0) glColor3f(0.5f, 0.1f, 0.7f); else glColor3f(1.0f, 0.82f, 0.1f);
        glutSolidCone(0.22, 0.8, 10, 1);
        glColor3f(0.95f, 0.78f, 0.15f);
        glTranslatef(0, 0, 0.8f); glutSolidSphere(0.1, 8, 8);
        glPopMatrix();
    }
    glPopMatrix();

    // WELCOME board held in front of the body
    glPushMatrix();
    glTranslatef(0, 1.05f, 0.62f);
    glColor3f(0.95f, 0.62f, 0.10f); box(3.0f, 1.15f, 0.1f);
    glColor3f(0.98f, 0.95f, 0.85f); glPushMatrix(); glTranslatef(0, 0, 0.03f); box(2.8f, 0.95f, 0.1f); glPopMatrix();
    glTranslatef(0, 0, 0.1f);
    signText("WELCOME", 2.45f, 0.8f, 0.08f, 0.12f);
    float level = 0.3f + 0.9f * dusk;
    for (int i = 0; i < 8; i++) {                                      // little lights around the sign
        float x = -1.4f + i * 0.4f;
        bulb(x, 0.52f, 0.0f, 0.06f, BULB_COL[i % 4], level, false);
        bulb(x, -0.52f, 0.0f, 0.06f, BULB_COL[(i + 2) % 4], level, false);
    }
    glPopMatrix();
    for (int side = -1; side <= 1; side += 2) {                        // arms holding the board
        if (side < 0) glColor3f(0.5f, 0.1f, 0.7f); else glColor3f(1.0f, 0.82f, 0.1f);
        limb(side * 0.45f, 2.25f, 0, side * 1.4f, 1.1f, 0.65f, 0.09f);
        glColor3f(1.0f, 0.88f, 0.78f);
        ballAt(side * 1.4f, 1.1f, 0.65f, 0.13f, 10);
    }
    glPopMatrix();
}

// ------------------------- kids ----------------------------------------
struct KidDef { float x, z, yaw; float shirt[3], pants[3], hair[3]; int pose; int girl; };
static const KidDef kids[10] = {
    {-21, 11,  25, {0.2f, 0.4f, 0.95f}, {0.2f, 0.2f, 0.5f}, {0.3f, 0.2f, 0.1f}, 1, 0},
    {-17.5f, 13.5f, -30, {1.0f, 0.85f, 0.2f}, {0.9f, 0.3f, 0.5f}, {0.1f, 0.1f, 0.1f}, 3, 1},
    { -7, 10.5f, 15, {0.9f, 0.2f, 0.2f}, {0.2f, 0.3f, 0.6f}, {0.8f, 0.55f, 0.2f}, 2, 0},
    { -1, 13, -15, {0.2f, 0.75f, 0.3f}, {0.3f, 0.3f, 0.35f}, {0.2f, 0.1f, 0.05f}, 0, 0},
    {15.5f, 15.8f, 190, {0.95f, 0.5f, 0.1f}, {0.2f, 0.25f, 0.5f}, {0.4f, 0.25f, 0.1f}, 1, 0},
    {18.5f, 16.0f, 170, {0.95f, 0.4f, 0.7f}, {0.9f, 0.6f, 0.8f}, {0.6f, 0.35f, 0.1f}, 0, 1},
    {21.5f, 15.6f, 185, {0.4f, 0.8f, 0.95f}, {0.5f, 0.2f, 0.2f}, {0.1f, 0.1f, 0.1f}, 3, 0},
    {19.2f, 7.0f,  10, {0.7f, 0.3f, 0.9f}, {0.2f, 0.2f, 0.3f}, {0.9f, 0.7f, 0.2f}, 2, 1},
    {22.8f, 7.8f, -20, {0.95f, 0.85f, 0.2f}, {0.2f, 0.4f, 0.7f}, {0.15f, 0.1f, 0.05f}, 1, 0},
    {-11, 5, 60, {0.95f, 0.3f, 0.3f}, {0.25f, 0.25f, 0.6f}, {0.3f, 0.2f, 0.1f}, 1, 1}};

void drawKid(const KidDef& k) {
    glPushMatrix();
    glTranslatef(k.x, 0, k.z);
    glRotatef(k.yaw, 0, 1, 0);
    glScalef(1.5f, 1.5f, 1.5f);
    col3(k.pants);
    limb(-0.12f, 0.85f, 0, -0.12f, 0.08f, 0, 0.09f); limb(0.12f, 0.85f, 0, 0.12f, 0.08f, 0, 0.09f);
    glColor3f(0.15f, 0.15f, 0.15f);
    glPushMatrix(); glTranslatef(-0.12f, 0.04f, 0.06f); box(0.16f, 0.08f, 0.28f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.12f, 0.04f, 0.06f); box(0.16f, 0.08f, 0.28f); glPopMatrix();
    col3(k.shirt);
    glPushMatrix(); glTranslatef(0, 1.2f, 0); box(0.5f, 0.65f, 0.3f); glPopMatrix();
    if (k.girl) { col3(k.pants); glPushMatrix(); glTranslatef(0, 0.78f, 0); glRotatef(-90, 1, 0, 0); glutSolidCone(0.38, 0.45, 10, 1); glPopMatrix(); }
    glColor3f(1.0f, 0.78f, 0.62f);
    glPushMatrix(); glTranslatef(0, 1.8f, 0); glutSolidSphere(0.26, 14, 14);
    col3(k.hair); glPushMatrix(); glTranslatef(0, 0.07f, -0.05f); glScalef(1.0f, 0.85f, 1.0f); glutSolidSphere(0.28, 12, 12); glPopMatrix();
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.09f, 0.0f, 0.23f, 0.035f, 6); ballAt(0.09f, 0.0f, 0.23f, 0.035f, 6);
    glPopMatrix();
    // arms by pose
    float lx, ly, lz, rx, ry, rz;
    switch (k.pose) {
        case 1:  lx = -0.34f; ly = 0.9f; lz = 0.05f;  rx = 0.5f;  ry = 2.0f; rz = 0.05f; break;   // waving
        case 2:  lx = -0.8f;  ly = 1.4f; lz = 0;      rx = 0.8f;  ry = 1.4f; rz = 0;     break;   // arms out
        case 3:  lx = -0.45f; ly = 2.0f; lz = 0.05f;  rx = 0.45f; ry = 2.0f; rz = 0.05f; break;   // cheering
        default: lx = -0.34f; ly = 0.9f; lz = 0.05f;  rx = 0.34f; ry = 0.9f; rz = 0.05f; break;
    }
    col3(k.shirt);
    limb(-0.3f, 1.45f, 0, lx, ly, lz, 0.07f); limb(0.3f, 1.45f, 0, rx, ry, rz, 0.07f);
    glColor3f(1.0f, 0.78f, 0.62f);
    ballAt(lx, ly, lz, 0.08f, 8); ballAt(rx, ry, rz, 0.08f, 8);
    glPopMatrix();
}

// running girl with a dripping ice cream
void drawRunningGirl() {
    float a = globalTime * 0.45f;
    float px = 8.0f + 3.5f * cosf(a), pz = 8.0f + 3.5f * sinf(a);
    float yaw = atan2f(-sinf(a), cosf(a)) * DEG;
    float phase = globalTime * 9.0f;
    const float SC = 1.5f;
    const float hx = 0.42f, hy = 1.85f, hz = 0.3f;     // hand holding the cone (local)

    glPushMatrix();
    glTranslatef(px, 0, pz);
    glRotatef(yaw, 0, 1, 0);
    glScalef(SC, SC, SC);

    // falling drips (stay in the world as she runs away from them)
    glColor3f(1.0f, 0.62f, 0.78f);
    for (int k = 0; k < 3; k++) {
        float u = fmodf(globalTime / 1.1f + k / 3.0f, 1.0f);
        float startY = hy + 0.2f;
        float y = startY - startY * u * u;
        float z = hz - (3.5f * 0.45f / SC) * u * 1.1f;
        glPushMatrix(); glTranslatef(hx + 0.08f, y, z); glScalef(0.7f, 1.3f, 0.7f); glutSolidSphere(0.05, 8, 8); glPopMatrix();
        if (u > 0.93f) { glPushMatrix(); glTranslatef(hx + 0.08f, 0.02f, z); glScalef(1.6f, 0.2f, 1.6f); glutSolidSphere(0.06, 8, 8); glPopMatrix(); }
    }

    glPushMatrix();
    glTranslatef(0, fabsf(sinf(phase)) * 0.1f, 0);
    glRotatef(7.0f, 1, 0, 0);
    // legs
    glColor3f(1.0f, 0.78f, 0.62f);
    for (int s = -1; s <= 1; s += 2) {
        float p = phase + (s > 0 ? PI_F : 0.0f);
        limb(s * 0.12f, 0.8f, 0, s * 0.12f, 0.1f + 0.25f * fmaxf(0.0f, cosf(p)), 0.42f * sinf(p), 0.07f);
        glColor3f(0.9f, 0.2f, 0.4f);
        glPushMatrix(); glTranslatef(s * 0.12f, 0.1f + 0.25f * fmaxf(0.0f, cosf(p)), 0.42f * sinf(p) + 0.06f); box(0.14f, 0.08f, 0.26f); glPopMatrix();
        glColor3f(1.0f, 0.78f, 0.62f);
    }
    glColor3f(0.98f, 0.45f, 0.7f);                                   // dress
    glPushMatrix(); glTranslatef(0, 0.8f, 0); glRotatef(-90, 1, 0, 0); glutSolidCone(0.38, 0.75, 12, 1); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 1.5f, 0); box(0.4f, 0.28f, 0.25f); glPopMatrix();
    // head, pigtails
    glColor3f(1.0f, 0.78f, 0.62f);
    glPushMatrix(); glTranslatef(0, 1.82f, 0); glutSolidSphere(0.26, 14, 14);
    glColor3f(0.45f, 0.25f, 0.1f);
    glPushMatrix(); glTranslatef(0, 0.07f, -0.05f); glScalef(1.0f, 0.85f, 1.0f); glutSolidSphere(0.28, 12, 12); glPopMatrix();
    ballAt(-0.3f, -0.05f, -0.02f, 0.11f, 8); ballAt(0.3f, -0.05f, -0.02f, 0.11f, 8);
    glColor3f(0.95f, 0.2f, 0.4f); ballAt(-0.27f, 0.08f, 0, 0.05f, 6); ballAt(0.27f, 0.08f, 0, 0.05f, 6);
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.09f, 0.0f, 0.23f, 0.035f, 6); ballAt(0.09f, 0.0f, 0.23f, 0.035f, 6);
    glColor3f(0.9f, 0.1f, 0.2f); glPushMatrix(); glTranslatef(0, -0.1f, 0.24f); box(0.1f, 0.03f, 0.03f); glPopMatrix();
    glPopMatrix();
    // arms
    glColor3f(1.0f, 0.78f, 0.62f);
    limb(-0.28f, 1.5f, 0, -0.3f, 1.05f, 0.38f * sinf(phase + PI_F), 0.06f);
    limb(0.28f, 1.5f, 0, hx, hy, hz, 0.06f);
    ballAt(hx, hy, hz, 0.07f, 8);
    // ice cream cone
    glPushMatrix();
    glTranslatef(hx, hy + 0.28f, hz);
    glColor3f(0.85f, 0.58f, 0.25f);
    glPushMatrix(); glRotatef(90, 1, 0, 0); glutSolidCone(0.11, 0.42, 10, 1); glPopMatrix();
    glColor3f(1.0f, 0.55f, 0.72f); ballAt(0, 0.08f, 0, 0.13f, 12);
    glColor3f(0.5f, 0.3f, 0.2f);   ballAt(0, 0.22f, 0, 0.1f, 10);
    glColor3f(0.9f, 0.05f, 0.1f);  ballAt(0, 0.33f, 0, 0.035f, 6);
    glColor3f(1.0f, 0.62f, 0.78f);                                    // melting streak
    float dl = 0.14f + 0.10f * (0.5f + 0.5f * sinf(globalTime * 2.5f));
    limb(0.1f, 0.05f, 0.04f, 0.11f, 0.05f - dl, 0.04f, 0.028f, 6);
    ballAt(0.11f, 0.05f - dl, 0.04f, 0.04f, 6);
    glPopMatrix();
    glPopMatrix();
    glPopMatrix();
}

// ------------------------- ice cream truck -----------------------------
void drawIceCreamTruck() {
    bool signOn = fmodf(globalTime, 1.4f) < 0.8f;
    glPushMatrix();
    glTranslatef(18.5f, 0, 12.5f);
    glRotatef(-12.0f, 0, 1, 0);
    glScalef(1.25f, 1.25f, 1.25f);
    const float cream[3] = {0.98f, 0.93f, 0.95f};
    col3(cream);   glPushMatrix(); glTranslatef(0.6f, 2.5f, 0); box(4.8f, 3.0f, 3.0f); glPopMatrix();   // box body
    glColor3f(0.95f, 0.5f, 0.7f); glPushMatrix(); glTranslatef(0.6f, 1.2f, 0); box(4.84f, 0.5f, 3.04f); glPopMatrix();
    col3(cream);   glPushMatrix(); glTranslatef(-2.6f, 1.7f, 0); box(1.8f, 2.0f, 2.8f); glPopMatrix();     // cab
    glColor3f(0.3f, 0.55f, 0.8f);
    glPushMatrix(); glTranslatef(-3.51f, 2.1f, 0); box(0.05f, 0.9f, 2.2f); glPopMatrix();
    for (int s = -1; s <= 1; s += 2) { glPushMatrix(); glTranslatef(-2.6f, 2.2f, s * 1.41f); box(1.0f, 0.8f, 0.04f); glPopMatrix(); }
    glColor3f(0.3f, 0.3f, 0.33f); glPushMatrix(); glTranslatef(-3.55f, 0.75f, 0); box(0.2f, 0.3f, 2.9f); glPopMatrix();
    { const float hl[3] = {1.0f, 0.95f, 0.7f}; for (int s = -1; s <= 1; s += 2) bulb(-3.55f, 1.4f, s * 0.9f, 0.18f, hl, 0.3f + 0.6f * dusk, dusk > 0.3f); }
    for (int xi = 0; xi < 2; xi++) for (int s = -1; s <= 1; s += 2) {                                       // wheels
        float x = xi ? 1.8f : -2.6f;
        glColor3f(0.08f, 0.08f, 0.1f);
        glPushMatrix(); glTranslatef(x, 0.65f, s * 1.5f - 0.2f); glutSolidCylinder(0.65, 0.4, 16, 1); glPopMatrix();
        glColor3f(0.7f, 0.7f, 0.72f); ballAt(x, 0.65f, s * 1.5f + s * 0.2f, 0.25f, 8);
    }
    // serving window + awning
    glColor3f(0.18f, 0.12f, 0.15f); glPushMatrix(); glTranslatef(0.6f, 2.5f, 1.51f); box(2.6f, 1.2f, 0.04f); glPopMatrix();
    { const float warm[3] = {1.0f, 0.8f, 0.4f}; glColor3f(0.35f, 0.25f, 0.2f); setEmission(0.8f * dusk * warm[0], 0.8f * dusk * warm[1], 0.8f * dusk * warm[2]); glPushMatrix(); glTranslatef(0.6f, 2.5f, 1.53f); box(2.4f, 1.0f, 0.02f); glPopMatrix(); clearEmission(); }
    glColor3f(0.95f, 0.85f, 0.75f); glPushMatrix(); glTranslatef(0.6f, 1.88f, 1.75f); box(2.8f, 0.12f, 0.5f); glPopMatrix();
    glBegin(GL_QUADS);
    glNormal3f(0, 0.6f, 0.8f);
    for (int i = 0; i < 6; i++) {
        float x0 = -0.7f + i * 0.43f;
        if (i % 2) glColor3f(1, 1, 1); else glColor3f(0.95f, 0.35f, 0.6f);
        glVertex3f(x0, 3.3f, 1.5f); glVertex3f(x0 + 0.43f, 3.3f, 1.5f); glVertex3f(x0 + 0.43f, 2.95f, 2.15f); glVertex3f(x0, 2.95f, 2.15f);
    }
    glEnd();
    // blinking roof sign
    glColor3f(0.3f, 0.3f, 0.35f);
    for (int s = -1; s <= 1; s += 2) { glPushMatrix(); glTranslatef(0.6f + s * 1.5f, 4.25f, 0); box(0.12f, 0.5f, 0.12f); glPopMatrix(); }
    glColor3f(0.14f, 0.08f, 0.18f);
    if (signOn) setEmission(0.45f, 0.15f, 0.30f);
    glPushMatrix(); glTranslatef(0.6f, 4.9f, 0); box(3.4f, 1.1f, 0.25f); glPopMatrix();
    clearEmission();
    for (int i = 0; i < 10; i++) {
        float x = 0.6f - 1.6f + i * 0.355f;
        float lv = signOn ? 1.0f : 0.06f;
        bulb(x, 5.42f, 0.14f, 0.07f, BULB_COL[i % 4], lv, signOn); bulb(x, 4.38f, 0.14f, 0.07f, BULB_COL[(i + 1) % 4], lv, signOn);
    }
    glPushMatrix(); glTranslatef(0.6f, 4.9f, 0.14f);
    if (signOn) signText("ICE CREAM", 3.0f, 1.0f, 0.45f, 0.75f); else signText("ICE CREAM", 3.0f, 0.28f, 0.2f, 0.28f);
    glPopMatrix();
    glPushMatrix(); glTranslatef(0.6f, 4.9f, -0.14f); glRotatef(180, 0, 1, 0);
    if (signOn) signText("ICE CREAM", 3.0f, 1.0f, 0.45f, 0.75f); else signText("ICE CREAM", 3.0f, 0.28f, 0.2f, 0.28f);
    glPopMatrix();
    if (signOn && dusk > 0.1f) { const float pk[3] = {1.0f, 0.4f, 0.7f}; glPushMatrix(); glTranslatef(0.6f, 4.9f, 0.4f); glowHere(2.0f, pk, 0.25f * dusk); glPopMatrix(); }
    // giant ice-cream cone on the roof
    glColor3f(0.85f, 0.58f, 0.25f);
    glPushMatrix(); glTranslatef(-1.9f + 0.0f, 5.5f, 0); glRotatef(90, 1, 0, 0); glutSolidCone(0.5, 1.5, 12, 1); glPopMatrix();
    glColor3f(1.0f, 0.55f, 0.72f); ballAt(-1.9f, 5.8f, 0, 0.6f, 14);
    glColor3f(1.0f, 0.96f, 0.9f);  ballAt(-1.9f, 6.4f, 0, 0.47f, 14);
    glColor3f(0.9f, 0.05f, 0.1f);  ballAt(-1.9f, 6.95f, 0, 0.14f, 8);
    glPopMatrix();
}

// ------------------------- HUD -----------------------------------------
void drawHUD() {
    const char* msg = "N: Day / Sunset    SPACE: Pause    ESC: Quit";
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    float c = lerpf(0.05f, 1.0f, dusk);
    glColor3f(c, c, c);
    glRasterPos2i(14, winH - 24);
    for (const char* p = msg; *p; ++p) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *p);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}

// ------------------------- display -------------------------------------
void setupLights() {
    const float ad[4] = {0.42f, 0.44f, 0.48f, 1}, as[4] = {0.55f, 0.36f, 0.34f, 1};
    const float dd[4] = {0.78f, 0.75f, 0.68f, 1}, ds[4] = {0.95f, 0.55f, 0.32f, 1};
    GLfloat amb[4], dif[4];
    for (int i = 0; i < 4; i++) { amb[i] = lerpf(ad[i], as[i], dusk); dif[i] = lerpf(dd[i], ds[i], dusk); }
    GLfloat pos[4] = {0.3f, lerpf(0.9f, 0.55f, dusk), 0.8f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
}

void display() {
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(camX, camY, camZ, lookX, lookY, lookZ, 0, 1, 0);

    drawSkyDome();
    drawSun();
    drawCity();
    setupLights();
    drawClouds();
    drawBirds();
    drawHotAirBalloon();
    drawGround();
    drawTrees();
    drawLamps();
    drawFlowers();
    drawRollerCoaster();
    drawFerrisWheel();
    drawCircusTent();
    drawCarousel();
    drawIceCreamTruck();
    drawClown();
    drawJoker();
    for (int i = 0; i < 10; i++) drawKid(kids[i]);
    drawRunningGirl();
    drawFloatingBalloons();
    drawHUD();
    glutSwapBuffers();
}

// ------------------------- animation timer -----------------------------
void update(int) {
    // day <-> sunset transition (keeps running even when paused)
    duskRaw += (targetSunset ? 1.0f : -1.0f) * 0.003f;
    duskRaw = clamp01(duskRaw);
    dusk = duskRaw * duskRaw * (3.0f - 2.0f * duskRaw);

    if (!isPaused) {
        globalTime += 0.016f;
        ferrisAngle += 0.45f; if (ferrisAngle >= 360.0f) ferrisAngle -= 360.0f;
        carouselAngle += 0.6f; if (carouselAngle >= 360.0f) carouselAngle -= 360.0f;
        coasterS += 0.30f; if (coasterS >= trackLen) coasterS -= trackLen;
        sunPulse += 0.03f;
        limbSwing += 0.06f;

        hotAirY += 0.02f;
        hotAirX += 0.01f * sinf(globalTime * 0.5f);
        if (hotAirY > 52.0f) { hotAirY = 6.0f; hotAirX = -30.0f + rand() % 60; }

        for (size_t i = 0; i < floatingBalloons.size(); i++) {
            Balloon& b = floatingBalloons[i];
            b.y += b.speedY; b.x += b.speedX;
            if (b.y > 30.0f) { b.y = 1.5f; b.x = -28.0f + (rand() % 56); }
        }
        for (size_t i = 0; i < clouds.size(); i++) { clouds[i].x += clouds[i].speed; if (clouds[i].x > 80.0f) clouds[i].x = -80.0f; }
        for (size_t i = 0; i < birds.size(); i++)  { birds[i].x += birds[i].speed;  if (birds[i].x > 52.0f)  birds[i].x = -52.0f; }
    }
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(48.0, (double)w / (double)h, 1.0, 400.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 'n': case 'N': targetSunset = !targetSunset; break;
        case 'd': case 'D': targetSunset = false; break;
        case 's': case 'S': targetSunset = true; break;
        case ' ': isPaused = !isPaused; break;
        case 27: exit(0); break;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 800);
    glutCreateWindow("3D Amusement Park - Day & Sunset");
    initScene();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}
