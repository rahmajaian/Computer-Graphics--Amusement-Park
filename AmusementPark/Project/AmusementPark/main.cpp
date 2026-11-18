#include <GL/freeglut.h>
#include <cmath>
#include <vector>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// GLOBAL STATE AND ANIMATION VARIABLES
// ============================================================
bool isSunset = false;
bool isPaused = false;
float globalTime = 0.0f;

float camX = 0.0f, camY = 19.0f, camZ = 48.0f;

float ferrisAngle = 0.0f;
float coasterProgress = 0.0f;
float hotAirY = 18.0f, hotAirX = 22.0f;
float sunPulse = 0.0f;
float birdWingAngle = 0.0f;

// Animated ice-cream truck
float iceCreamTruckX = -38.0f;
float truckWheelAngle = 0.0f;

struct Balloon {
    float x, y, z, speedY, speedX, r, g, b;
};
std::vector<Balloon> floatingBalloons;

struct Cloud {
    float x, y, z, speed, scale;
};
std::vector<Cloud> clouds;

struct Bird {
    float x, y, z, speed;
};
std::vector<Bird> birds;

// ============================================================
// ROLLER COASTER TRACK
// ============================================================
void getCoasterPoint(float t, float &x, float &y, float &z) {
    float angle = t;
    float radiusX = 24.0f;
    float radiusZ = 12.0f;

    x = 4.0f + radiusX * cos(angle);
    z = -16.0f + radiusZ * sin(angle);
    y = 4.5f + 2.5f * sin(2.0f * angle);

    float loopCenter = 1.15f;
    float dist = angle - loopCenter;

    if (dist > -0.6f && dist < 0.6f) {
        float localTheta = ((dist + 0.6f) / 1.2f) * 2.0f * M_PI;
        float loopR = 6.5f;
        x += loopR * sin(localTheta) * 0.7f;
        y += loopR * (1.0f - cos(localTheta));
    }
}

// ============================================================
// SCENE INITIALIZATION
// ============================================================
void initScene() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);

    GLfloat globalAmbient[] = {0.35f, 0.35f, 0.35f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // Floating balloons
    float colors[5][3] = {
        {1.0f, 0.25f, 0.3f}, {0.2f, 0.8f, 0.95f},
        {1.0f, 0.82f, 0.15f}, {0.95f, 0.45f, 0.75f},
        {0.6f, 0.35f, 0.95f}
    };

    for (int i = 0; i < 15; i++) {
        Balloon b;
        b.x = -28.0f + (rand() % 56);
        b.y = 3.0f + (rand() % 18);
        b.z = -18.0f + (rand() % 34);
        b.speedY = 0.018f + (rand() % 10) * 0.0025f;
        b.speedX = ((rand() % 10) - 5) * 0.003f;
        b.r = colors[i % 5][0];
        b.g = colors[i % 5][1];
        b.b = colors[i % 5][2];
        floatingBalloons.push_back(b);
    }

    // Clouds
    for (int i = 0; i < 7; i++) {
        Cloud c;
        c.x = -45.0f + i * 15.0f;
        c.y = 26.0f + (i % 3) * 2.2f;
        c.z = -28.0f - (i % 2) * 8.0f;
        c.speed = 0.025f + (i % 3) * 0.008f;
        c.scale = 1.1f + (i % 2) * 0.45f;
        clouds.push_back(c);
    }

    // Birds
    for (int i = 0; i < 6; i++) {
        Bird bd;
        bd.x = -34.0f + i * 12.0f;
        bd.y = 20.0f + (i % 3) * 2.0f;
        bd.z = -10.0f + (i % 4) * 5.0f;
        bd.speed = 0.07f + (i % 2) * 0.025f;
        birds.push_back(bd);
    }
}

// ============================================================
// SKY DOME
// ============================================================
void drawSkyDome() {
    glPushMatrix();
    glTranslatef(camX, camY, camZ);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);

    if (!isSunset)
        glColor3f(0.52f, 0.78f, 0.96f);
    else
        glColor3f(0.92f, 0.48f, 0.28f);

    glutSolidSphere(180.0, 30, 30);

    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glPopMatrix();
}

// ============================================================
// GROUND, CITY SKYLINE, WINDOWS AND STREET LAMPS
// ============================================================
void drawGroundAndCity() {
    glColor3f(isSunset ? 0.08f : 0.16f,
              isSunset ? 0.18f : 0.52f,
              isSunset ? 0.04f : 0.18f);

    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-150, 0, -150);
    glVertex3f(150, 0, -150);
    glVertex3f(150, 0, 150);
    glVertex3f(-150, 0, 150);
    glEnd();

    // Plaza
    glColor3f(isSunset ? 0.28f : 0.58f,
              isSunset ? 0.24f : 0.52f,
              isSunset ? 0.22f : 0.48f);

    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-34, 0.03f, -14);
    glVertex3f(34, 0.03f, -14);
    glVertex3f(34, 0.03f, 26);
    glVertex3f(-34, 0.03f, 26);
    glEnd();

    float bldgWidths[] = {6.0f, 4.5f, 7.0f, 5.0f, 8.0f, 4.0f,
                         6.5f, 5.5f, 7.5f, 4.2f, 6.0f};
    float bldgHeights[] = {18.0f, 26.0f, 14.0f, 32.0f, 22.0f, 28.0f,
                          16.0f, 35.0f, 20.0f, 24.0f, 19.0f};

    float currentX = -65.0f;

    for (int i = 0; i < 11; i++) {
        float w = bldgWidths[i];
        float h = bldgHeights[i];

        if (!isSunset)
            glColor3f(0.35f + (i % 3) * 0.08f,
                      0.42f + (i % 2) * 0.06f,
                      0.52f + (i % 4) * 0.05f);
        else
            glColor3f(0.18f + (i % 3) * 0.03f,
                      0.12f + (i % 2) * 0.02f,
                      0.16f + (i % 4) * 0.03f);

        glPushMatrix();
        glTranslatef(currentX + w / 2.0f, h / 2.0f, -60.0f);
        glScalef(w, h, 5.0f);
        glutSolidCube(1.0f);
        glPopMatrix();

        if (isSunset) {
            GLfloat winEmit[] = {0.95f, 0.75f, 0.3f, 1.0f};
            glMaterialfv(GL_FRONT, GL_EMISSION, winEmit);
            glColor3f(1.0f, 0.85f, 0.4f);
        } else {
            glColor3f(0.65f, 0.75f, 0.85f);
        }

        for (float wy = 3.0f; wy < h - 2.0f; wy += 3.0f) {
            for (float wx = currentX + 1.0f; wx < currentX + w - 1.0f; wx += 1.8f) {
                glPushMatrix();
                glTranslatef(wx, wy, -57.4f);
                glScalef(0.8f, 1.2f, 0.1f);
                glutSolidCube(1.0f);
                glPopMatrix();
            }
        }

        GLfloat noEmit[] = {0, 0, 0, 1};
        glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);
        currentX += w + 1.2f;
    }

    // Street lamps
    float lampCoords[][2] = {
        {-22, 12}, {-12, 16}, {14, 12}, {22, 4}, {-6, -12}, {16, -12}
    };

    for (int i = 0; i < 6; i++) {
        glPushMatrix();
        glTranslatef(lampCoords[i][0], 0, lampCoords[i][1]);

        glColor3f(0.12f, 0.15f, 0.18f);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        glutSolidCylinder(0.12, 3.8, 10, 2);
        glPopMatrix();

        float pulse = 0.7f + 0.3f * sin(globalTime * 4.0f + i);
        float intensity = isSunset ? 1.4f : 0.5f;
        GLfloat emit[] = {
            intensity * pulse,
            0.85f * intensity * pulse,
            0.4f * intensity * pulse,
            1.0f
        };
        glMaterialfv(GL_FRONT, GL_EMISSION, emit);
        glColor3f(1.0f, 0.95f, 0.5f);
        glTranslatef(0, 4.0f, 0);
        glutSolidSphere(0.4f, 14, 14);

        GLfloat noEmit[] = {0, 0, 0, 1};
        glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);
        glPopMatrix();
    }
}

// ============================================================
// SUN WITH PULSING BRIGHTNESS
// ============================================================
void drawCelestialBody() {
    glPushMatrix();
    glTranslatef(4.0f, 32.0f, -50.0f);

    float brightness = 0.75f + 0.25f * sin(sunPulse);

    if (!isSunset) {
        GLfloat sunEmit[] = {
            brightness, 0.90f * brightness, 0.35f * brightness, 1.0f
        };
        glMaterialfv(GL_FRONT, GL_EMISSION, sunEmit);
        glColor3f(1.0f, 0.95f * brightness, 0.4f);
        glutSolidSphere(3.4f, 24, 24);
    } else {
        GLfloat sunsetEmit[] = {1.2f, 0.55f, 0.15f, 1.0f};
        glMaterialfv(GL_FRONT, GL_EMISSION, sunsetEmit);
        glColor3f(1.0f, 0.48f, 0.12f);
        glutSolidSphere(3.8f, 24, 24);
    }

    GLfloat noEmit[] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);
    glPopMatrix();
}

// ============================================================
// CLOUDS
// ============================================================
void drawClouds() {
    glColor3f(0.98f,
              isSunset ? 0.65f : 0.95f,
              isSunset ? 0.52f : 0.92f);

    for (const auto& c : clouds) {
        glPushMatrix();
        glTranslatef(c.x, c.y, c.z);
        glScalef(c.scale, c.scale * 0.65f, c.scale * 0.7f);
        glutSolidSphere(2.2, 16, 16);
        glTranslatef(1.9f, -0.2f, 0.2f);
        glutSolidSphere(1.7, 16, 16);
        glTranslatef(-3.7f, -0.1f, -0.1f);
        glutSolidSphere(1.6, 16, 16);
        glPopMatrix();
    }
}

// ============================================================
// FERRIS WHEEL
// ============================================================
void drawFerrisWheel() {
    glPushMatrix();
    glTranslatef(-20.0f, 0.0f, -6.0f);
    glRotatef(20.0f, 0, 1, 0);

    float hubY = 13.5f;
    float radius = 9.5f;

    // Support legs
    glColor3f(0.12f, 0.55f, 0.62f);
    for (int zSide = -1; zSide <= 1; zSide += 2) {
        for (int xSide = -1; xSide <= 1; xSide += 2) {
            glPushMatrix();
            glTranslatef(xSide * 4.8f, 0.0f, zSide * 2.2f);
            glRotatef(xSide * 20.0f, 0, 0, 1);
            glRotatef(-90, 1, 0, 0);
            glutSolidCylinder(0.28, 14.5, 10, 2);
            glPopMatrix();
        }
    }

    // Hub axle
    glPushMatrix();
    glTranslatef(0, hubY, -2.5f);
    glColor3f(0.95f, 0.82f, 0.2f);
    glutSolidCylinder(0.45, 5.0, 16, 2);
    glPopMatrix();

    // Rotating wheel
    glPushMatrix();
    glTranslatef(0, hubY, 0);
    glRotatef(ferrisAngle, 0, 0, 1);

    glColor3f(0.95f, 0.80f, 0.22f);
    for (int zRim = -1; zRim <= 1; zRim += 2) {
        glPushMatrix();
        glTranslatef(0, 0, zRim * 1.4f);
        glutSolidTorus(0.22, radius, 12, 36);
        glPopMatrix();
    }

    int numCabins = 10;
    for (int i = 0; i < numCabins; i++) {
        float theta = i * (360.0f / numCabins);
        float rad = theta * M_PI / 180.0f;

        glPushMatrix();
        glRotatef(theta, 0, 0, 1);
        glColor3f(0.95f, 0.85f, 0.35f);

        for (int zRim = -1; zRim <= 1; zRim += 2) {
            glPushMatrix();
            glTranslatef(0, 0, zRim * 1.4f);
            glRotatef(90, 0, 1, 0);
            glutSolidCylinder(0.08, radius, 8, 2);
            glPopMatrix();
        }
        glPopMatrix();

        // Diagonal bracing
        float nextTheta = (i + 0.5f) * (360.0f / numCabins);
        glPushMatrix();
        glRotatef(nextTheta, 0, 0, 1);
        glColor3f(0.85f, 0.72f, 0.15f);
        glRotatef(90, 0, 1, 0);
        glutSolidCylinder(0.05, radius * 0.7f, 6, 2);
        glPopMatrix();

        // Animated rim lights
        if (isSunset) {
            float flicker = 0.6f + 0.4f * sin(globalTime * 6.0f + i);
            GLfloat rimEmit[] = {
                flicker, 0.6f * flicker, 0.2f * flicker, 1.0f
            };
            glMaterialfv(GL_FRONT, GL_EMISSION, rimEmit);
            glColor3f(1.0f, 0.7f, 0.3f);
            glPushMatrix();
            glTranslatef(radius * cos(rad), radius * sin(rad), 0);
            glutSolidSphere(0.25, 10, 10);
            glPopMatrix();

            GLfloat noEmit[] = {0, 0, 0, 1};
            glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);
        }

        // Passenger cabin
        float cx = radius * cos(rad);
        float cy = radius * sin(rad);

        glPushMatrix();
        glTranslatef(cx, cy, 0);
        glRotatef(-ferrisAngle, 0, 0, 1);

        glColor3f((i % 2 == 0) ? 0.15f : 0.95f,
                  (i % 2 == 0) ? 0.65f : 0.78f,
                  (i % 2 == 0) ? 0.72f : 0.22f);
        glTranslatef(0, -1.1f, 0);
        glutSolidCube(1.6f);

        glColor3f(0.98f, 0.95f, 0.88f);
        glTranslatef(0, 1.0f, 0);
        glRotatef(-90, 1, 0, 0);
        glutSolidCone(1.2, 0.7, 12, 2);
        glPopMatrix();
    }

    glPopMatrix();
    glPopMatrix();
}

// ============================================================
// CIRCUS TENT WITH ANIMATED DECORATIVE LIGHTS
// ============================================================
void drawCircusTent() {
    glPushMatrix();
    glTranslatef(6.0f, 0.0f, 1.0f);

    int slices = 18;
    float radius = 7.8f;
    float wallH = 4.6f;
    float roofH = 6.2f;

    for (int i = 0; i < slices; i++) {
        float a1 = i * 2.0f * M_PI / slices;
        float a2 = (i + 1) * 2.0f * M_PI / slices;

        if (i % 2 == 0)
            glColor3f(0.10f, 0.45f, 0.48f);
        else
            glColor3f(0.96f, 0.92f, 0.82f);

        // Leave one opening
        if (i != 4) {
            glBegin(GL_QUADS);
            glNormal3f(cos(a1), 0, sin(a1));
            glVertex3f(radius * cos(a1), 0, radius * sin(a1));
            glVertex3f(radius * cos(a2), 0, radius * sin(a2));
            glVertex3f(radius * cos(a2), wallH, radius * sin(a2));
            glVertex3f(radius * cos(a1), wallH, radius * sin(a1));
            glEnd();
        }

        // Roof panels
        glBegin(GL_TRIANGLES);
        glNormal3f(cos((a1 + a2) / 2), 0.5f, sin((a1 + a2) / 2));
        glVertex3f(0, wallH + roofH, 0);
        glVertex3f((radius + 0.45f) * cos(a1), wallH,
                   (radius + 0.45f) * sin(a1));
        glVertex3f((radius + 0.45f) * cos(a2), wallH,
                   (radius + 0.45f) * sin(a2));
        glEnd();

        // Flickering bulbs
        float lightMult = isSunset ? 2.0f : 1.0f;
        float flicker = 0.65f +
            0.35f * sin(globalTime * 8.0f + i * 1.4f);
        float glow = lightMult * flicker;

        GLfloat emit[] = {
            glow, 0.75f * glow, 0.25f * glow, 1.0f
        };
        glMaterialfv(GL_FRONT, GL_EMISSION, emit);
        glColor3f(1.0f, 0.85f, 0.3f);

        glPushMatrix();
        glTranslatef((radius + 0.5f) * cos(a1),
                     wallH - 0.1f,
                     (radius + 0.5f) * sin(a1));
        glutSolidSphere(0.22, 10, 10);
        glPopMatrix();
    }

    GLfloat noEmit[] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);

    // Tent top decoration
    glColor3f(0.95f, 0.78f, 0.20f);
    glPushMatrix();
    glTranslatef(0, wallH + roofH, 0);
    glRotatef(-90, 1, 0, 0);
    glutSolidCylinder(0.12, 2.0, 8, 2);
    glTranslatef(0, 0, 2.0f);
    glutSolidSphere(0.28, 10, 10);
    glPopMatrix();

    glPopMatrix();
}

// ============================================================
// CLOWN CHARACTER
// ============================================================
void drawClown() {
    glPushMatrix();
    glTranslatef(15.0f, 0.0f, 10.0f);

    // Body
    glColor3f(0.95f, 0.15f, 0.25f);
    glPushMatrix();
    glTranslatef(0, 2.4f, 0);
    glScalef(1.1f, 1.8f, 0.7f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Head
    glColor3f(1.0f, 0.85f, 0.68f);
    glPushMatrix();
    glTranslatef(0, 4.0f, 0);
    glutSolidSphere(0.85f, 20, 20);
    glPopMatrix();

    // Nose
    glColor3f(1.0f, 0.0f, 0.0f);
    glPushMatrix();
    glTranslatef(0, 3.95f, 0.78f);
    glutSolidSphere(0.22f, 12, 12);
    glPopMatrix();

    // Eyes
    glColor3f(0.0f, 0.0f, 0.0f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.3f, 4.25f, 0.72f);
        glutSolidSphere(0.10f, 10, 10);
        glPopMatrix();
    }

    // Hat
    glColor3f(0.1f, 0.3f, 1.0f);
    glPushMatrix();
    glTranslatef(0, 4.85f, 0);
    glutSolidCone(0.65f, 1.1f, 16, 16);
    glPopMatrix();

    glColor3f(1.0f, 0.85f, 0.0f);
    glPushMatrix();
    glTranslatef(0, 5.95f, 0);
    glutSolidSphere(0.18f, 10, 10);
    glPopMatrix();

    // Arms
    glColor3f(1.0f, 0.8f, 0.65f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.9f, 2.9f, 0);
        glRotatef(side * 35.0f, 0, 0, 1);
        glScalef(0.3f, 1.2f, 0.3f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // Legs
    glColor3f(0.1f, 0.25f, 0.85f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.35f, 0.7f, 0);
        glScalef(0.35f, 1.3f, 0.4f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    glPopMatrix();
}

// ============================================================
// JOKER CHARACTER
// ============================================================
void drawJoker() {
    glPushMatrix();
    glTranslatef(-17.0f, 0.0f, 10.0f);

    // Body
    glColor3f(0.35f, 0.05f, 0.55f);
    glPushMatrix();
    glTranslatef(0, 2.4f, 0);
    glScalef(1.1f, 1.8f, 0.7f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Head
    glColor3f(0.9f, 0.85f, 0.72f);
    glPushMatrix();
    glTranslatef(0, 4.0f, 0);
    glutSolidSphere(0.85f, 20, 20);
    glPopMatrix();

    // Nose
    glColor3f(0.9f, 0.05f, 0.1f);
    glPushMatrix();
    glTranslatef(0, 3.95f, 0.78f);
    glutSolidSphere(0.17f, 12, 12);
    glPopMatrix();

    // Eyes
    glColor3f(0.05f, 0.05f, 0.05f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.3f, 4.25f, 0.72f);
        glutSolidSphere(0.1f, 10, 10);
        glPopMatrix();
    }

    // Joker cap
    glColor3f(0.15f, 0.8f, 0.25f);
    glPushMatrix();
    glTranslatef(0, 4.7f, 0);
    glRotatef(180, 1, 0, 0);
    glutSolidCone(0.65f, 1.3f, 12, 12);
    glPopMatrix();

    // Hat tips
    glColor3f(1.0f, 0.8f, 0.0f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.55f, 5.15f, 0);
        glutSolidSphere(0.17f, 10, 10);
        glPopMatrix();
    }

    // Arms
    glColor3f(0.9f, 0.8f, 0.7f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.9f, 2.9f, 0);
        glRotatef(side * 35.0f, 0, 0, 1);
        glScalef(0.3f, 1.2f, 0.3f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // Legs
    glColor3f(0.9f, 0.1f, 0.35f);
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.35f, 0.7f, 0);
        glScalef(0.35f, 1.3f, 0.4f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    glPopMatrix();
}

// ============================================================
// ANIMATED ICE-CREAM TRUCK
// ============================================================
void drawIceCreamTruck() {
    glPushMatrix();
    glTranslatef(iceCreamTruckX, 0.0f, 20.0f);

    // Main truck body
    glColor3f(0.95f, 0.25f, 0.4f);
    glPushMatrix();
    glTranslatef(0, 1.5f, 0);
    glScalef(7.0f, 2.0f, 2.8f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Driver cabin
    glColor3f(0.15f, 0.65f, 0.85f);
    glPushMatrix();
    glTranslatef(2.2f, 2.5f, 0);
    glScalef(2.0f, 1.5f, 2.7f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Front windshield
    glColor3f(0.65f, 0.9f, 1.0f);
    glPushMatrix();
    glTranslatef(2.2f, 2.6f, 1.37f);
    glScalef(1.4f, 0.9f, 0.05f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Ice-cream decoration on roof
    glColor3f(1.0f, 0.85f, 0.2f);
    glPushMatrix();
    glTranslatef(-1.0f, 2.9f, 0);
    glutSolidSphere(0.65f, 16, 16);
    glTranslatef(0, 0.65f, 0);
    glColor3f(1.0f, 0.35f, 0.55f);
    glutSolidCone(0.5f, 0.9f, 12, 12);
    glPopMatrix();

    // Four wheels
    for (int side = -1; side <= 1; side += 2) {
        for (int wheel = -1; wheel <= 1; wheel += 2) {
            glPushMatrix();
            glTranslatef(wheel * 2.2f, 0.65f, side * 1.4f);
            glRotatef(truckWheelAngle, 0, 0, 1);
            glColor3f(0.08f, 0.08f, 0.08f);
            glutSolidTorus(0.18f, 0.48f, 12, 20);
            glPopMatrix();
        }
    }

    // Side serving window
    glColor3f(1.0f, 0.9f, 0.65f);
    glPushMatrix();
    glTranslatef(-0.7f, 2.0f, 1.42f);
    glScalef(1.5f, 0.8f, 0.08f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Front headlights
    glColor3f(1.0f, 1.0f, 0.2f);
    glPushMatrix();
    glTranslatef(3.55f, 1.5f, 0.8f);
    glutSolidSphere(0.2f, 10, 10);
    glPopMatrix();

    glPopMatrix();
}

// ============================================================
// ROLLER COASTER
// ============================================================
void drawRollerCoaster() {
    int segments = 120;
    float step = (2.0f * M_PI) / segments;

    glLineWidth(4.5f);
    glColor3f(0.15f, 0.72f, 0.78f);

    for (int i = 0; i < segments; i++) {
        float t1 = i * step;
        float t2 = (i + 1) * step;
        float x1, y1, z1, x2, y2, z2;

        getCoasterPoint(t1, x1, y1, z1);
        getCoasterPoint(t2, x2, y2, z2);

        glBegin(GL_LINES);
        glVertex3f(x1 - 0.35f, y1, z1);
        glVertex3f(x2 - 0.35f, y2, z2);
        glVertex3f(x1 + 0.35f, y1, z1);
        glVertex3f(x2 + 0.35f, y2, z2);
        glEnd();

        if (i % 4 == 0 && y1 < 12.0f) {
            glColor3f(0.48f, 0.32f, 0.65f);
            glPushMatrix();
            glTranslatef(x1, 0, z1);
            glRotatef(-90, 1, 0, 0);
            glutSolidCylinder(0.16, y1, 8, 2);
            glPopMatrix();
            glColor3f(0.15f, 0.72f, 0.78f);
        }
    }

    // Five coaster cars
    for (int c = 0; c < 5; c++) {
        float carT = coasterProgress - c * 0.14f;
        float cx, cy, cz, nx, ny, nz;

        getCoasterPoint(carT, cx, cy, cz);
        getCoasterPoint(carT + 0.04f, nx, ny, nz);

        float dx = nx - cx;
        float dy = ny - cy;
        float dz = nz - cz;
        float length = sqrt(dx * dx + dy * dy + dz * dz);

        float yaw = atan2(dx, dz) * 180.0f / M_PI;
        float pitch = -asin(dy / length) * 180.0f / M_PI;

        glPushMatrix();
        glTranslatef(cx, cy + 0.3f, cz);
        glRotatef(yaw, 0, 1, 0);
        glRotatef(pitch, 1, 0, 0);

        glColor3f(c == 0 ? 0.95f : 0.85f,
                  c % 2 == 0 ? 0.25f : 0.75f, 0.2f);
        glScalef(1.0f, 0.65f, 1.45f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}

// ============================================================
// HOT AIR BALLOON
// ============================================================
void drawHotAirBalloon() {
    glPushMatrix();
    glTranslatef(hotAirX, hotAirY, -14.0f);

    glColor3f(0.58f, 0.35f, 0.16f);
    glutSolidCube(1.4f);

    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_LINES);
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            glVertex3f(x * 0.6f, 0.7f, z * 0.6f);
            glVertex3f(x * 1.0f, 2.4f, z * 1.0f);
        }
    }
    glEnd();

    glTranslatef(0, 4.8f, 0);
    glColor3f(0.15f, 0.68f, 0.75f);
    glutSolidSphere(3.1, 20, 20);

    glColor3f(0.96f, 0.52f, 0.15f);
    glRotatef(90, 1, 0, 0);
    glutSolidTorus(0.45, 2.95, 12, 24);

    glPopMatrix();
}

// ============================================================
// FLOATING BALLOONS
// ============================================================
void drawFloatingBalloons() {
    for (const auto& b : floatingBalloons) {
        glPushMatrix();
        glTranslatef(b.x, b.y, b.z);

        glColor3f(0.85f, 0.85f, 0.85f);
        glBegin(GL_LINES);
        glVertex3f(0, 0, 0);
        glVertex3f(0, -1.3f, 0);
        glEnd();

        glColor3f(b.r, b.g, b.b);
        glScalef(0.85f, 1.1f, 0.85f);
        glutSolidSphere(0.48, 14, 14);
        glPopMatrix();
    }
}

// ============================================================
// TREES AND FLOWERS
// ============================================================
void drawVegetation() {
    float treePos[][2] = {
        {-30, -18}, {-28, 6}, {-25, 18}, {28, -16},
        {30, 8}, {-8, -22}, {12, -22}, {22, -22}
    };

    for (auto& p : treePos) {
        glPushMatrix();
        glTranslatef(p[0], 0, p[1]);

        glColor3f(0.42f, 0.25f, 0.12f);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        glutSolidCylinder(0.38, 3.2, 10, 2);
        glPopMatrix();

        glColor3f(isSunset ? 0.08f : 0.18f,
                  isSunset ? 0.25f : 0.60f,
                  isSunset ? 0.05f : 0.22f);
        glTranslatef(0, 3.8f, 0);
        glutSolidSphere(2.1, 14, 14);
        glTranslatef(0, 1.3f, 0);
        glutSolidSphere(1.5, 14, 14);
        glPopMatrix();
    }

    // Flowers
    for (float fx = -16.0f; fx <= 16.0f; fx += 2.2f) {
        if (fabs(fx) < 4.5f) continue;

        glPushMatrix();
        glTranslatef(fx, 0.2f, 16.5f);
        glColor3f(0.95f, 0.25f, 0.45f);
        glutSolidSphere(0.32, 8, 8);
        glTranslatef(0.6f, 0, 0.3f);
        glColor3f(1.0f, 0.85f, 0.15f);
        glutSolidSphere(0.30, 8, 8);
        glPopMatrix();
    }
}

// ============================================================
// MOVING BIRDS WITH FLAPPING WINGS
// ============================================================
void drawBirds() {
    glColor3f(0.2f, 0.2f, 0.25f);

    for (const auto& bd : birds) {
        // Bird body
        glPushMatrix();
        glTranslatef(bd.x, bd.y, bd.z);
        glScalef(0.6f, 0.2f, 0.25f);
        glutSolidSphere(0.5, 8, 8);
        glPopMatrix();

        // Flapping wings
        for (int side = -1; side <= 1; side += 2) {
            glPushMatrix();
            glTranslatef(bd.x, bd.y, bd.z);
            glRotatef(side * birdWingAngle, 1, 0, 0);
            glTranslatef(0, 0, side * 0.45f);
            glScalef(0.35f, 0.05f, 0.9f);
            glutSolidCube(1.0f);
            glPopMatrix();
        }
    }
}

// ============================================================
// MAIN DISPLAY FUNCTION
// ============================================================
void display() {
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(camX, camY, camZ,
              0.0f, 6.0f, -12.0f,
              0.0f, 1.0f, 0.0f);

    drawSkyDome();

    // Sunlight
    GLfloat lightPos[] = {4.0f, 32.0f, -50.0f, 1.0f};
    float sunIntensity = isSunset ? 1.1f : 0.90f;

    GLfloat diffuse[] = {
        sunIntensity,
        sunIntensity * 0.80f,
        isSunset ? 0.5f : sunIntensity * 0.88f,
        1.0f
    };

    GLfloat ambient[] = {
        isSunset ? 0.35f : 0.38f,
        isSunset ? 0.25f : 0.36f,
        isSunset ? 0.22f : 0.36f,
        1.0f
    };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    drawGroundAndCity();
    drawCelestialBody();
    drawClouds();

    drawFerrisWheel();
    drawCircusTent();

    drawClown();
    drawJoker();

    drawIceCreamTruck();
    drawRollerCoaster();

    drawHotAirBalloon();
    drawFloatingBalloons();
    drawVegetation();
    drawBirds();

    glutSwapBuffers();
}

// ============================================================
// ANIMATION TIMER
// ============================================================
void update(int value) {
    if (!isPaused) {
        globalTime += 0.016f;

        // Ferris wheel
        ferrisAngle += 0.55f;
        if (ferrisAngle >= 360.0f) ferrisAngle -= 360.0f;

        // Roller coaster
        coasterProgress += 0.022f;
        if (coasterProgress >= 2.0f * M_PI)
            coasterProgress -= 2.0f * M_PI;

        // Sun brightness pulse and bird wings
        sunPulse += 0.02f;
        birdWingAngle = 35.0f * sin(globalTime * 10.0f);

        // Hot-air balloon
        hotAirY += 0.018f;
        hotAirX += 0.012f * sin(globalTime * 0.5f);
        if (hotAirY > 36.0f) hotAirY = 10.0f;

        // Ice-cream truck movement and wheel rotation
        iceCreamTruckX += 0.045f;
        if (iceCreamTruckX > 38.0f)
            iceCreamTruckX = -38.0f;

        truckWheelAngle -= 5.0f;
        if (truckWheelAngle < 0.0f)
            truckWheelAngle += 360.0f;

        // Floating balloons
        for (auto& b : floatingBalloons) {
            b.y += b.speedY;
            b.x += b.speedX;

            if (b.y > 32.0f) {
                b.y = 2.0f;
                b.x = -28.0f + (rand() % 56);
            }
        }

        // Clouds
        for (auto& c : clouds) {
            c.x += c.speed;
            if (c.x > 52.0f) c.x = -52.0f;
        }

        // Birds move from left to right
        for (auto& bd : birds) {
            bd.x += bd.speed;
            if (bd.x > 46.0f) bd.x = -46.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// ============================================================
// WINDOW RESHAPE
// ============================================================
void reshape(int w, int h) {
    if (h == 0) h = 1;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(48.0, (double)w / (double)h, 1.0, 250.0);
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================
// KEYBOARD CONTROLS
// ============================================================
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 'n':
        case 'N':
            isSunset = !isSunset;
            break;

        case ' ':
            isPaused = !isPaused;
            break;

        case 27:
            exit(0);
            break;
    }

    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    // Camera is fixed in this project.
}

// ============================================================
// MAIN
// ============================================================
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 800);
    glutCreateWindow("3D Amusement Park - Animated Carnival Scene");

    initScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
