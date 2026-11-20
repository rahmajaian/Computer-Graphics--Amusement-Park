// =====================================================================
//  3D AMUSEMENT PARK  -  C++ / OpenGL / freeglut   (Code::Blocks)
//
//  Modes:  D = Day    S = Sunset    N = Night    M = next mode
//  Zoom:   mouse wheel  or  + / -        Rotate: drag mouse or arrow keys
//  Views:  1 Park   2 Merry-go-round   3 Ferris wheel   4 Roller coaster
//          5 Circus tent   6 Ice-cream truck
//  Other:  P = music on/off   SPACE = pause   L = show/hide far lamp row   ESC = quit
//  (music uses the Windows PlaySound API -> link  -lwinmm)
//
//  Code::Blocks linker settings (Project > Build options > Linker settings):
//      -lfreeglut  -lopengl32  -lglu32  -lwinmm  -lgdi32
//  (in this order; make sure freeglut is installed for your MinGW)
// =====================================================================
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#endif
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
int   mode = 0;               // 0 = day, 1 = sunset, 2 = night
bool  isPaused = false;
bool  showLamps = true;
float duskRaw = 0.0f, dusk = 0.0f;     // 0 = day .. 1 = sunset/night (lights on)
float nightRaw = 0.0f, night = 0.0f;   // 0 .. 1 = full night
float globalTime = 0.0f;
int   winW = 1280, winH = 800;

// orbit camera (target point, distance, yaw/pitch in degrees)
struct Cam { float tx, ty, tz, d, yaw, pitch; };
static const Cam VIEWS[6] = {
    {0.0f, 11.0f, -12.0f, 74.0f, 0.0f, 3.5f},      // 1 whole park
    {-3.0f, 3.5f, 2.0f, 24.0f, 0.0f, 14.0f},       // 2 merry-go-round
    {-19.0f, 11.0f, -5.0f, 40.0f, 14.0f, 8.0f},    // 3 Ferris wheel
    {4.0f, 8.0f, -21.0f, 56.0f, 0.0f, 10.0f},      // 4 roller coaster
    {20.0f, 5.0f, -1.0f, 30.0f, -12.0f, 10.0f},    // 5 circus tent
    {27.5f, 2.5f, 15.5f, 22.0f, -18.0f, 12.0f}};   // 6 ice cream truck
Cam camCur = VIEWS[0], camTgt = VIEWS[0];
float camX = 0, camY = 15, camZ = 62, lookX = 0, lookY = 11, lookZ = -12;

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
std::vector<P3>      stars;


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

void initRunners();

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
        c.x = -75.0f + i * 20.0f; c.y = 47.0f + (i % 3) * 4.5f; c.z = -50.0f - (i % 3) * 8.0f;
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
    for (float x = -100.0f; x <= 100.0f; x += 25.0f) { Tree t; t.x = x + (rand() % 60) / 10.0f; t.z = -43.0f; t.s = 1.0f + (rand() % 5) * 0.08f; t.pal = rand() % 5; trees.push_back(t); }   // just a few trees

    buildTrack();
    initRunners();
    for (int i = 0; i < 260; i++) {                          // stars
        P3 st; float e = 0.1f + (rand() % 1000) / 1000.0f * 1.35f, lon = (rand() % 1000) / 1000.0f * 2 * PI_F;
        st.x = cosf(e) * cosf(lon); st.y = sinf(e); st.z = cosf(e) * sinf(lon); st.t = (float)(rand() % 100);
        stars.push_back(st);
    }
}

// ------------------------- sky, sun ----------------------------------
static void skyColors(float* hor, float* top) {
    const float dh[3] = {0.74f, 0.89f, 1.00f}, dt[3] = {0.20f, 0.50f, 0.95f};
    const float sh[3] = {1.00f, 0.60f, 0.22f}, st[3] = {0.45f, 0.30f, 0.60f};
    const float nh[3] = {0.10f, 0.16f, 0.36f}, nt[3] = {0.015f, 0.03f, 0.13f};   // navy night sky
    float h1[3], t1[3];
    lerp3(dh, sh, dusk, h1); lerp3(dt, st, dusk, t1);
    lerp3(h1, nh, night, hor); lerp3(t1, nt, night, top);
}
static void sunPos(float& x, float& y, float& z)  { x = 10.0f;  y = lerpf(lerpf(66.0f, 25.0f, dusk), -30.0f, night); z = -110.0f; }
static void moonPos(float& x, float& y, float& z) { x = 38.0f; y = lerpf(-25.0f, 60.0f, night); z = -110.0f; }

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
                float glow = (powf(dt, 10.0f) * (0.25f + 0.5f * dusk) + powf(dt, 3.0f) * 0.35f * dusk) * (1.0f - night);
                glColor3f(clamp01(c[0] + glow), clamp01(c[1] + glow * 0.55f), clamp01(c[2] + glow * 0.18f));
                glVertex3f(R * dx, R * dy, R * dz);
            }
        }
        glEnd();
    }
    if (night > 0.02f) {                                      // twinkling stars
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (size_t i = 0; i < stars.size(); i++) {
            float tw = 0.55f + 0.45f * sinf(globalTime * 3.0f + stars[i].t);
            glColor4f(1.0f, 1.0f, 0.92f, night * tw);
            glVertex3f(190.0f * stars[i].x, 190.0f * stars[i].y, 190.0f * stars[i].z);
        }
        glEnd();
        glDisable(GL_BLEND); glPointSize(1.0f);
    }
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glPopMatrix();
}

void drawSun() {
    float sx, sy, sz; sunPos(sx, sy, sz);
    if (sy < -14.0f) return;
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
        glVertex3f(r * cosf(a - w), r * sinf(a - w), 0);
        glVertex3f(r * cosf(a + w), r * sinf(a + w), 0);
        glColor4f(c[0], c[1], c[2], 0.0f);
        glVertex3f(len * cosf(a), len * sinf(a), 0);
    }
    glEnd();
    endGlow();
    glPopMatrix();
}

void drawMoon() {
    if (night < 0.01f) return;
    float mx, my, mz; moonPos(mx, my, mz);
    glPushMatrix();
    glTranslatef(mx, my, mz);
    glDisable(GL_LIGHTING);
    glColor3f(0.96f, 0.96f, 0.88f);
    glutSolidSphere(7.0, 28, 28);
    glColor3f(0.84f, 0.84f, 0.76f);                         // craters
    ballAt(-1.8f, 1.8f, 6.4f, 1.1f, 10); ballAt(2.2f, -1.0f, 6.4f, 1.5f, 10); ballAt(-0.5f, -3.0f, 6.2f, 0.8f, 10); ballAt(2.8f, 3.0f, 5.9f, 0.7f, 10);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// ------------------------- far city silhouette ------------------------
void drawCity() {
    float hor[3], top[3]; skyColors(hor, top);
    const float dd[3] = {0.30f, 0.38f, 0.52f}, ds[3] = {0.24f, 0.10f, 0.20f};
    float dark[3]; lerp3(dd, ds, dusk, dark);
    { const float dn[3] = {0.04f, 0.06f, 0.14f}; lerp3(dark, dn, night, dark); }
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
static float gateVis() { return clamp01((camCur.d - 84.0f) / 14.0f); }   // gate + front fence only show when zoomed far out

void drawGround() {
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glColor3f(0.20f, 0.55f, 0.20f);                       // grass outside the park
    glVertex3f(-300, 0, -300); glVertex3f(300, 0, -300); glVertex3f(300, 0, 300); glVertex3f(-300, 0, 300);
    glColor3f(0.70f, 0.78f, 0.88f);                       // promenade at the back
    glVertex3f(-300, 0.02f, -37); glVertex3f(300, 0.02f, -37); glVertex3f(300, 0.02f, -31); glVertex3f(-300, 0.02f, -31);
    glColor3f(0.60f, 0.72f, 0.84f);                       // grout colour under the slabs
    glVertex3f(-90, 0.03f, -31); glVertex3f(90, 0.03f, -31); glVertex3f(90, 0.03f, 35); glVertex3f(-90, 0.03f, 35);
    const float ts = 6.0f, gap = 0.08f;                   // pale-blue concrete slabs
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 11; j++) {
            unsigned int h = (unsigned int)((i * 73856093) ^ (j * 19349663)) & 0xFFFF;
            float v = (h % 5) * 0.012f;
            glColor3f(0.72f + v, 0.85f + v, 0.96f + v * 0.3f);
            float x0 = -90.0f + i * ts, z0 = -31.0f + j * ts;
            glVertex3f(x0 + gap, 0.045f, z0 + gap);      glVertex3f(x0 + gap, 0.045f, z0 + ts - gap);
            glVertex3f(x0 + ts - gap, 0.045f, z0 + ts - gap); glVertex3f(x0 + ts - gap, 0.045f, z0 + gap);
        }
    glEnd();
    for (int i = 0; i < 30; i++)                          // a few small pink / yellow dots, spaced well apart
        for (int j = 0; j < 11; j++) {
            unsigned int h = (unsigned int)((i * 73856093) ^ (j * 19349663)) & 0xFFFF;
            if (h % 4 != 0) continue;
            float cx = -90.0f + i * 6.0f + 1.2f + ((h >> 2) % 37) / 37.0f * 3.6f;
            float cz = -31.0f + j * 6.0f + 1.2f + ((h >> 5) % 31) / 31.0f * 3.6f;
            if ((h >> 3) % 2) glColor3f(1.0f, 0.60f, 0.78f); else glColor3f(1.0f, 0.88f, 0.38f);
            glBegin(GL_TRIANGLE_FAN);
            glNormal3f(0, 1, 0);
            glVertex3f(cx, 0.06f, cz);
            for (int k = 0; k <= 16; k++) { float a = k * 2 * PI_F / 16; glVertex3f(cx + 0.6f * cosf(a), 0.06f, cz + 0.6f * sinf(a)); }
            glEnd();
        }
}

static void fenceLine(float x1, float z1, float x2, float z2, bool lights, float sink) {
    float dx = x2 - x1, dz = z2 - z1, len = sqrtf(dx * dx + dz * dz);
    int n = (int)(len / 3.0f); if (n < 1) n = 1;
    float level = 0.2f + 0.9f * dusk;
    glColor3f(0.97f, 0.97f, 0.97f);
    limb(x1, 0.95f + sink, z1, x2, 0.95f + sink, z2, 0.06f, 6);
    limb(x1, 0.45f + sink, z1, x2, 0.45f + sink, z2, 0.05f, 6);
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, x = x1 + dx * t, z = z1 + dz * t;
        if (i % 2) glColor3f(0.9f, 0.2f, 0.25f); else glColor3f(0.25f, 0.5f, 0.9f);
        glPushMatrix(); glTranslatef(x, 0.65f + sink, z); box(0.26f, 1.3f, 0.26f); glPopMatrix();
        if (lights && dusk > 0.05f && i < n) bulb(x + dx / n * 0.5f, 1.12f + sink, z + dz / n * 0.5f + 0.1f, 0.09f, BULB_COL[i % 4], level * (0.7f + 0.3f * sinf(globalTime * 4 + i)), false);
    }
}

void drawFence() {                                       // small fence around the whole park
    float vis = gateVis(), sink = -(1.0f - vis) * 40.0f;
    fenceLine(-88.0f, -32.0f, 88.0f, -32.0f, true, 0.0f);
    fenceLine(-88.0f, -32.0f, -88.0f, 34.0f, false, 0.0f);
    fenceLine(88.0f, -32.0f, 88.0f, 34.0f, false, 0.0f);
    if (vis > 0.005f) {                                  // front fence with an opening for the gate
        fenceLine(-88.0f, 34.0f, -21.0f, 34.0f, false, sink);
        fenceLine(21.0f, 34.0f, 88.0f, 34.0f, false, sink);
    }
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
    if (!showLamps) return;
    for (float x = -108.0f; x <= 108.0f; x += 9.0f) drawLamp(x, -34.0f);
}

void drawFlowers() {
    for (float fx = -36.0f; fx <= 36.0f; fx += 2.2f) {
        if (fabsf(fx - 20.0f) < 9.0f || fabsf(fx + 19.0f) < 8.0f || fabsf(fx) > 28.0f) continue;
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
    { const float cn[3] = {0.20f, 0.24f, 0.42f}; lerp3(c, cn, night, c); }
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
    if (night > 0.5f) return;
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
    if (dusk > 0.3f) { const float fl[3] = {1.0f, 0.65f, 0.2f}; bulb(0, 2.7f, 0, 0.25f, fl, 0.7f + 0.3f * sinf(globalTime * 11.0f), true); }
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
    glTranslatef(-19.0f, 0.0f, -5.0f);
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
    glTranslatef(13.0f, 0, 16.5f);
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
    glTranslatef(-21.0f, 0, 15.0f);
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
const int NRUN = 12;
struct Runner { float z, xmin, xmax, speed; float shirt[3], pants[3], hair[3]; int pose, girl; float x, dir, yaw, phase; };
static Runner runners[NRUN] = {
    {20.5f, -26, 22, 0.085f, {0.2f, 0.4f, 0.95f}, {0.2f, 0.2f, 0.5f}, {0.3f, 0.2f, 0.1f}, 0, 0},
    {19.2f, -26, 22, 0.070f, {1.0f, 0.85f, 0.2f}, {0.9f, 0.3f, 0.5f}, {0.1f, 0.1f, 0.1f}, 3, 1},
    {12.8f, -21.5f, 18, 0.070f, {0.9f, 0.2f, 0.2f}, {0.2f, 0.3f, 0.6f}, {0.8f, 0.55f, 0.2f}, 1, 0},
    {11.8f, -21.5f, 18, 0.055f, {0.2f, 0.75f, 0.3f}, {0.3f, 0.3f, 0.35f}, {0.2f, 0.1f, 0.05f}, 0, 0},
    {-6.0f, -12, 12, 0.060f, {0.95f, 0.5f, 0.1f}, {0.2f, 0.25f, 0.5f}, {0.4f, 0.25f, 0.1f}, 1, 0},
    {-5.0f, -12, 12, 0.075f, {0.95f, 0.4f, 0.7f}, {0.9f, 0.6f, 0.8f}, {0.6f, 0.35f, 0.1f}, 0, 1},
    {9.5f, -22, -12, 0.060f, {0.4f, 0.8f, 0.95f}, {0.5f, 0.2f, 0.2f}, {0.1f, 0.1f, 0.1f}, 3, 0},
    {10.5f, -22, -12, 0.070f, {0.7f, 0.3f, 0.9f}, {0.2f, 0.2f, 0.3f}, {0.9f, 0.7f, 0.2f}, 1, 1},
    {17.5f, -12, 8,  0.090f, {0.95f, 0.85f, 0.2f}, {0.2f, 0.4f, 0.7f}, {0.15f, 0.1f, 0.05f}, 1, 0},
    {16.0f, -12, 8,  0.065f, {0.95f, 0.3f, 0.3f}, {0.25f, 0.25f, 0.6f}, {0.3f, 0.2f, 0.1f}, 0, 1},
    {1.5f, 4, 12, 0.050f, {0.3f, 0.85f, 0.5f}, {0.3f, 0.3f, 0.6f}, {0.2f, 0.1f, 0.1f}, 3, 0},
    {2.2f, 4, 12, 0.060f, {0.95f, 0.6f, 0.2f}, {0.7f, 0.2f, 0.5f}, {0.5f, 0.3f, 0.1f}, 1, 1}};

void initRunners() {
    for (int i = 0; i < NRUN; i++) {
        Runner& r = runners[i];
        r.x = r.xmin + (r.xmax - r.xmin) * fmodf(i * 0.37f, 1.0f);
        r.dir = (i % 2) ? -1.0f : 1.0f;
        r.yaw = r.dir * 90.0f; r.phase = (float)i;
    }
}
void updateRunners() {
    for (int i = 0; i < NRUN; i++) {
        Runner& r = runners[i];
        r.x += r.dir * r.speed;
        if (r.x > r.xmax) { r.x = r.xmax; r.dir = -1.0f; }
        if (r.x < r.xmin) { r.x = r.xmin; r.dir = 1.0f; }
        float d = r.dir * 90.0f - r.yaw;                       // turn around smoothly
        if (d > 10.0f) d = 10.0f;
        if (d < -10.0f) d = -10.0f;
        r.yaw += d;
        r.phase += r.speed * 2.5f;
    }
}

void drawKid(const Runner& k) {
    const float ph = k.phase;
    glPushMatrix();
    glTranslatef(k.x, 0, k.z);
    glRotatef(k.yaw, 0, 1, 0);
    glScalef(1.5f, 1.5f, 1.5f);
    glTranslatef(0, fabsf(sinf(ph)) * 0.08f, 0);
    glRotatef(6.0f, 1, 0, 0);
    for (int s = -1; s <= 1; s += 2) {                                   // running legs
        float p = ph + (s > 0 ? PI_F : 0.0f);
        float fy = 0.08f + 0.22f * fmaxf(0.0f, cosf(p)), fz = 0.4f * sinf(p);
        col3(k.pants);
        limb(s * 0.12f, 0.85f, 0, s * 0.12f, fy, fz, 0.09f);
        glColor3f(0.15f, 0.15f, 0.15f);
        glPushMatrix(); glTranslatef(s * 0.12f, fy - 0.04f, fz + 0.06f); box(0.16f, 0.08f, 0.28f); glPopMatrix();
    }
    col3(k.shirt);
    glPushMatrix(); glTranslatef(0, 1.2f, 0); box(0.5f, 0.65f, 0.3f); glPopMatrix();
    if (k.girl) { col3(k.pants); glPushMatrix(); glTranslatef(0, 0.78f, 0); glRotatef(-90, 1, 0, 0); glutSolidCone(0.38, 0.45, 10, 1); glPopMatrix(); }
    glColor3f(1.0f, 0.78f, 0.62f);
    glPushMatrix(); glTranslatef(0, 1.8f, 0); glutSolidSphere(0.26, 14, 14);
    col3(k.hair); glPushMatrix(); glTranslatef(0, 0.07f, -0.05f); glScalef(1.0f, 0.85f, 1.0f); glutSolidSphere(0.28, 12, 12); glPopMatrix();
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.09f, 0.0f, 0.23f, 0.035f, 6); ballAt(0.09f, 0.0f, 0.23f, 0.035f, 6);
    glPopMatrix();
    // arms: swinging, one waving (pose 1) or both cheering (pose 3)
    float lx = -0.3f, ly = 1.05f, lz = 0.35f * sinf(ph + PI_F);
    float rx = 0.3f,  ry = 1.05f, rz = 0.35f * sinf(ph);
    if (k.pose == 1) { rx = 0.5f; ry = 2.0f + 0.1f * sinf(ph * 2); rz = 0.05f; }
    if (k.pose == 3) { lx = -0.45f; ly = 2.0f + 0.1f * sinf(ph * 2); lz = 0.05f; rx = 0.45f; ry = 2.0f + 0.1f * sinf(ph * 2 + 1); rz = 0.05f; }
    col3(k.shirt);
    limb(-0.3f, 1.45f, 0, lx, ly, lz, 0.07f); limb(0.3f, 1.45f, 0, rx, ry, rz, 0.07f);
    glColor3f(1.0f, 0.78f, 0.62f);
    ballAt(lx, ly, lz, 0.08f, 8); ballAt(rx, ry, rz, 0.08f, 8);
    glPopMatrix();
}

// running girl with a dripping ice cream
void drawRunningGirl() {
    float a = globalTime * 0.45f;
    float px = 8.0f + 3.5f * cosf(a), pz = 7.0f + 3.5f * sinf(a);
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
    glTranslatef(27.5f, 0, 15.5f);
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

// ------------------------- static people, chairs, food stalls -----------
static const float PAL[8][3]  = {{0.9f, 0.2f, 0.2f}, {0.2f, 0.45f, 0.95f}, {0.95f, 0.8f, 0.2f}, {0.25f, 0.75f, 0.35f}, {0.95f, 0.45f, 0.7f}, {0.6f, 0.3f, 0.85f}, {0.95f, 0.55f, 0.15f}, {0.3f, 0.8f, 0.85f}};
static const float HAIRC[4][3] = {{0.1f, 0.08f, 0.06f}, {0.4f, 0.25f, 0.1f}, {0.85f, 0.65f, 0.25f}, {0.55f, 0.2f, 0.1f}};
static const float JEANC[3][3] = {{0.2f, 0.3f, 0.6f}, {0.25f, 0.25f, 0.3f}, {0.5f, 0.3f, 0.2f}};

// pose: 0 arms down, 1 holding an ice cream, 2 both arms forward, 3 eating (hand to mouth)
void drawPerson(float x, float y, float z, float yaw, float sc, const float* shirt, const float* pants, const float* hair, int pose, int girl, bool seated = false) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yaw, 0, 1, 0);
    glScalef(sc, sc, sc);
    col3(pants);
    for (int s = -1; s <= 1; s += 2) {
        if (seated) { limb(s * 0.12f, 0.45f, 0, s * 0.12f, 0.5f, 0.45f, 0.09f); limb(s * 0.12f, 0.5f, 0.45f, s * 0.12f, 0.08f, 0.5f, 0.09f); }
        else limb(s * 0.12f, 0.85f, 0, s * 0.12f, 0.08f, 0, 0.09f);
        glColor3f(0.15f, 0.15f, 0.15f);
        glPushMatrix(); glTranslatef(s * 0.12f, 0.04f, seated ? 0.55f : 0.06f); box(0.16f, 0.08f, 0.28f); glPopMatrix();
        col3(pants);
    }
    glPushMatrix();
    glTranslatef(0, seated ? -0.42f : 0.0f, 0);
    col3(shirt);
    glPushMatrix(); glTranslatef(0, 1.2f, 0); box(0.5f, 0.65f, 0.3f); glPopMatrix();
    if (girl) { col3(pants); glPushMatrix(); glTranslatef(0, 0.78f, 0); glRotatef(-90, 1, 0, 0); glutSolidCone(0.38, 0.45, 10, 1); glPopMatrix(); }
    glColor3f(1.0f, 0.78f, 0.62f);
    glPushMatrix(); glTranslatef(0, 1.8f, 0); glutSolidSphere(0.26, 14, 14);
    col3(hair); glPushMatrix(); glTranslatef(0, 0.07f, -0.05f); glScalef(1.0f, 0.85f, 1.0f); glutSolidSphere(0.28, 12, 12); glPopMatrix();
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.09f, 0.0f, 0.23f, 0.035f, 6); ballAt(0.09f, 0.0f, 0.23f, 0.035f, 6);
    glPopMatrix();
    float lx = -0.34f, ly = 0.9f, lz = 0.05f, rx = 0.34f, ry = 0.9f, rz = 0.05f;
    if (pose == 1) { rx = 0.3f; ry = 1.3f; rz = 0.5f; }
    if (pose == 2) { lx = -0.25f; ly = 1.2f; lz = 0.55f; rx = 0.25f; ry = 1.2f; rz = 0.55f; }
    if (pose == 3) { rx = 0.15f; ry = 1.62f; rz = 0.28f; }
    col3(shirt);
    limb(-0.3f, 1.45f, 0, lx, ly, lz, 0.07f); limb(0.3f, 1.45f, 0, rx, ry, rz, 0.07f);
    glColor3f(1.0f, 0.78f, 0.62f);
    ballAt(lx, ly, lz, 0.08f, 8); ballAt(rx, ry, rz, 0.08f, 8);
    if (pose == 1) {                                          // ice cream in hand
        glPushMatrix(); glTranslatef(rx, ry + 0.2f, rz); glColor3f(0.85f, 0.58f, 0.25f);
        glPushMatrix(); glRotatef(90, 1, 0, 0); glutSolidCone(0.09, 0.32, 8, 1); glPopMatrix();
        glColor3f(1.0f, 0.55f, 0.72f); ballAt(0, 0.07f, 0, 0.11f, 10);
        glPopMatrix();
    }
    glPopMatrix();
    glPopMatrix();
}

void drawChair(float x, float z, float yaw) {
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(yaw, 0, 1, 0);
    glColor3f(0.55f, 0.35f, 0.2f);
    glPushMatrix(); glTranslatef(0, 0.4f, 0); box(0.55f, 0.06f, 0.55f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0.75f, -0.26f); box(0.55f, 0.6f, 0.05f); glPopMatrix();
    for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) limb(a * 0.24f, 0, b * 0.24f, a * 0.24f, 0.4f, b * 0.24f, 0.03f, 6);
    glPopMatrix();
}

void drawTableSet(float x, float z, int variant) {
    static const float U[4][2][3] = {{{0.9f, 0.15f, 0.15f}, {1, 1, 1}}, {{0.2f, 0.5f, 0.95f}, {1, 1, 1}}, {{1.0f, 0.8f, 0.15f}, {1, 1, 1}}, {{0.2f, 0.7f, 0.35f}, {1, 1, 1}}};
    float level = 0.2f + 0.9f * dusk;
    glPushMatrix();
    glTranslatef(x, 0, z);
    glColor3f(0.4f, 0.4f, 0.45f); cylY(0.08f, 1.05f, 8);
    glColor3f(0.9f, 0.85f, 0.75f); glPushMatrix(); glTranslatef(0, 1.05f, 0); cylY(1.0f, 0.08f, 20); glPopMatrix();
    glColor3f(0.35f, 0.35f, 0.4f); cylY(0.05f, 3.2f, 8);                       // umbrella pole
    stripedCone(2.3f, 3.0f, 0.9f, 8, U[variant % 4][0], U[variant % 4][1]);
    for (int i = 0; i < 8; i++) { float a = i * 2 * PI_F / 8; bulb(2.3f * cosf(a), 3.0f, 2.3f * sinf(a), 0.07f, BULB_COL[i % 4], level, false); }
    for (int k = 0; k < 4; k++) {                                              // chairs + diners
        float a = k * PI_F / 2 + variant * 0.5f;
        float cx = 1.7f * cosf(a), cz = 1.7f * sinf(a);
        float yaw = atan2f(-cx, -cz) * DEG;
        drawChair(cx, cz, yaw);
        if (k < 3) drawPerson(cx, 0, cz, yaw, 1.3f, PAL[(variant * 3 + k) % 8], JEANC[k % 3], HAIRC[(k + variant) % 4], 3, (k + variant) % 2 == 0, true);
    }
    glColor3f(0.95f, 0.4f, 0.4f); ballAt(0.3f, 1.2f, 0.2f, 0.16f, 8);        // snacks on the table
    glColor3f(0.9f, 0.75f, 0.3f); ballAt(-0.3f, 1.18f, -0.2f, 0.14f, 8);
    glPopMatrix();
}

// type: 0 popcorn shop, 1 food shop, 2 balloon shop, 3 shooting range
void drawShop(float x, float z, float yaw, int type) {
    static const char* NAMES[7] = {"POPCORN", "FOOD SHOP", "BALLOONS", "SHOOTING", "COTTON CANDY", "PIZZA", "DRINKS"};
    static const float AW[7][2][3] = {{{1.0f, 0.8f, 0.15f}, {0.9f, 0.15f, 0.15f}}, {{0.9f, 0.15f, 0.15f}, {1, 1, 1}},
                                       {{0.25f, 0.55f, 0.95f}, {1, 1, 1}}, {{0.2f, 0.65f, 0.3f}, {1, 1, 1}},
                                       {{0.95f, 0.45f, 0.7f}, {1, 1, 1}}, {{0.2f, 0.65f, 0.3f}, {1, 0.9f, 0.8f}}, {{0.2f, 0.7f, 0.9f}, {1, 1, 0.6f}}};
    float level = 0.25f + 0.85f * dusk;
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(yaw, 0, 1, 0);
    glColor3f(0.96f, 0.92f, 0.84f);                                           // walls
    glPushMatrix(); glTranslatef(0, 1.8f, -1.0f); box(4.2f, 3.6f, 0.2f); glPopMatrix();
    for (int sd = -1; sd <= 1; sd += 2) { glPushMatrix(); glTranslatef(sd * 2.05f, 1.8f, 0.1f); box(0.15f, 3.6f, 2.3f); glPopMatrix(); }
    col3(AW[type][0]); glPushMatrix(); glTranslatef(0, 0.55f, 1.0f); box(4.2f, 1.1f, 0.9f); glPopMatrix();   // counter
    glColor3f(0.55f, 0.38f, 0.22f); glPushMatrix(); glTranslatef(0, 1.13f, 1.0f); box(4.5f, 0.1f, 1.2f); glPopMatrix();
    glBegin(GL_QUADS);                                                        // striped awning
    for (int i = 0; i < 7; i++) {
        float x0 = -2.2f + i * 0.63f, x1 = x0 + 0.63f;
        col3(AW[type][i % 2]);
        glNormal3f(0, 0.98f, 0.2f);
        glVertex3f(x0, 3.9f, -1.1f); glVertex3f(x1, 3.9f, -1.1f); glVertex3f(x1, 3.3f, 1.8f); glVertex3f(x0, 3.3f, 1.8f);
        glNormal3f(0, 0, 1);
        glVertex3f(x0, 3.3f, 1.8f); glVertex3f(x1, 3.3f, 1.8f); glVertex3f(x1, 3.0f, 1.8f); glVertex3f(x0, 3.0f, 1.8f);
    }
    glEnd();
    glColor3f(0.15f, 0.1f, 0.12f); glPushMatrix(); glTranslatef(0, 4.5f, -0.95f); box(3.7f, 0.8f, 0.12f); glPopMatrix();   // sign
    glPushMatrix(); glTranslatef(0, 4.5f, -0.87f); signText(NAMES[type], type == 4 ? 3.4f : 2.8f, 1.0f, 0.9f, 0.4f); glPopMatrix();
    for (int i = 0; i < 9; i++) bulb(-2.2f + i * 0.55f, 3.0f, 1.82f, 0.07f, BULB_COL[i % 4], level, false);

    switch (type) {
        case 0:                                                               // popcorn machine + boxes
            glColor3f(1.0f, 0.97f, 0.8f); glPushMatrix(); glTranslatef(-1.2f, 1.95f, 0.6f); box(1.2f, 1.5f, 0.8f); glPopMatrix();
            glColor3f(0.9f, 0.15f, 0.15f); glPushMatrix(); glTranslatef(-1.2f, 2.8f, 0.6f); box(1.35f, 0.25f, 0.95f); glPopMatrix();
            glColor3f(1.0f, 0.95f, 0.75f); for (int i = 0; i < 6; i++) ballAt(-1.2f + (i % 3) * 0.3f, 2.2f + (i / 3) * 0.3f, 0.7f, 0.17f, 8);
            for (int i = 0; i < 3; i++) { glPushMatrix(); glTranslatef(0.3f + i * 0.6f, 1.18f, 1.0f); glColor3f(0.9f, 0.15f, 0.15f); cylY(0.22f, 0.45f, 10); glColor3f(1.0f, 0.95f, 0.75f); ballAt(0, 0.5f, 0, 0.24f, 8); glPopMatrix(); }
            break;
        case 1:                                                               // hot dogs + burgers
            for (int i = 0; i < 3; i++) { float px = -1.6f + i * 0.75f; glColor3f(0.9f, 0.7f, 0.4f); glPushMatrix(); glTranslatef(px, 1.25f, 1.0f); box(0.55f, 0.14f, 0.22f); glPopMatrix(); glColor3f(0.8f, 0.2f, 0.15f); limb(px - 0.3f, 1.35f, 1.0f, px + 0.3f, 1.35f, 1.0f, 0.07f); }
            for (int i = 0; i < 2; i++) { glPushMatrix(); glTranslatef(0.7f + i * 1.0f, 1.18f, 1.0f); glColor3f(0.85f, 0.6f, 0.3f); cylY(0.3f, 0.12f, 12); glTranslatef(0, 0.12f, 0); glColor3f(0.45f, 0.25f, 0.15f); cylY(0.32f, 0.1f, 12); glTranslatef(0, 0.1f, 0); glColor3f(0.3f, 0.75f, 0.25f); cylY(0.33f, 0.04f, 12); glTranslatef(0, 0.04f, 0); glColor3f(0.85f, 0.6f, 0.3f); cylY(0.3f, 0.14f, 12); glPopMatrix(); }
            break;
        case 2:                                                               // balloon bunches above the shop
            for (int side = -1; side <= 1; side += 2) {
                float ax = side * 1.7f;
                for (int k = 0; k < 7; k++) {
                    float ang = k * 0.9f + side, bx = ax + cosf(ang) * (0.4f + (k % 3) * 0.35f), bz = 0.3f + sinf(ang) * 0.7f;
                    float by = 5.6f + (k % 3) * 0.7f + 0.12f * sinf(globalTime * 1.5f + k + side);
                    glDisable(GL_LIGHTING); glColor3f(0.85f, 0.85f, 0.85f);
                    glBegin(GL_LINES); glVertex3f(ax, 3.9f, 0.3f); glVertex3f(bx, by - 0.5f, bz); glEnd();
                    glEnable(GL_LIGHTING);
                    col3(PAL[(k + (side > 0 ? 3 : 0)) % 8]);
                    glPushMatrix(); glTranslatef(bx, by, bz); glScalef(0.85f, 1.1f, 0.85f); glutSolidSphere(0.5, 12, 12); glPopMatrix();
                }
            }
            for (int k = 0; k < 5; k++) { col3(PAL[(k * 3) % 8]); glPushMatrix(); glTranslatef(-1.6f + k * 0.8f, 1.5f, 1.0f); glScalef(0.9f, 1.1f, 0.9f); glutSolidSphere(0.32, 10, 10); glPopMatrix(); }
            break;
        case 3:                                                               // shooting range: targets, plush prizes, rifles
            for (int t = 0; t < 3; t++) {
                float tx = -1.3f + t * 1.3f;
                glPushMatrix(); glTranslatef(tx, 2.6f, -0.88f);
                glColor3f(0.9f, 0.1f, 0.1f); glutSolidCylinder(0.62, 0.03, 18, 1);
                glTranslatef(0, 0, 0.01f); glColor3f(1, 1, 1); glutSolidCylinder(0.46, 0.03, 18, 1);
                glTranslatef(0, 0, 0.01f); glColor3f(0.9f, 0.1f, 0.1f); glutSolidCylinder(0.3, 0.03, 18, 1);
                glTranslatef(0, 0, 0.01f); glColor3f(1, 1, 1); glutSolidCylinder(0.16, 0.03, 18, 1);
                glPopMatrix();
            }
            for (int side = -1; side <= 1; side += 2) {                       // plush bears
                glPushMatrix(); glTranslatef(side * 1.75f, 1.45f, 1.0f); glColor3f(0.7f, 0.45f, 0.25f);
                glutSolidSphere(0.3, 10, 10); ballAt(0, 0.38f, 0, 0.2f, 10); ballAt(-0.14f, 0.55f, 0, 0.07f, 8); ballAt(0.14f, 0.55f, 0, 0.07f, 8);
                glPopMatrix();
            }
            glColor3f(0.25f, 0.25f, 0.28f);
            limb(-0.5f, 1.3f, 1.4f, -0.5f, 1.3f, 0.6f, 0.04f); limb(0.5f, 1.3f, 1.4f, 0.5f, 1.3f, 0.6f, 0.04f);
            break;
        case 4:                                                               // cotton candy
            for (int i = 0; i < 4; i++) { float px = -1.4f + i * 0.93f; glColor3f(0.9f, 0.85f, 0.7f); glPushMatrix(); glTranslatef(px, 1.18f, 1.0f); cylY(0.03f, 0.5f, 6); glPopMatrix(); col3(PAL[(i % 2) ? 4 : 7]); ballAt(px, 1.9f, 1.0f, 0.38f, 12); }
            break;
        case 5:                                                               // pizza
            for (int i = 0; i < 3; i++) { glPushMatrix(); glTranslatef(-1.3f + i * 1.3f, 1.2f, 1.0f); glColor3f(0.95f, 0.72f, 0.28f); cylY(0.55f, 0.07f, 16); glColor3f(0.85f, 0.15f, 0.1f); ballAt(0.15f, 0.1f, 0.1f, 0.1f, 6); ballAt(-0.2f, 0.1f, -0.15f, 0.1f, 6); glPopMatrix(); }
            break;
        default:                                                              // drinks
            for (int i = 0; i < 6; i++) { glPushMatrix(); glTranslatef(-1.6f + i * 0.64f, 1.18f, 1.0f); col3(PAL[(i * 2) % 8]); cylY(0.15f, 0.4f, 10); glPopMatrix(); }
            break;
    }
    // vendor (chef hat) and two customers
    drawPerson(0, 0, -0.1f, 0, 1.5f, PAL[(type * 2 + 1) % 8], JEANC[type % 3], HAIRC[type % 4], 2, 0);
    glColor3f(1, 1, 1); glPushMatrix(); glTranslatef(0, 2.95f, -0.1f); cylY(0.3f, 0.3f, 10); ballAt(0, 0.35f, 0, 0.36f, 10); glPopMatrix();
    drawPerson(-1.1f, 0, 2.8f, 180, 1.5f, PAL[(type + 2) % 8], JEANC[(type + 1) % 3], HAIRC[(type + 1) % 4], 2, 0);
    drawPerson(0.9f, 0, 3.0f, 188, 1.5f, PAL[(type + 5) % 8], PAL[(type + 3) % 8], HAIRC[(type + 2) % 4], type == 2 ? 3 : 1, 1);
    if (type == 3) { glColor3f(0.25f, 0.25f, 0.28f); limb(-1.1f, 1.95f, 2.6f, -1.1f, 1.95f, 1.2f, 0.05f); }          // customer aims a rifle
    if (type == 2) {                                                          // customers hold balloons
        for (int k = 0; k < 2; k++) {
            float bx = k ? 0.9f : -1.1f, bz = k ? 3.0f : 2.8f;
            glDisable(GL_LIGHTING); glColor3f(0.85f, 0.85f, 0.85f);
            glBegin(GL_LINES); glVertex3f(bx, 2.0f, bz); glVertex3f(bx, 3.6f, bz); glEnd(); glEnable(GL_LIGHTING);
            col3(PAL[k ? 4 : 0]); glPushMatrix(); glTranslatef(bx, 4.1f, bz); glScalef(0.85f, 1.1f, 0.85f); glutSolidSphere(0.5, 12, 12); glPopMatrix();
        }
    }
    glPopMatrix();
}

void drawFoodCart(float x, float z, float yaw) {
    float level = 0.25f + 0.85f * dusk;
    const float red[3] = {0.9f, 0.15f, 0.15f}, white[3] = {1, 1, 1};
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(yaw, 0, 1, 0);
    glColor3f(0.95f, 0.3f, 0.3f); glPushMatrix(); glTranslatef(0, 1.0f, 0); box(2.8f, 1.2f, 1.5f); glPopMatrix();
    glColor3f(1, 1, 1);           glPushMatrix(); glTranslatef(0, 1.0f, 0); box(2.82f, 0.22f, 1.52f); glPopMatrix();
    glColor3f(0.55f, 0.38f, 0.22f); glPushMatrix(); glTranslatef(0, 1.65f, 0); box(3.0f, 0.08f, 1.7f); glPopMatrix();
    glColor3f(0.08f, 0.08f, 0.1f);
    for (int sd = -1; sd <= 1; sd += 2) for (int zs = -1; zs <= 1; zs += 2) { glPushMatrix(); glTranslatef(sd * 0.9f, 0.5f, zs * 0.8f + (zs > 0 ? 0.0f : -0.15f)); glutSolidCylinder(0.5, 0.15, 14, 1); glPopMatrix(); }
    glColor3f(0.3f, 0.3f, 0.33f); limb(-1.4f, 1.2f, 0, -2.3f, 1.55f, 0, 0.05f);                 // handle
    glColor3f(0.35f, 0.35f, 0.4f); glPushMatrix(); glTranslatef(0.6f, 1.65f, -0.3f); cylY(0.05f, 1.8f, 8); glPopMatrix();   // umbrella
    glPushMatrix(); glTranslatef(0.6f, 0, -0.3f); stripedCone(2.2f, 3.3f, 0.8f, 10, red, white); glPopMatrix();
    for (int i = 0; i < 10; i++) { float a = i * 2 * PI_F / 10; bulb(0.6f + 2.2f * cosf(a), 3.3f, -0.3f + 2.2f * sinf(a), 0.07f, BULB_COL[i % 4], level, false); }
    for (int i = 0; i < 4; i++) {                                                               // hot dogs on top
        float px = -1.0f + i * 0.6f;
        glColor3f(0.9f, 0.7f, 0.4f); glPushMatrix(); glTranslatef(px, 1.75f, 0.2f); box(0.5f, 0.12f, 0.2f); glPopMatrix();
        glColor3f(0.8f, 0.2f, 0.15f); limb(px - 0.27f, 1.84f, 0.2f, px + 0.27f, 1.84f, 0.2f, 0.06f);
    }
    glColor3f(0.15f, 0.1f, 0.12f); glPushMatrix(); glTranslatef(0, 1.0f, 0.78f); box(2.2f, 0.5f, 0.05f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 1.0f, 0.82f); signText("FOOD CART", 1.9f, 1.0f, 0.9f, 0.4f); glPopMatrix();
    drawPerson(0.0f, 0, -1.2f, 0, 1.4f, PAL[1], JEANC[1], HAIRC[1], 2, 0);
    drawPerson(0.4f, 0, 2.0f, 180, 1.5f, PAL[6], JEANC[0], HAIRC[2], 1, 1);
    drawPerson(-1.0f, 0, 2.4f, 175, 1.3f, PAL[3], JEANC[2], HAIRC[0], 0, 0);
    glPopMatrix();
}

struct ShopDef { float x, z, yaw; int type; };
static const ShopDef shops[] = {                       // left and right edges of the park
    {-33.0f, 1.0f, 55, 0}, {-33.0f, 8.5f, 55, 2}, {-31.5f, 16.0f, 45, 3},
    {34.5f, 0.8f, -55, 1}, {34.5f, -6.5f, -55, 2},
    {-45.0f, 4.0f, 60, 4}, {-45.0f, 12.5f, 60, 5}, {47.0f, 5.5f, -60, 6}, {46.0f, 13.5f, -60, 4}};

struct CrowdDef { float x, z, yaw; int pose, girl, c1, c2, hair; };
static const CrowdDef crowd[] = {
    {-17.0f, 0.6f, 180, 0, 0, 0, 0, 0}, {-19.5f, 1.4f, 185, 1, 1, 4, 1, 1}, {-15.8f, 1.9f, 175, 0, 0, 2, 2, 2},   // Ferris wheel queue
    {26.5f, 19.4f, 172, 1, 0, 1, 1, 3}, {28.6f, 19.9f, 182, 2, 1, 5, 0, 0}, {30.3f, 19.2f, 188, 1, 0, 6, 2, 1},   // at the truck window
    {19.4f, 6.9f, 190, 0, 1, 7, 1, 2}, {22.2f, 6.7f, 170, 0, 0, 3, 0, 0}};                                        // at the tent door

void drawStallsAndSeating() {
    for (size_t i = 0; i < sizeof(shops) / sizeof(shops[0]); i++) drawShop(shops[i].x, shops[i].z, shops[i].yaw, shops[i].type);
    drawFoodCart(33.0f, 8.0f, -55.0f);
    drawTableSet(-26.0f, 10.0f, 0);
    drawTableSet(26.0f, 10.5f, 1);
    for (size_t i = 0; i < sizeof(crowd) / sizeof(crowd[0]); i++) {
        const CrowdDef& c = crowd[i];
        drawPerson(c.x, 0, c.z, c.yaw, 1.5f, PAL[c.c1], JEANC[c.c2 % 3], HAIRC[c.hair], c.pose, c.girl);
    }
}

// ------------------------- fireworks ---------------------
struct Spark  { float x, y, z, vx, vy, vz, life, maxLife, r, g, b; };
struct Rocket { float x, y, z, vy, ty, r, g, b; };
std::vector<Spark>  sparks;
std::vector<Rocket> rockets;
float fwTimer = 20.0f;

static void fwColor(int k, float* c) {
    static const float P[6][3] = {{1.0f, 0.25f, 0.25f}, {1.0f, 0.85f, 0.2f}, {0.3f, 1.0f, 0.4f}, {0.3f, 0.6f, 1.0f}, {1.0f, 0.4f, 0.9f}, {0.9f, 0.95f, 1.0f}};
    for (int i = 0; i < 3; i++) c[i] = P[k % 6][i];
}
static void launchRocket() {
    Rocket r; r.x = -40.0f + rand() % 80; r.z = -52.0f - rand() % 12; r.y = 2.0f;
    r.vy = 0.8f + (rand() % 30) * 0.01f; r.ty = 46.0f + rand() % 16;
    float c[3]; fwColor(rand() % 6, c); r.r = c[0]; r.g = c[1]; r.b = c[2];
    rockets.push_back(r);
}
static void explodeRocket(const Rocket& r) {
    int kind = rand() % 3;
    float c2[3]; fwColor(rand() % 6, c2);
    for (int i = 0; i < 80 && sparks.size() < 2000; i++) {
        Spark p; p.x = r.x; p.y = r.y; p.z = r.z;
        float th = acosf(1.0f - 2.0f * (rand() % 1000) / 1000.0f), ph = 2 * PI_F * (rand() % 1000) / 1000.0f;
        float sp = 0.30f + (rand() % 100) * 0.001f;
        if (kind == 1) { p.vx = sp * cosf(ph); p.vy = sp * sinf(ph); p.vz = 0; }          // flat ring
        else { p.vx = sp * sinf(th) * cosf(ph); p.vy = sp * cosf(th); p.vz = sp * sinf(th) * sinf(ph); }
        p.maxLife = p.life = 70.0f + rand() % 35;
        bool second = (kind == 2 && i % 2);
        p.r = second ? c2[0] : r.r; p.g = second ? c2[1] : r.g; p.b = second ? c2[2] : r.b;
        sparks.push_back(p);
    }
}
void updateFireworks() {
    if (night > 0.6f) { fwTimer -= 1.0f; if (fwTimer <= 0.0f) { launchRocket(); if (rand() % 4 == 0) launchRocket(); fwTimer = 35.0f + rand() % 60; } }
    for (size_t i = 0; i < rockets.size();) {
        Rocket& r = rockets[i];
        r.y += r.vy;
        if (sparks.size() < 2000) { Spark t; t.x = r.x; t.y = r.y; t.z = r.z; t.vx = (rand() % 20 - 10) * 0.002f; t.vy = -0.05f; t.vz = (rand() % 20 - 10) * 0.002f; t.maxLife = t.life = 16.0f; t.r = 1.0f; t.g = 0.7f; t.b = 0.3f; sparks.push_back(t); }
        if (r.y >= r.ty) { explodeRocket(r); rockets.erase(rockets.begin() + i); } else i++;
    }
    for (size_t i = 0; i < sparks.size();) {
        Spark& p = sparks[i];
        p.x += p.vx; p.y += p.vy; p.z += p.vz;
        p.vx *= 0.975f; p.vy = p.vy * 0.975f - 0.004f; p.vz *= 0.975f;
        p.life -= 1.0f;
        if (p.life <= 0.0f || p.y < 0.0f) sparks.erase(sparks.begin() + i); else i++;
    }
}
void drawFireworks() {
    if (sparks.empty() && rockets.empty()) return;
    glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < sparks.size(); i++) { const Spark& p = sparks[i]; glColor4f(p.r, p.g, p.b, p.life / p.maxLife); glVertex3f(p.x, p.y, p.z); }
    for (size_t i = 0; i < rockets.size(); i++) { glColor4f(1.0f, 0.9f, 0.6f, 1.0f); glVertex3f(rockets[i].x, rockets[i].y, rockets[i].z); }
    glEnd();
    glBegin(GL_LINES);                                                         // short glowing streaks
    for (size_t i = 0; i < sparks.size(); i++) {
        const Spark& p = sparks[i]; float a = p.life / p.maxLife;
        glColor4f(p.r, p.g, p.b, a * 0.6f); glVertex3f(p.x, p.y, p.z);
        glColor4f(p.r, p.g, p.b, 0.0f);     glVertex3f(p.x - p.vx * 4.0f, p.y - p.vy * 4.0f, p.z - p.vz * 4.0f);
    }
    glEnd();
    glPointSize(1.0f);
    endGlow();
}

// ------------------------- music (synthesised soft music-box tune) ------------
static std::vector<unsigned char> wavData;
static bool musicOn = false;
struct Note { int midi; float beats; };
static void addTone(std::vector<float>& buf, float start, float ring, float freq, int type, float amp) {
    const float SR = 22050.0f;
    int s0 = (int)(start * SR), n = (int)(ring * SR);
    for (int i = 0; i < n; i++) {
        int idx = s0 + i; if (idx >= (int)buf.size()) break;
        float t = i / SR, ph = 2.0f * PI_F * freq * t;
        float env = (t < 0.004f ? t / 0.004f : 1.0f) * expf(-t * (type == 0 ? 2.4f : 3.4f));       // soft music-box decay
        float v = sinf(ph) + 0.30f * sinf(2 * ph) + 0.10f * sinf(4 * ph);
        buf[idx] += v * env * amp;
    }
}
static void putU32(std::vector<unsigned char>& w, unsigned int v) { for (int i = 0; i < 4; i++) w.push_back((unsigned char)((v >> (8 * i)) & 255)); }
static void putU16(std::vector<unsigned char>& w, unsigned int v) { for (int i = 0; i < 2; i++) w.push_back((unsigned char)((v >> (8 * i)) & 255)); }
static void buildMusic() {                                // gentle music-box lullaby in 3/4
    const float SR = 22050.0f, beat = 60.0f / 88.0f;
    static const Note mel[] = {
        {76, 1}, {79, 1}, {81, 1},   {79, 2}, {76, 1},   {74, 1}, {76, 1}, {79, 1},   {76, 2}, {74, 1},
        {72, 1}, {74, 1}, {76, 1},   {79, 1}, {76, 1}, {74, 1},   {72, 1}, {74, 1}, {69, 1},   {72, 3},
        {79, 1}, {81, 1}, {84, 1},   {81, 2}, {79, 1},   {76, 1}, {79, 1}, {81, 1},   {79, 3},
        {81, 1}, {79, 1}, {76, 1},   {74, 1}, {76, 1}, {79, 1},   {76, 1}, {74, 1}, {72, 1},   {72, 3}};
    static const int chordId[16] = {0, 0, 1, 2, 3, 0, 1, 0, 0, 3, 2, 1, 3, 1, 0, 0};      // C, G, Am, F
    static const int arp[4][3] = {{48, 55, 64}, {43, 50, 59}, {45, 52, 60}, {41, 48, 57}};
    std::vector<float> buf((int)((48 * beat + 2.0f) * SR), 0.0f);
    float t = 0;
    for (size_t i = 0; i < sizeof(mel) / sizeof(mel[0]); i++) {
        addTone(buf, t, 2.6f, 440.0f * powf(2.0f, (mel[i].midi - 69) / 12.0f), 0, 0.30f);
        t += mel[i].beats * beat;
    }
    for (int bar = 0; bar < 16; bar++)                        // soft arpeggio underneath
        for (int k = 0; k < 3; k++)
            addTone(buf, (bar * 3 + k) * beat, 1.6f, 440.0f * powf(2.0f, (arp[chordId[bar]][k] - 69) / 12.0f), 1, 0.13f);
    const int d = (int)(0.33f * SR);                          // gentle echo
    for (size_t i = d; i < buf.size(); i++) buf[i] += 0.28f * buf[i - d];
    int fade = (int)(0.5f * SR);
    for (int i = 0; i < fade; i++) buf[buf.size() - 1 - i] *= (float)i / fade;
    float mx = 0.001f;
    for (size_t i = 0; i < buf.size(); i++) if (fabsf(buf[i]) > mx) mx = fabsf(buf[i]);
    wavData.clear();
    unsigned int dataBytes = (unsigned int)buf.size() * 2;
    wavData.push_back('R'); wavData.push_back('I'); wavData.push_back('F'); wavData.push_back('F'); putU32(wavData, 36 + dataBytes);
    wavData.push_back('W'); wavData.push_back('A'); wavData.push_back('V'); wavData.push_back('E');
    wavData.push_back('f'); wavData.push_back('m'); wavData.push_back('t'); wavData.push_back(' ');
    putU32(wavData, 16); putU16(wavData, 1); putU16(wavData, 1); putU32(wavData, 22050); putU32(wavData, 44100); putU16(wavData, 2); putU16(wavData, 16);
    wavData.push_back('d'); wavData.push_back('a'); wavData.push_back('t'); wavData.push_back('a'); putU32(wavData, dataBytes);
    for (size_t i = 0; i < buf.size(); i++) putU16(wavData, (unsigned int)(short)(buf[i] / mx * 0.7f * 32767.0f) & 0xFFFF);
}
static void setMusic(bool on) {
#ifdef _WIN32
    if (on && !wavData.empty()) PlaySoundA((LPCSTR)&wavData[0], NULL, SND_MEMORY | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
    else PlaySoundA(NULL, NULL, 0);
#endif
    musicOn = on;
}

// ------------------------- extra attractions ----------------------------
static void coneUp(float r, float h) { glPushMatrix(); glRotatef(-90, 1, 0, 0); glutSolidCone(r, h, 14, 1); glPopMatrix(); }
static void flagAt(float x, float y, float z, float len, float r, float g, float b) {
    float w = 0.25f * sinf(globalTime * 4.0f + x);
    glDisable(GL_LIGHTING); glColor3f(r, g, b);
    glBegin(GL_TRIANGLES);
    glVertex3f(x, y, z); glVertex3f(x, y - len * 0.5f, z); glVertex3f(x + len + w, y - len * 0.25f, z + w * 0.3f);
    glEnd(); glEnable(GL_LIGHTING);
}

void drawCastle(float x, float z, float yaw) {
    float lv = 0.2f + 0.9f * dusk;
    const float roofB[3] = {0.3f, 0.5f, 0.95f}, roofP[3] = {0.95f, 0.35f, 0.6f};
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(yaw, 0, 1, 0);
    glColor3f(0.97f, 0.86f, 0.92f);
    glPushMatrix(); glTranslatef(0, 5.5f, -1.0f); box(9, 11, 7); glPopMatrix();                            // keep
    for (int i = 0; i < 7; i++) {                                                                         // battlements
        glPushMatrix(); glTranslatef(-4.0f + i * 1.33f, 11.5f, 2.3f);  box(0.8f, 1.0f, 0.8f); glPopMatrix();
        glPushMatrix(); glTranslatef(-4.0f + i * 1.33f, 11.5f, -4.3f); box(0.8f, 1.0f, 0.8f); glPopMatrix();
    }
    for (int sd = -1; sd <= 1; sd += 2) {                                                                 // curtain walls
        glPushMatrix(); glTranslatef(sd * 5.6f, 2.2f, 5.0f); box(5.2f, 4.4f, 1.2f); glPopMatrix();
        glPushMatrix(); glTranslatef(sd * 8.0f, 2.2f, 0.0f); box(1.2f, 4.4f, 9.8f); glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, 4.0f, 5.0f); box(4.0f, 1.6f, 1.2f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 2.2f, -4.5f); box(16.0f, 4.4f, 1.2f); glPopMatrix();
    glColor3f(0.25f, 0.12f, 0.1f); setEmission(0.8f * dusk, 0.5f * dusk, 0.2f * dusk);                    // glowing gate
    glPushMatrix(); glTranslatef(0, 1.6f, 4.6f); box(4.0f, 3.2f, 0.3f); glPopMatrix();
    clearEmission();
    glColor3f(0.6f, 0.4f, 0.25f); glPushMatrix(); glTranslatef(0, 0.1f, 6.6f); box(3.0f, 0.2f, 3.0f); glPopMatrix();   // drawbridge
    for (int k = 0; k < 4; k++) {                                                                         // corner towers
        float sx = (k % 2) ? 8.0f : -8.0f, sz = (k / 2) ? 5.0f : -4.5f;
        glPushMatrix(); glTranslatef(sx, 0, sz);
        glColor3f(0.97f, 0.86f, 0.92f); cylY(1.9f, 9.0f, 16);
        glTranslatef(0, 9.0f, 0); col3((k % 2) ? roofB : roofP); coneUp(2.7f, 4.5f);
        flagAt(0, 4.5f + 1.2f, 0, 1.4f, 1, 0.9f, 0.2f);
        for (int i = 0; i < 8; i++) { float a = i * 2 * PI_F / 8; bulb(2.0f * cosf(a), -0.1f, 2.0f * sinf(a), 0.1f, BULB_COL[i % 4], lv, false); }
        glPopMatrix();
    }
    glPushMatrix(); glTranslatef(0, 11.0f, -1.0f);                                                        // tall spire
    glColor3f(0.97f, 0.86f, 0.92f); cylY(2.0f, 6.0f, 16);
    glTranslatef(0, 6.0f, 0); col3(roofB); coneUp(2.8f, 5.5f);
    flagAt(0, 5.5f + 1.5f, 0, 1.8f, 1, 0.3f, 0.4f);
    glPopMatrix();
    for (int r = 0; r < 2; r++) for (int c = -1; c <= 1; c++) {                                          // windows
        float f = 0.7f + 0.3f * sinf(globalTime * 2.0f + c + r * 2.0f);
        glColor3f(0.4f, 0.55f, 0.9f); setEmission(1.0f * lv * f, 0.8f * lv * f, 0.3f * lv * f);
        glPushMatrix(); glTranslatef(c * 2.8f, 6.0f + r * 2.8f, 2.55f); box(0.9f, 1.4f, 0.1f); glPopMatrix();
        clearEmission();
    }
    for (int i = 0; i < 7; i++) bulb(-4.0f + i * 1.33f, 12.15f, 2.75f, 0.1f, BULB_COL[i % 4], lv * (0.6f + 0.4f * sinf(globalTime * 4 + i)), false);
    glPopMatrix();
}

void drawHauntedMansion(float x, float z, float yaw) {
    float lv = 0.25f + 0.9f * dusk;
    glPushMatrix();
    glTranslatef(x, 0, z);
    glRotatef(yaw, 0, 1, 0);
    glColor3f(0.34f, 0.30f, 0.40f);
    glPushMatrix(); glTranslatef(0, 3.5f, 0); box(12, 7, 8); glPopMatrix();
    glColor3f(0.16f, 0.12f, 0.20f);                                                                         // roof
    glBegin(GL_QUADS);
    glNormal3f(0, 0.6f, 0.8f);  glVertex3f(-6.6f, 6.8f, 4.6f);  glVertex3f(6.6f, 6.8f, 4.6f);  glVertex3f(6.6f, 11.0f, 0); glVertex3f(-6.6f, 11.0f, 0);
    glNormal3f(0, 0.6f, -0.8f); glVertex3f(6.6f, 6.8f, -4.6f);  glVertex3f(-6.6f, 6.8f, -4.6f); glVertex3f(-6.6f, 11.0f, 0); glVertex3f(6.6f, 11.0f, 0);
    glEnd();
    glColor3f(0.34f, 0.30f, 0.40f);
    glBegin(GL_TRIANGLES);
    glNormal3f(1, 0, 0);  glVertex3f(6.01f, 7, 4);   glVertex3f(6.01f, 7, -4);   glVertex3f(6.01f, 11, 0);
    glNormal3f(-1, 0, 0); glVertex3f(-6.01f, 7, -4); glVertex3f(-6.01f, 7, 4);   glVertex3f(-6.01f, 11, 0);
    glEnd();
    glPushMatrix(); glTranslatef(-5.5f, 0, -2.0f);                                                          // crooked tower
    glColor3f(0.30f, 0.27f, 0.36f); glPushMatrix(); glTranslatef(0, 6.5f, 0); box(3.4f, 13, 3.4f); glPopMatrix();
    glColor3f(0.16f, 0.12f, 0.20f); glTranslatef(0, 13.0f, 0); glRotatef(6.0f, 0, 0, 1); coneUp(2.8f, 5.0f);
    glPopMatrix();
    glColor3f(0.1f, 0.1f, 0.12f); glPushMatrix(); glTranslatef(0, 0.1f, 4.9f); box(3.2f, 0.2f, 1.2f); glPopMatrix();   // steps
    glColor3f(0.08f, 0.1f, 0.08f); setEmission(0.15f * lv, 0.7f * lv, 0.2f * lv);                          // glowing door
    glPushMatrix(); glTranslatef(0, 1.5f, 4.05f); box(1.8f, 3.0f, 0.2f); glPopMatrix();
    clearEmission();
    static const float wp[8][2] = {{-4, 5}, {-1.7f, 5}, {1.7f, 5}, {4, 5}, {-3.8f, 2}, {3.8f, 2}, {-5.5f, 9}, {-5.5f, 11.5f}};
    for (int k = 0; k < 8; k++) {                                                                          // flickering windows
        float f = 0.5f + 0.5f * sinf(globalTime * (2.5f + k * 0.7f) + k * 2.0f);
        float zz = (k < 6) ? 4.05f : -0.28f;
        glColor3f(0.1f, 0.1f, 0.1f); setEmission(lv * (0.15f + 0.6f * f), lv * (0.35f + 0.6f * f), lv * 0.15f);
        glPushMatrix(); glTranslatef(wp[k][0], wp[k][1], zz); box(0.9f, 1.3f, 0.12f); glPopMatrix();
        clearEmission();
    }
    glColor3f(0.08f, 0.08f, 0.1f); glPushMatrix(); glTranslatef(0, 3.7f, 4.05f); box(3.4f, 0.8f, 0.12f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 3.7f, 4.13f); signText("HAUNTED", 3.0f, 0.4f, 1.0f, 0.4f); glPopMatrix();
    for (int i = 0; i < 4; i++) {                                                                          // pumpkins
        float px = (i < 2 ? -1 : 1) * (2.3f + (i % 2) * 0.9f);
        glColor3f(1.0f, 0.5f, 0.1f); setEmission(0.7f * lv, 0.3f * lv, 0.0f);
        glPushMatrix(); glTranslatef(px, 0.4f, 5.6f + (i % 2) * 0.6f); glScalef(1.0f, 0.8f, 1.0f); glutSolidSphere(0.5, 10, 10); glPopMatrix();
        clearEmission();
    }
    glColor3f(0.08f, 0.08f, 0.1f);                                                                         // crooked fence
    for (int i = 0; i < 9; i++) { float fx = -6.0f + i * 1.5f; limb(fx, 0, 8.0f, fx + ((i * 7) % 5 - 2) * 0.12f, 1.8f, 8.0f, 0.07f, 6); }
    limb(-6.0f, 1.2f, 8.0f, 6.0f, 1.0f, 8.0f, 0.05f, 6);
    glColor3f(0.12f, 0.1f, 0.1f);                                                                          // bare tree
    glPushMatrix(); glTranslatef(-10.0f, 0, 5.0f); cylY(0.5f, 5.0f, 8);
    limb(0, 4.0f, 0, 2.0f, 6.5f, 0.3f, 0.18f, 6); limb(0, 4.5f, 0, -1.8f, 6.8f, -0.3f, 0.16f, 6); limb(0, 5.0f, 0, 0.3f, 7.8f, 0.5f, 0.14f, 6);
    glPopMatrix();
    for (int i = 0; i < 3; i++) {                                                                          // floating ghosts
        float a = globalTime * 0.6f + i * 2.1f;
        glPushMatrix();
        glTranslatef(9.5f * cosf(a), 4.5f + 1.2f * sinf(globalTime * 1.5f + i), 8.0f * sinf(a) + 1.0f);
        glRotatef(-a * DEG + 90.0f, 0, 1, 0);
        glColor3f(0.95f, 0.95f, 1.0f); setEmission(0.3f, 0.3f, 0.4f);
        glPushMatrix(); glRotatef(-90, 1, 0, 0); glutSolidCone(0.9, 2.0, 12, 1); glPopMatrix();
        ballAt(0, 2.1f, 0, 0.7f, 12);
        clearEmission();
        glColor3f(0.05f, 0.05f, 0.1f); ballAt(-0.25f, 2.2f, 0.6f, 0.1f, 6); ballAt(0.25f, 2.2f, 0.6f, 0.1f, 6); ballAt(0, 1.85f, 0.65f, 0.13f, 6);
        glPopMatrix();
    }
    { const float g[3] = {0.3f, 1.0f, 0.4f}; glPushMatrix(); glTranslatef(0, 1.5f, 5.0f); glowHere(2.2f, g, 0.35f * dusk); glPopMatrix(); }
    glPopMatrix();
}

static void slidePoint(int k, float u, float& px, float& py, float& pz) {
    float a = k * 2.0944f + u * 2.4f * PI_F, r = 2.8f + u * 6.2f;
    px = r * cosf(a); pz = r * sinf(a); py = 10.2f - 9.3f * u;
}
void drawWaterSlide(float x, float z) {
    float lv = 0.2f + 0.9f * dusk;
    static const float sc[3][3] = {{0.95f, 0.2f, 0.25f}, {1.0f, 0.85f, 0.15f}, {0.2f, 0.55f, 0.95f}};
    const float red[3] = {0.95f, 0.3f, 0.2f}, white[3] = {1, 1, 1};
    glPushMatrix();
    glTranslatef(x, 0, z);
    glColor3f(0.97f, 0.93f, 0.85f); cylY(12.0f, 0.5f, 40);                                                  // pool
    float sh = 0.5f + 0.5f * sinf(globalTime * 2.0f);
    glColor3f(0.15f + 0.1f * sh, 0.62f + 0.08f * sh, 0.95f);
    glPushMatrix(); glTranslatef(0, 0.05f, 0); cylY(11.5f, 0.5f, 40); glPopMatrix();
    glColor3f(0.9f, 0.9f, 0.95f);                                                                          // tower
    for (int a = -1; a <= 1; a += 2) for (int b = -1; b <= 1; b += 2) { limb(a * 1.6f, 0.3f, b * 1.6f, a * 1.6f, 12.5f, b * 1.6f, 0.18f); }
    glColor3f(0.95f, 0.5f, 0.2f); glPushMatrix(); glTranslatef(0, 10.5f, 0); box(4.8f, 0.35f, 4.8f); glPopMatrix();
    stripedCone(3.6f, 12.5f, 1.7f, 10, red, white);
    glColor3f(0.9f, 0.9f, 0.95f);
    for (int i = 0; i < 14; i++) {                                                                         // spiral stairs
        float ang = i * 0.5f;
        glPushMatrix(); glTranslatef(3.0f * cosf(ang), 0.5f + i * 0.7f, 3.0f * sinf(ang)); glRotatef(-ang * DEG, 0, 1, 0); box(1.3f, 0.14f, 0.8f); glPopMatrix();
    }
    for (int k = 0; k < 3; k++) {                                                                          // the three slides
        col3(sc[k]);
        for (int sg = 0; sg < 36; sg++) {
            float x1, y1, z1, x2, y2, z2;
            slidePoint(k, sg / 36.0f, x1, y1, z1); slidePoint(k, (sg + 1) / 36.0f, x2, y2, z2);
            limb(x1, y1, z1, x2, y2, z2, 0.55f, 8);
        }
        for (int j = 0; j < 2; j++) {                                                                      // riders sliding down
            float u = fmodf(globalTime * 0.12f + j * 0.5f + k * 0.17f, 1.0f), px, py, pz;
            slidePoint(k, u, px, py, pz);
            col3(sc[(k + 1) % 3]); ballAt(px, py + 0.75f, pz, 0.4f, 8);
            glColor3f(1.0f, 0.78f, 0.62f); ballAt(px, py + 1.2f, pz, 0.28f, 8);
        }
        float ex, ey, ez; slidePoint(k, 1.0f, ex, ey, ez);                                                 // splash
        glColor3f(1, 1, 1);
        for (int q = 0; q < 4; q++) ballAt(ex + 0.5f * cosf(q * 1.6f), 0.7f + 0.5f * fabsf(sinf(globalTime * 3.0f + q + k)), ez + 0.5f * sinf(q * 1.6f), 0.18f, 6);
    }
    for (int i = 0; i < 12; i++) { float a = i * 2 * PI_F / 12; bulb(2.9f * cosf(a), 10.9f, 2.9f * sinf(a), 0.1f, BULB_COL[i % 4], lv, false); }
    flagAt(0, 15.0f, 0, 1.6f, 1, 0.3f, 0.4f); glColor3f(0.3f, 0.3f, 0.35f); limb(0, 14.2f, 0, 0, 15.2f, 0, 0.05f, 6);
    glColor3f(0.3f, 0.3f, 0.35f);                                                                           // sign
    limb(-3.5f, 0, 13.2f, -3.5f, 3.2f, 13.2f, 0.1f, 6); limb(3.5f, 0, 13.2f, 3.5f, 3.2f, 13.2f, 0.1f, 6);
    glColor3f(0.1f, 0.4f, 0.8f); glPushMatrix(); glTranslatef(0, 2.6f, 13.2f); box(8.0f, 1.8f, 0.2f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 2.6f, 13.35f); signText("WATER SLIDES", 7.0f, 1.0f, 1.0f, 1.0f); glPopMatrix();
    for (int p = 0; p < 2; p++) {                                                                          // palm trees
        float px = p ? 9.5f : -9.5f, pz = 10.5f;
        glColor3f(0.55f, 0.38f, 0.2f); limb(px, 0, pz, px + 0.8f, 5.0f, pz, 0.22f, 8);
        glColor3f(0.2f, 0.65f, 0.25f);
        for (int f = 0; f < 6; f++) { float a = f * PI_F / 3; limb(px + 0.8f, 5.0f, pz, px + 0.8f + 2.4f * cosf(a), 4.2f, pz + 2.4f * sinf(a), 0.12f, 6); }
    }
    glPopMatrix();
}

void drawSwingRide(float x, float z) {
    float lv = 0.2f + 0.9f * dusk;
    const float red[3] = {0.9f, 0.2f, 0.3f}, yel[3] = {1.0f, 0.85f, 0.2f};
    glPushMatrix();
    glTranslatef(x, 0, z);
    glColor3f(0.35f, 0.55f, 0.9f); cylY(4.5f, 0.4f, 28);
    glColor3f(0.95f, 0.95f, 0.95f); cylY(0.7f, 10.5f, 16);
    glPushMatrix();
    glTranslatef(0, 10.5f, 0);
    glRotatef(globalTime * 45.0f, 0, 1, 0);
    glColor3f(1.0f, 0.85f, 0.2f); cylY(5.8f, 0.4f, 28);
    stripedCone(5.9f, 0.4f, 2.4f, 12, red, yel);
    for (int i = 0; i < 16; i++) { float a = i * 2 * PI_F / 16; bulb(5.9f * cosf(a), 0.2f, 5.9f * sinf(a), 0.13f, BULB_COL[i % 4], lv * (0.6f + 0.4f * sinf(globalTime * 6 + i)), false); }
    const float L = 6.0f, phi = 0.42f;
    for (int i = 0; i < 12; i++) {
        float a = i * 2 * PI_F / 12, cr = 5.0f + L * sinf(phi), cy = -L * cosf(phi);
        glColor3f(0.7f, 0.7f, 0.75f);
        limb(5.0f * cosf(a), 0.1f, 5.0f * sinf(a), cr * cosf(a), cy, cr * sinf(a), 0.03f, 5);
        glPushMatrix(); glTranslatef(cr * cosf(a), cy, cr * sinf(a)); glRotatef(-a * DEG, 0, 1, 0);
        col3(PAL[i % 8]); glPushMatrix(); glTranslatef(0, -0.1f, 0); box(0.8f, 0.15f, 0.8f); glPopMatrix();
        glPushMatrix(); glTranslatef(0, 0.3f, -0.35f); box(0.8f, 0.7f, 0.1f); glPopMatrix();
        glColor3f(1.0f, 0.78f, 0.62f); ballAt(0, 0.7f, 0, 0.25f, 8);
        col3(PAL[(i + 3) % 8]); glPushMatrix(); glTranslatef(0, 0.3f, 0); box(0.45f, 0.5f, 0.25f); glPopMatrix();
        glPopMatrix();
    }
    flagAt(0, 4.6f, 0, 1.6f, 1, 0.3f, 0.4f); glColor3f(0.3f, 0.3f, 0.35f); limb(0, 2.4f, 0, 0, 4.7f, 0, 0.05f, 6);
    glPopMatrix();
    glPopMatrix();
}

// ---- animals for the entrance gate (face +z) ----
static void drawElephant() {
    glColor3f(0.62f, 0.64f, 0.72f);
    glPushMatrix(); glTranslatef(0, 2.2f, 0); glScalef(1.2f, 1.0f, 1.7f); glutSolidSphere(1.0, 16, 14); glPopMatrix();
    ballAt(0, 2.7f, 1.9f, 0.95f, 14);
    for (int s = -1; s <= 1; s += 2) {
        glColor3f(0.78f, 0.62f, 0.68f);
        glPushMatrix(); glTranslatef(s * 1.05f, 2.8f, 1.6f); glRotatef(s * 25.0f, 0, 1, 0); glScalef(0.15f, 0.9f, 0.8f); glutSolidSphere(1.0, 12, 10); glPopMatrix();
        glColor3f(0.62f, 0.64f, 0.72f);
        limb(s * 0.7f, 1.5f, s * 0 + 1.0f, s * 0.7f, 0, 1.0f, 0.42f, 10); limb(s * 0.7f, 1.5f, -1.0f, s * 0.7f, 0, -1.0f, 0.42f, 10);
        glColor3f(0.05f, 0.05f, 0.05f); ballAt(s * 0.45f, 2.95f, 2.7f, 0.1f, 6);
        glColor3f(1.0f, 1.0f, 0.9f); limb(s * 0.4f, 2.0f, 2.7f, s * 0.55f, 1.7f, 3.5f, 0.1f, 6);
    }
    glColor3f(0.62f, 0.64f, 0.72f);
    limb(0, 2.4f, 2.7f, 0, 1.5f, 3.3f, 0.3f, 8); limb(0, 1.5f, 3.3f, 0, 0.8f, 3.1f, 0.22f, 8);
    limb(0, 2.5f, -1.7f, 0, 1.5f, -2.0f, 0.08f, 6);
}
static void drawLion() {
    glColor3f(0.95f, 0.65f, 0.25f);
    glPushMatrix(); glTranslatef(0, 1.7f, 0); glScalef(0.65f, 0.6f, 1.3f); glutSolidSphere(1.0, 14, 12); glPopMatrix();
    for (int s = -1; s <= 1; s += 2) { limb(s * 0.4f, 1.3f, 0.9f, s * 0.4f, 0, 0.9f, 0.2f, 8); limb(s * 0.4f, 1.3f, -0.9f, s * 0.4f, 0, -0.9f, 0.2f, 8); }
    glColor3f(0.55f, 0.32f, 0.12f); ballAt(0, 2.5f, 1.5f, 0.95f, 14);
    glColor3f(0.98f, 0.7f, 0.3f);  ballAt(0, 2.45f, 1.95f, 0.58f, 12);
    glColor3f(0.2f, 0.1f, 0.1f);   ballAt(0, 2.35f, 2.5f, 0.12f, 6); ballAt(-0.25f, 2.65f, 2.45f, 0.07f, 6); ballAt(0.25f, 2.65f, 2.45f, 0.07f, 6);
    glColor3f(0.95f, 0.65f, 0.25f); limb(0, 1.8f, -1.3f, 0, 1.0f, -2.3f, 0.08f, 6);
    glColor3f(0.55f, 0.32f, 0.12f); ballAt(0, 0.9f, -2.4f, 0.2f, 8);
}
static void drawGiraffe() {
    glColor3f(1.0f, 0.8f, 0.2f);
    glPushMatrix(); glTranslatef(0, 3.0f, 0); glScalef(0.65f, 0.7f, 1.2f); glutSolidSphere(1.0, 14, 12); glPopMatrix();
    for (int s = -1; s <= 1; s += 2) { limb(s * 0.4f, 2.6f, 0.7f, s * 0.4f, 0, 0.7f, 0.14f, 6); limb(s * 0.4f, 2.6f, -0.7f, s * 0.4f, 0, -0.7f, 0.14f, 6); }
    limb(0, 3.4f, 0.9f, 0, 6.2f, 1.5f, 0.3f, 8);
    glPushMatrix(); glTranslatef(0, 6.5f, 1.9f); glScalef(0.3f, 0.35f, 0.6f); glutSolidSphere(1.0, 12, 10); glPopMatrix();
    glColor3f(0.5f, 0.3f, 0.1f);
    limb(-0.15f, 6.8f, 1.6f, -0.15f, 7.3f, 1.6f, 0.05f, 6); limb(0.15f, 6.8f, 1.6f, 0.15f, 7.3f, 1.6f, 0.05f, 6);
    ballAt(-0.15f, 7.35f, 1.6f, 0.09f, 6); ballAt(0.15f, 7.35f, 1.6f, 0.09f, 6);
    for (int i = 0; i < 6; i++) { glPushMatrix(); glTranslatef((i % 2 ? 0.62f : -0.62f), 3.0f + (i % 3) * 0.3f - 0.2f, -0.6f + (i / 2) * 0.55f); glScalef(0.3f, 1.0f, 1.0f); glutSolidSphere(0.22, 8, 8); glPopMatrix(); }
    glColor3f(0.05f, 0.05f, 0.05f); ballAt(-0.2f, 6.6f, 2.3f, 0.06f, 6); ballAt(0.2f, 6.6f, 2.3f, 0.06f, 6);
}

// ---- framed "photos" of rides (drawn flat, unlit) ----
static void disc2(float cx, float cy, float r, int n) {
    glBegin(GL_TRIANGLE_FAN); glVertex3f(cx, cy, 0);
    for (int i = 0; i <= n; i++) { float a = i * 2 * PI_F / n; glVertex3f(cx + r * cosf(a), cy + r * sinf(a), 0); }
    glEnd();
}
static void photoPanel(int kind) {
    glDisable(GL_LIGHTING);
    glColor3f(0.95f, 0.75f, 0.2f); box(4.9f, 6.3f, 0.12f);
    glPushMatrix(); glTranslatef(0, 0, 0.07f);
    glBegin(GL_QUADS);
    glColor3f(0.55f, 0.82f, 1.0f); glVertex3f(-2.3f, -2.9f, 0); glVertex3f(2.3f, -2.9f, 0);
    glColor3f(0.2f, 0.5f, 0.95f);  glVertex3f(2.3f, 2.9f, 0);   glVertex3f(-2.3f, 2.9f, 0);
    glEnd();
    glTranslatef(0, 0, 0.05f);
    glColor3f(1.0f, 0.9f, 0.3f); disc2(1.5f, 2.2f, 0.5f, 16);
    glColor3f(0.3f, 0.75f, 0.35f);
    glBegin(GL_QUADS); glVertex3f(-2.3f, -2.9f, 0); glVertex3f(2.3f, -2.9f, 0); glVertex3f(2.3f, -2.1f, 0); glVertex3f(-2.3f, -2.1f, 0); glEnd();
    glTranslatef(0, 0, 0.05f);
    glLineWidth(4.0f);
    if (kind == 0) {                                                       // Ferris wheel
        glColor3f(1.0f, 0.95f, 0.4f);
        glBegin(GL_LINE_LOOP); for (int i = 0; i < 28; i++) { float a = i * 2 * PI_F / 28; glVertex3f(1.6f * cosf(a), 0.2f + 1.6f * sinf(a), 0); } glEnd();
        glBegin(GL_LINES); for (int i = 0; i < 8; i++) { float a = i * PI_F / 4; glVertex3f(0, 0.2f, 0); glVertex3f(1.6f * cosf(a), 0.2f + 1.6f * sinf(a), 0); } glEnd();
        for (int i = 0; i < 8; i++) { float a = i * PI_F / 4; col3(PAL[i]); disc2(1.6f * cosf(a), 0.2f + 1.6f * sinf(a), 0.23f, 10); }
        glColor3f(0.9f, 0.3f, 0.3f); glBegin(GL_LINES); glVertex3f(0, 0.2f, 0); glVertex3f(-1.1f, -2.1f, 0); glVertex3f(0, 0.2f, 0); glVertex3f(1.1f, -2.1f, 0); glEnd();
    } else if (kind == 1) {                                                // roller coaster with loop
        glColor3f(0.95f, 0.2f, 0.25f);
        glBegin(GL_LINE_STRIP); for (int i = 0; i <= 20; i++) { float t = i / 20.0f; glVertex3f(-2.1f + 4.2f * t, -1.2f + 0.8f * sinf(t * 2 * PI_F) + 0.5f * t, 0); } glEnd();
        glBegin(GL_LINE_LOOP); for (int i = 0; i < 24; i++) { float a = i * 2 * PI_F / 24; glVertex3f(0.4f + 0.85f * cosf(a), 0.4f + 0.85f * sinf(a), 0); } glEnd();
        glColor3f(0.4f, 0.3f, 0.5f); glBegin(GL_LINES); for (int i = 0; i < 6; i++) { float x = -1.9f + i * 0.75f; glVertex3f(x, -2.1f, 0); glVertex3f(x, -1.2f + 0.8f * sinf((x + 2.1f) / 4.2f * 2 * PI_F) + 0.5f * (x + 2.1f) / 4.2f, 0); } glEnd();
        glColor3f(1.0f, 0.85f, 0.1f); glBegin(GL_QUADS); glVertex3f(-1.7f, -1.2f, 0); glVertex3f(-1.2f, -1.2f, 0); glVertex3f(-1.2f, -0.8f, 0); glVertex3f(-1.7f, -0.8f, 0); glEnd();
    } else if (kind == 2) {                                                // balloons
        glColor3f(0.9f, 0.9f, 0.9f);
        glBegin(GL_LINES); for (int i = 0; i < 5; i++) { glVertex3f(0, -2.1f, 0); glVertex3f(-1.3f + i * 0.65f, 0.3f + (i % 2) * 0.7f, 0); } glEnd();
        for (int i = 0; i < 5; i++) { col3(PAL[i]); disc2(-1.3f + i * 0.65f, 0.7f + (i % 2) * 0.7f, 0.5f, 14); }
    } else {                                                               // castle
        glColor3f(0.97f, 0.7f, 0.85f);
        glBegin(GL_QUADS);
        glVertex3f(-1.3f, -2.1f, 0); glVertex3f(1.3f, -2.1f, 0); glVertex3f(1.3f, 0.4f, 0); glVertex3f(-1.3f, 0.4f, 0);
        glVertex3f(-2.0f, -2.1f, 0); glVertex3f(-1.2f, -2.1f, 0); glVertex3f(-1.2f, 1.2f, 0); glVertex3f(-2.0f, 1.2f, 0);
        glVertex3f(1.2f, -2.1f, 0); glVertex3f(2.0f, -2.1f, 0); glVertex3f(2.0f, 1.2f, 0); glVertex3f(1.2f, 1.2f, 0);
        glEnd();
        glColor3f(0.3f, 0.5f, 0.95f);
        glBegin(GL_TRIANGLES); glVertex3f(-2.1f, 1.2f, 0); glVertex3f(-1.1f, 1.2f, 0); glVertex3f(-1.6f, 2.2f, 0); glVertex3f(1.1f, 1.2f, 0); glVertex3f(2.1f, 1.2f, 0); glVertex3f(1.6f, 2.2f, 0); glVertex3f(-0.8f, 0.4f, 0); glVertex3f(0.8f, 0.4f, 0); glVertex3f(0, 1.5f, 0); glEnd();
        glColor3f(0.3f, 0.15f, 0.15f); glBegin(GL_QUADS); glVertex3f(-0.35f, -2.1f, 0); glVertex3f(0.35f, -2.1f, 0); glVertex3f(0.35f, -1.0f, 0); glVertex3f(-0.35f, -1.0f, 0); glEnd();
    }
    glLineWidth(1.0f);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void drawParkGate() {
    float vis = gateVis();
    if (vis < 0.005f) return;                              // zoomed in: the gate is gone, you are 'inside' the park
    float lv = 0.25f + 0.9f * dusk;
    glPushMatrix();
    glTranslatef(0, -(1.0f - vis) * 40.0f, 34.0f);
    for (int sx = -1; sx <= 1; sx += 2) {                                   // striped pillars with domes
        for (int i = 0; i < 6; i++) { col3(PAL[(i * 3 + (sx > 0 ? 1 : 0)) % 8]); glPushMatrix(); glTranslatef(sx * 10.5f, 1.0f + i * 2.0f, 0); box(3.4f, 2.0f, 3.4f); glPopMatrix(); }
        glColor3f(0.95f, 0.8f, 0.2f); ballAt(sx * 10.5f, 12.9f, 0, 2.3f, 16);
        glColor3f(0.9f, 0.2f, 0.3f); glPushMatrix(); glTranslatef(sx * 10.5f, 14.8f, 0); coneUp(1.2f, 3.0f); glPopMatrix();
        flagAt(sx * 10.5f, 17.6f, 0, 1.6f, 1.0f, 0.9f, 0.2f); glColor3f(0.3f, 0.3f, 0.35f); limb(sx * 10.5f, 17.2f, 0, sx * 10.5f, 17.9f, 0, 0.05f, 6);
        glColor3f(0.98f, 0.9f, 0.85f);                                          // wings with photo panels
        glPushMatrix(); glTranslatef(sx * 17.0f, 4.0f, 0); box(6.2f, 8.0f, 1.8f); glPopMatrix();
        glPushMatrix(); glTranslatef(sx * 13.0f, 3.0f, 0); box(3.4f, 6.0f, 1.6f); glPopMatrix();
        glColor3f(0.95f, 0.45f, 0.7f); glPushMatrix(); glTranslatef(sx * 17.0f, 8.3f, 0); box(6.8f, 0.6f, 2.2f); glPopMatrix();
        glPushMatrix(); glTranslatef(sx * 17.0f, 4.2f, 0.95f); photoPanel(sx < 0 ? 0 : 1); glPopMatrix();
        glPushMatrix(); glTranslatef(sx * 17.0f, 4.2f, -0.95f); glRotatef(180, 0, 1, 0); photoPanel(sx < 0 ? 2 : 3); glPopMatrix();
    }
    glColor3f(0.95f, 0.8f, 0.2f); glPushMatrix(); glTranslatef(0, 10.6f, 0); box(21.0f, 2.2f, 3.2f); glPopMatrix();    // beam
    glPushMatrix(); glTranslatef(0, 16.0f, 0); box(19.6f, 7.4f, 1.0f); glPopMatrix();                                  // sign frame
    glColor3f(0.15f, 0.2f, 0.55f); glPushMatrix(); glTranslatef(0, 16.0f, 0); box(18.6f, 6.4f, 1.3f); glPopMatrix();
    glColor3f(0.95f, 0.8f, 0.2f); glPushMatrix(); glTranslatef(0, 20.0f, 0); box(19.0f, 0.5f, 1.8f); glPopMatrix();    // animal pedestal
    for (int face = 0; face < 2; face++) {                                   // lettering + chasing lights on both faces
        glPushMatrix();
        if (face) glRotatef(180, 0, 1, 0);
        glPushMatrix(); glTranslatef(0, 16.9f, 0.68f); signText("FUN LAND", 12.5f, 1.0f, 0.9f, 0.3f); glPopMatrix();
        glPushMatrix(); glTranslatef(0, 14.4f, 0.68f); signText("AMUSEMENT PARK", 10.5f, 1.0f, 0.5f, 0.8f); glPopMatrix();
        for (int i = 0; i < 19; i++) {
            float ch = 0.4f + 0.6f * (sinf(globalTime * 6.0f - i * 0.5f) > 0 ? 1.0f : 0.3f);
            bulb(-9.0f + i, 19.0f, 0.7f, 0.12f, BULB_COL[i % 4], lv * ch, false);
            bulb(-9.0f + i, 13.0f, 0.7f, 0.12f, BULB_COL[(i + 2) % 4], lv * ch, false);
        }
        for (int i = 0; i < 5; i++) { bulb(-9.2f, 13.5f + i, 0.7f, 0.12f, BULB_COL[i % 4], lv, false); bulb(9.2f, 13.5f + i, 0.7f, 0.12f, BULB_COL[(i + 1) % 4], lv, false); }
        glPopMatrix();
    }
    glPushMatrix(); glTranslatef(-6.5f, 20.25f, 0); glScalef(1.5f, 1.5f, 1.5f); drawElephant(); glPopMatrix();      // animals on top
    glPushMatrix(); glTranslatef(0.0f, 20.25f, 0);  glScalef(1.6f, 1.6f, 1.6f); drawLion(); glPopMatrix();
    glPushMatrix(); glTranslatef(6.5f, 20.25f, 0);  glScalef(1.15f, 1.15f, 1.15f); drawGiraffe(); glPopMatrix();
    glPopMatrix();
}

// ------------------------- camera --------------------------------------
static void zoomBy(float f) { camTgt.d *= f; if (camTgt.d < 6.0f) camTgt.d = 6.0f; if (camTgt.d > 110.0f) camTgt.d = 110.0f; }
static void updateCamera() {
    const float k = 0.09f;
    camCur.tx += (camTgt.tx - camCur.tx) * k; camCur.ty += (camTgt.ty - camCur.ty) * k; camCur.tz += (camTgt.tz - camCur.tz) * k;
    camCur.d += (camTgt.d - camCur.d) * k; camCur.yaw += (camTgt.yaw - camCur.yaw) * k; camCur.pitch += (camTgt.pitch - camCur.pitch) * k;
    float yw = camCur.yaw / DEG, pt = camCur.pitch / DEG;
    camX = camCur.tx + camCur.d * cosf(pt) * sinf(yw);
    camY = camCur.ty + camCur.d * sinf(pt);
    camZ = camCur.tz + camCur.d * cosf(pt) * cosf(yw);
    if (camZ < -60.0f) {                                   // don't let the camera go behind the city skyline
        float f = (-60.0f - camCur.tz) / (camZ - camCur.tz);
        camX = camCur.tx + (camX - camCur.tx) * f; camY = camCur.ty + (camY - camCur.ty) * f; camZ = -60.0f;
    }
    lookX = camCur.tx; lookY = camCur.ty; lookZ = camCur.tz;
}

// ------------------------- HUD -----------------------------------------
void drawHUD() {
    const char* modeName = mode == 0 ? "DAY" : (mode == 1 ? "SUNSET" : "NIGHT");
    char l1[160];
    sprintf(l1, "Mode: %s     D = day   S = sunset   N = night   M = next mode", modeName);
    const char* l2 = "Zoom: mouse wheel or + / -     Rotate: drag mouse or arrow keys";
    const char* l3 = "Views: 1 Park   2 Merry-go-round   3 Ferris wheel   4 Coaster   5 Tent   6 Ice-cream truck     SPACE pause   ESC quit";
    const char* l4 = "P = music on/off     SPACE = pause     L = far lamps on/off     ESC = quit";
    const char* lines[4] = {l1, l2, l3, l4};
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    float c = lerpf(0.05f, 1.0f, dusk);
    glColor3f(c, c, c);
    for (int i = 0; i < 4; i++) {
        glRasterPos2i(14, winH - 24 - i * 18);
        for (const char* p = lines[i]; *p; ++p) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *p);
    }
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
}

// ------------------------- display -------------------------------------
void setupLights() {
    const float ad[4] = {0.42f, 0.44f, 0.48f, 1}, as[4] = {0.55f, 0.36f, 0.34f, 1}, an[4] = {0.17f, 0.20f, 0.36f, 1};
    const float dd[4] = {0.78f, 0.75f, 0.68f, 1}, ds[4] = {0.95f, 0.55f, 0.32f, 1}, dn[4] = {0.22f, 0.28f, 0.50f, 1};
    GLfloat amb[4], dif[4];
    for (int i = 0; i < 4; i++) {
        amb[i] = lerpf(lerpf(ad[i], as[i], dusk), an[i], night);
        dif[i] = lerpf(lerpf(dd[i], ds[i], dusk), dn[i], night);
    }
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
    drawMoon();
    drawCity();
    setupLights();
    drawClouds();
    drawBirds();
    drawHotAirBalloon();
    drawGround();
    drawFence();
    drawTrees();
    drawLamps();
    drawFlowers();
    drawRollerCoaster();
    drawFerrisWheel();
    drawCircusTent();
    drawCarousel();
    drawIceCreamTruck();
    drawStallsAndSeating();
    drawParkGate();
    drawCastle(-55.0f, -22.0f, 25.0f);
    drawHauntedMansion(55.0f, -22.0f, -25.0f);
    drawWaterSlide(64.0f, 12.0f);
    drawSwingRide(-58.0f, 10.0f);
    drawClown();
    drawJoker();
    for (int i = 0; i < NRUN; i++) drawKid(runners[i]);
    drawRunningGirl();
    drawFloatingBalloons();
    drawFireworks();
    drawHUD();
    glutSwapBuffers();
}

// ------------------------- animation timer -----------------------------
void update(int) {
    // mode transitions (keep running even when paused)
    duskRaw  += ((mode >= 1) ? 1.0f : -1.0f) * 0.003f;  duskRaw  = clamp01(duskRaw);
    nightRaw += ((mode == 2) ? 1.0f : -1.0f) * 0.003f;  nightRaw = clamp01(nightRaw);
    dusk  = duskRaw * duskRaw * (3.0f - 2.0f * duskRaw);
    night = nightRaw * nightRaw * (3.0f - 2.0f * nightRaw);
    updateCamera();

    if (!isPaused) {
        globalTime += 0.016f;
        ferrisAngle += 0.45f; if (ferrisAngle >= 360.0f) ferrisAngle -= 360.0f;
        carouselAngle += 0.6f; if (carouselAngle >= 360.0f) carouselAngle -= 360.0f;
        coasterS += 0.30f; if (coasterS >= trackLen) coasterS -= trackLen;
        sunPulse += 0.03f;
        limbSwing += 0.06f;
        updateRunners();
        updateFireworks();

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
    gluPerspective(52.0, (double)w / (double)h, 0.5, 400.0);
    glMatrixMode(GL_MODELVIEW);
}

// ------------------------- input ---------------------------------------
int lastMX = 0, lastMY = 0; bool dragging = false;

void keyboard(unsigned char key, int, int) {
    switch (key) {
        case 'd': case 'D': mode = 0; break;
        case 's': case 'S': mode = 1; break;
        case 'n': case 'N': mode = 2; break;
        case 'm': case 'M': mode = (mode + 1) % 3; break;
        case 'l': case 'L': showLamps = !showLamps; break;
        case 'p': case 'P': setMusic(!musicOn); break;
        case ' ': isPaused = !isPaused; break;
        case '+': case '=': zoomBy(0.85f); break;
        case '-': case '_': zoomBy(1.18f); break;
        case '1': case '2': case '3': case '4': case '5': case '6': camTgt = VIEWS[key - '1']; break;
        case '0': camTgt = VIEWS[0]; break;
        case 27: exit(0); break;
    }
}
void specialKeys(int key, int, int) {
    switch (key) {
        case GLUT_KEY_LEFT:  camTgt.yaw -= 6.0f; break;
        case GLUT_KEY_RIGHT: camTgt.yaw += 6.0f; break;
        case GLUT_KEY_UP:    camTgt.pitch += 3.0f; break;
        case GLUT_KEY_DOWN:  camTgt.pitch -= 3.0f; break;
    }
    if (camTgt.yaw > 175.0f) camTgt.yaw = 175.0f;
    if (camTgt.yaw < -175.0f) camTgt.yaw = -175.0f;
    if (camTgt.pitch > 75.0f) camTgt.pitch = 75.0f;
    if (camTgt.pitch < 1.0f) camTgt.pitch = 1.0f;
}
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) { dragging = (state == GLUT_DOWN); lastMX = x; lastMY = y; }
}
void motion(int x, int y) {
    if (!dragging) return;
    camTgt.yaw   -= (x - lastMX) * 0.3f;
    camTgt.pitch += (y - lastMY) * 0.3f;
    if (camTgt.yaw > 175.0f) camTgt.yaw = 175.0f;
    if (camTgt.yaw < -175.0f) camTgt.yaw = -175.0f;
    if (camTgt.pitch > 75.0f) camTgt.pitch = 75.0f;
    if (camTgt.pitch < 1.0f) camTgt.pitch = 1.0f;
    lastMX = x; lastMY = y;
}
void mouseWheel(int, int dir, int, int) { zoomBy(dir > 0 ? 0.88f : 1.14f); }

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 800);
    glutCreateWindow("3D Amusement Park - Day / Sunset / Night");
    initScene();
    updateCamera();
    buildMusic();
    setMusic(true);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(mouseWheel);
    glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}
