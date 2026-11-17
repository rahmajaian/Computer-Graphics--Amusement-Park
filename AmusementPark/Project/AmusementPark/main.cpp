#include <GL/freeglut.h>
#include <cmath>
#include <vector>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Global State & Animation Variables ---
bool isNight = false;
bool isPaused = false;
float globalTime = 0.0f;

// Camera Controls (Positioned to match the elevated 3/4 view)
float camX = 4.0f, camY = 14.0f, camZ = 44.0f;
float camAngleY = -0.08f;

// Feature Animation States
float ferrisAngle = 0.0f;
float carouselAngle = 0.0f;
float coasterProgress = 0.0f;
float hotAirY = 18.0f, hotAirX = 22.0f;
float sunPulse = 0.0f;
float birdWingAngle = 0.0f;
float limbSwing = 0.0f;

struct Balloon {
    float x, y, z;
    float speedY, speedX;
    float r, g, b;
};
std::vector<Balloon> floatingBalloons;

struct Cloud {
    float x, y, z;
    float speed;
    float scale;
};
std::vector<Cloud> clouds;

struct Bird {
    float x, y, z;
    float speed;
};
std::vector<Bird> birds;

// --- 3D Vertical Loop Roller Coaster Track Parametric Function ---
void getCoasterPoint(float t, float &x, float &y, float &z) {
    // t goes from 0 to 2*PI around the park background
    float angle = t;
    float radiusX = 24.0f;
    float radiusZ = 12.0f;

    x = 4.0f + radiusX * cos(angle);
    z = -16.0f + radiusZ * sin(angle);
    y = 4.5f + 2.5f * sin(2.0f * angle);

    // Add a dramatic vertical loop near the upper-right back section (around angle = 1.1 rad)
    float loopCenter = 1.15f;
    float dist = angle - loopCenter;
    if (dist > -0.6f && dist < 0.6f) {
        float localTheta = ((dist + 0.6f) / 1.2f) * 2.0f * M_PI;
        float loopR = 6.5f;
        x += loopR * sin(localTheta) * 0.7f;
        y += loopR * (1.0f - cos(localTheta));
    }
}

// --- Scene Initialization ---
void initScene() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);

    // Floating Balloons (Red, Teal, Gold, Pink, Purple)
    float colors[5][3] = {{1.0f,0.25f,0.3f}, {0.2f,0.8f,0.95f}, {1.0f,0.82f,0.15f}, {0.95f,0.45f,0.75f}, {0.6f,0.35f,0.95f}};
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

    // Fluffy Clouds
    for (int i = 0; i < 7; i++) {
        Cloud c;
        c.x = -45.0f + i * 15.0f;
        c.y = 26.0f + (i % 3) * 2.2f;
        c.z = -28.0f - (i % 2) * 8.0f;
        c.speed = 0.025f + (i % 3) * 0.008f;
        c.scale = 1.1f + (i % 2) * 0.45f;
        clouds.push_back(c);
    }

    // Flying Birds
    for (int i = 0; i < 6; i++) {
        Bird bd;
        bd.x = -34.0f + i * 12.0f;
        bd.y = 20.0f + (i % 3) * 2.0f;
        bd.z = -10.0f + (i % 4) * 5.0f;
        bd.speed = 0.07f + (i % 2) * 0.025f;
        birds.push_back(bd);
    }
}

// --- 1. Ground, Cobblestone Plaza, Elevated Railing & Decorative Lights ---
void drawGroundAndPlaza() {
    // Lush Park Grass
    glColor3f(isNight ? 0.04f : 0.18f, isNight ? 0.18f : 0.55f, isNight ? 0.05f : 0.20f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-70, 0, -70);
    glVertex3f(70, 0, -70);
    glVertex3f(70, 0, 70);
    glVertex3f(-70, 0, 70);
    glEnd();

    // Warm Cobblestone Plaza Floor
    glColor3f(isNight ? 0.22f : 0.58f, isNight ? 0.20f : 0.52f, isNight ? 0.22f : 0.48f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-34, 0.03f, -14);
    glVertex3f(34, 0.03f, -14);
    glVertex3f(34, 0.03f, 26);
    glVertex3f(-34, 0.03f, 26);
    glEnd();

    // Elevated Stone Balustrade / Overlook Railing (Foreground Right)
    glPushMatrix();
    glTranslatef(16.0f, 0.0f, 21.0f);
    glRotatef(-22.0f, 0, 1, 0);
    glColor3f(isNight ? 0.3f : 0.78f, isNight ? 0.28f : 0.74f, isNight ? 0.3f : 0.68f);

    // Base & Top Rail
    glPushMatrix();
    glTranslatef(0, 0.3f, 0);
    glScalef(22.0f, 0.6f, 0.8f);
    glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, 1.8f, 0);
    glScalef(22.0f, 0.25f, 0.9f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Balusters
    for (float bx = -10.0f; bx <= 10.0f; bx += 1.2f) {
        glPushMatrix();
        glTranslatef(bx, 1.05f, 0);
        glScalef(0.22f, 1.3f, 0.22f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
    glPopMatrix();

    // Animated Decorative Street Lamps around the Plaza
    float lampCoords[][2] = {{-22, 12}, {-12, 16}, {14, 12}, {22, 4}, {-6, -12}, {16, -12}};
    for (int i = 0; i < 6; i++) {
        glPushMatrix();
        glTranslatef(lampCoords[i][0], 0, lampCoords[i][1]);

        // Iron Post
        glColor3f(0.15f, 0.18f, 0.20f);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        glutSolidCylinder(0.12, 3.8, 10, 2);
        glPopMatrix();

        // Periodically Changing Glowing Lantern
        float pulse = 0.6f + 0.4f * sin(globalTime * 3.5f + i);
        float r = (i % 2 == 0) ? 1.0f : 0.3f;
        float g = 0.85f;
        float b = (i % 2 == 0) ? 0.3f : 1.0f;

        GLfloat emit[] = {r * pulse, g * pulse, b * pulse, 1.0f};
        glMaterialfv(GL_FRONT, GL_EMISSION, emit);
        glColor3f(r, g, b);
        glTranslatef(0, 4.0f, 0);
        glutSolidSphere(0.38, 14, 14);

        GLfloat noEmit[] = {0, 0, 0, 1};
        glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);
        glPopMatrix();
    }
}

// --- 2. Animated Sun / Moon with Brightness Effect ---
void drawCelestialBody() {
    glPushMatrix();
    glTranslatef(4.0f, 32.0f, -50.0f);
    float brightness = 0.75f + 0.25f * sin(sunPulse);

    if (!isNight) {
        GLfloat sunEmit[] = {1.0f * brightness, 0.90f * brightness, 0.35f * brightness, 1.0f};
        glMaterialfv(GL_FRONT, GL_EMISSION, sunEmit);
        glColor3f(1.0f, 0.95f * brightness, 0.4f);
        glutSolidSphere(3.4f + 0.25f * sin(sunPulse), 24, 24);
    } else {
        GLfloat moonEmit[] = {0.75f, 0.80f, 0.95f, 1.0f};
        glMaterialfv(GL_FRONT, GL_EMISSION, moonEmit);
        glColor3f(0.88f, 0.90f, 0.98f);
        glutSolidSphere(2.7f, 24, 24);
    }

    GLfloat noEmit[] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);
    glPopMatrix();
}

// --- 3. Moving Clouds ---
void drawClouds() {
    glColor3f(isNight ? 0.30f : 0.98f, isNight ? 0.33f : 0.95f, isNight ? 0.42f : 0.92f);
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

// --- 4. Rotating Ferris Wheel (Left Side - Gold & Teal with Upright Cabins) ---
void drawFerrisWheel() {
    glPushMatrix();
    glTranslatef(-20.0f, 0.0f, -6.0f);
    glRotatef(20.0f, 0, 1, 0);

    float hubY = 13.5f;
    float radius = 9.5f;

    // Teal A-Frame Support Legs
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

    // Hub Axle
    glPushMatrix();
    glTranslatef(0, hubY, -2.5f);
    glColor3f(0.95f, 0.82f, 0.2f);
    glutSolidCylinder(0.45, 5.0, 16, 2);
    glPopMatrix();

    // Rotating Wheel
    glPushMatrix();
    glTranslatef(0, hubY, 0);
    glRotatef(ferrisAngle, 0, 0, 1);

    // Golden Rims
    glColor3f(0.95f, 0.80f, 0.22f);
    for (int zRim = -1; zRim <= 1; zRim += 2) {
        glPushMatrix();
        glTranslatef(0, 0, zRim * 1.4f);
        glutSolidTorus(0.22, radius, 12, 36);
        glPopMatrix();
    }

    // Spokes & Upright Cabins
    int numCabins = 10;
    for (int i = 0; i < numCabins; i++) {
        float theta = i * (360.0f / numCabins);
        float rad = theta * M_PI / 180.0f;

        // Spokes
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

        // Upright Passenger Cabin
        float cx = radius * cos(rad);
        float cy = radius * sin(rad);

        glPushMatrix();
        glTranslatef(cx, cy, 0);
        glRotatef(-ferrisAngle, 0, 0, 1); // Keeps cabin level at all angles

        // Gondola Body
        glColor3f((i % 2 == 0) ? 0.15f : 0.95f, (i % 2 == 0) ? 0.65f : 0.78f, (i % 2 == 0) ? 0.72f : 0.22f);
        glTranslatef(0, -1.1f, 0);
        glutSolidCube(1.6f);

        // Cabin Roof
        glColor3f(0.98f, 0.95f, 0.88f);
        glTranslatef(0, 1.0f, 0);
        glRotatef(-90, 1, 0, 0);
        glutSolidCone(1.2, 0.7, 12, 2);
        glPopMatrix();
    }
    glPopMatrix();
    glPopMatrix();
}

// --- 5. Grand Teal & Cream Striped Circus Tent (Center-Right) ---
void drawCircusTent() {
    glPushMatrix();
    glTranslatef(6.0f, 0.0f, 1.0f);

    int slices = 18;
    float radius = 7.8f;
    float wallH = 4.6f;
    float roofH = 6.2f;

    // Striped Cylindrical Walls & Conical Roof
    for (int i = 0; i < slices; i++) {
        float a1 = i * 2.0f * M_PI / slices;
        float a2 = (i + 1) * 2.0f * M_PI / slices;

        // Alternating Teal & Warm Cream Stripes
        if (i % 2 == 0) glColor3f(0.10f, 0.45f, 0.48f);
        else glColor3f(0.96f, 0.92f, 0.82f);

        // Leave entrance opening on the front-left slice
        if (i != 4) {
            glBegin(GL_QUADS);
            glNormal3f(cos(a1), 0, sin(a1));
            glVertex3f(radius * cos(a1), 0, radius * sin(a1));
            glVertex3f(radius * cos(a2), 0, radius * sin(a2));
            glVertex3f(radius * cos(a2), wallH, radius * sin(a2));
            glVertex3f(radius * cos(a1), wallH, radius * sin(a1));
            glEnd();
        }

        // Conical Roof
        glBegin(GL_TRIANGLES);
        glNormal3f(cos((a1+a2)/2), 0.5f, sin((a1+a2)/2));
        glVertex3f(0, wallH + roofH, 0);
        glVertex3f((radius + 0.45f) * cos(a1), wallH, (radius + 0.45f) * sin(a1));
        glVertex3f((radius + 0.45f) * cos(a2), wallH, (radius + 0.45f) * sin(a2));
        glEnd();

        // Warm Flickering Fairy Lights along Scalloped Eaves
        float flicker = 0.6f + 0.4f * sin(globalTime * 7.0f + i * 1.4f);
        GLfloat emit[] = {1.0f * flicker, 0.82f * flicker, 0.25f * flicker, 1.0f};
        glMaterialfv(GL_FRONT, GL_EMISSION, emit);
        glColor3f(1.0f, 0.88f, 0.35f);
        glPushMatrix();
        glTranslatef((radius + 0.5f) * cos(a1), wallH - 0.1f, (radius + 0.5f) * sin(a1));
        glutSolidSphere(0.22, 10, 10);
        glPopMatrix();
    }

    GLfloat noEmit[] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);

    // Golden Spire Finial
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

// --- 6. "Scoop's Paradise" Ice Cream Stand & Vendor (Foreground Left) ---
void drawIceCreamStand() {
    glPushMatrix();
    glTranslatef(-8.5f, 0.0f, 9.0f);
    glRotatef(25.0f, 0, 1, 0);

    // Cream & Pastel Pink Kiosk Booth
    glColor3f(0.96f, 0.90f, 0.80f);
    glPushMatrix();
    glTranslatef(0, 1.6f, 0);
    glScalef(3.4f, 3.2f, 2.4f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Pink Counter Trim
    glColor3f(0.92f, 0.52f, 0.62f);
    glPushMatrix();
    glTranslatef(0, 1.4f, 1.25f);
    glScalef(3.5f, 0.25f, 0.3f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Dual Purple Castle Turret Towers on Corners
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 1.6f, 0, 1.1f);
        glColor3f(0.92f, 0.85f, 0.76f);
        glRotatef(-90, 1, 0, 0);
        glutSolidCylinder(0.35, 3.6, 12, 2);
        glTranslatef(0, 0, 3.6f);
        glColor3f(0.55f, 0.35f, 0.72f);
        glutSolidCone(0.55, 1.3, 12, 2);
        glPopMatrix();
    }

    // Giant Decorative Ice Cream Cone & Swirl on Roof
    glPushMatrix();
    glTranslatef(-0.5f, 3.6f, 0.2f);
    glColor3f(1.0f, 0.96f, 0.92f);
    glutSolidSphere(0.75, 14, 14);
    glTranslatef(0, 0.65f, 0);
    glutSolidSphere(0.50, 14, 14);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.7f, 4.1f, 0.3f);
    glRotatef(-15.0f, 0, 0, 1);
    glColor3f(0.85f, 0.58f, 0.28f);
    glRotatef(90, 1, 0, 0);
    glutSolidCone(0.45, 1.1, 12, 2);
    glRotatef(-90, 1, 0, 0);
    glColor3f(0.98f, 0.55f, 0.68f);
    glutSolidSphere(0.48, 12, 12);
    glPopMatrix();

    // Animated Ice Cream Vendor inside Serving Window
    glPushMatrix();
    glTranslatef(0.0f, 1.7f, 0.7f);
    glColor3f(0.95f, 0.80f, 0.68f);
    glutSolidSphere(0.32, 12, 12); // Head
    glPopMatrix();

    glPopMatrix();
}

// --- 7. Vertical-Looping 3D Roller Coaster (Background) ---
void drawRollerCoaster() {
    int segments = 120;
    float step = (2.0f * M_PI) / segments;

    for (int i = 0; i < segments; i++) {
        float t1 = i * step;
        float t2 = (i + 1) * step;
        float x1, y1, z1, x2, y2, z2;
        getCoasterPoint(t1, x1, y1, z1);
        getCoasterPoint(t2, x2, y2, z2);

        // Teal Coaster Rails
        glColor3f(0.15f, 0.72f, 0.78f);
        glLineWidth(4.5f);
        glBegin(GL_LINES);
        glVertex3f(x1 - 0.35f, y1, z1);
        glVertex3f(x2 - 0.35f, y2, z2);
        glVertex3f(x1 + 0.35f, y1, z1);
        glVertex3f(x2 + 0.35f, y2, z2);
        glEnd();

        // Purple Support Pillars
        if (i % 4 == 0 && y1 < 12.0f) {
            glColor3f(0.48f, 0.32f, 0.65f);
            glPushMatrix();
            glTranslatef(x1, 0, z1);
            glRotatef(-90, 1, 0, 0);
            glutSolidCylinder(0.16, y1, 8, 2);
            glPopMatrix();
        }
    }

    // Multi-Car Coaster Train
    for (int c = 0; c < 5; c++) {
        float carT = coasterProgress - c * 0.14f;
        float cx, cy, cz, nx, ny, nz;
        getCoasterPoint(carT, cx, cy, cz);
        getCoasterPoint(carT + 0.04f, nx, ny, nz);

        float dx = nx - cx, dy = ny - cy, dz = nz - cz;
        float yaw = atan2(dx, dz) * 180.0f / M_PI;
        float pitch = -asin(dy / sqrt(dx*dx + dy*dy + dz*dz)) * 180.0f / M_PI;

        glPushMatrix();
        glTranslatef(cx, cy + 0.3f, cz);
        glRotatef(yaw, 0, 1, 0);
        glRotatef(pitch, 1, 0, 0);

        glColor3f(c == 0 ? 0.95f : 0.85f, c % 2 == 0 ? 0.25f : 0.75f, 0.2f);
        glScalef(1.0f, 0.65f, 1.45f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}

// --- 8. Animated Juggling Clown & Joker (Center Foreground) ---
void drawClowns() {
    // Juggling Clown in Purple & Green (Matching Foreground Center)
    glPushMatrix();
    glTranslatef(-2.0f, 0.0f, 13.5f);

    float swing = sin(limbSwing) * 28.0f;

    // Legs
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.24f, 1.1f, 0);
        glRotatef(side * swing * 0.5f, 1, 0, 0);
        glColor3f(0.18f, 0.72f, 0.35f); // Green plaid pants
        glTranslatef(0, -0.55f, 0);
        glScalef(0.26f, 1.1f, 0.26f);
        glutSolidCube(1.0f);

        // Purple Clown Shoes
        glColor3f(0.55f, 0.20f, 0.78f);
        glTranslatef(0, -0.5f, 0.28f);
        glScalef(1.3f, 0.35f, 1.9f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // Purple Polka-Dot Jacket Torso
    glPushMatrix();
    glTranslatef(0, 1.8f, 0);
    glColor3f(0.52f, 0.22f, 0.72f);
    glScalef(0.88f, 1.25f, 0.52f);
    glutSolidCube(1.0f);
    glPopMatrix();

    // Animated Juggling Arms
    for (int side = -1; side <= 1; side += 2) {
        glPushMatrix();
        glTranslatef(side * 0.58f, 2.25f, 0);
        glRotatef(-45.0f + side * swing, 1, 0, 0);
        glRotatef(side * 25.0f, 0, 0, 1);
        glColor3f(0.52f, 0.22f, 0.72f);
        glTranslatef(0, -0.45f, 0);
        glScalef(0.22f, 0.9f, 0.22f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // Head & Green Curly Hair
    glPushMatrix();
    glTranslatef(0, 2.8f, 0);
    glColor3f(1.0f, 0.92f, 0.86f);
    glutSolidSphere(0.40, 16, 16);

    // Red Nose
    glColor3f(0.95f, 0.15f, 0.15f);
    glTranslatef(0, 0, 0.40f);
    glutSolidSphere(0.11, 10, 10);

    // Green Hair Puffs
    glColor3f(0.15f, 0.75f, 0.28f);
    glTranslatef(-0.38f, 0.15f, -0.35f);
    glutSolidSphere(0.22, 10, 10);
    glTranslatef(0.76f, 0, 0);
    glutSolidSphere(0.22, 10, 10);
    glPopMatrix();

    // Animated Juggling Balls in Air
    float ballColors[3][3] = {{1,0.2,0.4}, {0.2,0.9,0.4}, {0.2,0.6,1.0}};
    for (int b = 0; b < 3; b++) {
        float phase = globalTime * 4.5f + b * (2.0f * M_PI / 3.0f);
        float bx = 0.85f * cos(phase);
        float by = 3.2f + 0.95f * fabs(sin(phase));
        glPushMatrix();
        glTranslatef(bx, by, 0.4f);
        glColor3f(ballColors[b][0], ballColors[b][1], ballColors[b][2]);
        glutSolidSphere(0.14, 10, 10);
        glPopMatrix();
    }

    glPopMatrix();
}

// --- 9. Multi-Colored Hot Air Balloon (Upper Right Sky) ---
void drawHotAirBalloon() {
    glPushMatrix();
    glTranslatef(hotAirX, hotAirY, -14.0f);

    // Wicker Basket
    glColor3f(0.58f, 0.35f, 0.16f);
    glutSolidCube(1.4f);

    // Suspension Ropes
    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_LINES);
    for (int x = -1; x <= 1; x += 2) {
        for (int z = -1; z <= 1; z += 2) {
            glVertex3f(x * 0.6f, 0.7f, z * 0.6f);
            glVertex3f(x * 1.0f, 2.4f, z * 1.0f);
        }
    }
    glEnd();

    // Colorful Striped Balloon Envelope
    glTranslatef(0, 4.8f, 0);
    glColor3f(0.15f, 0.68f, 0.75f); // Teal base
    glutSolidSphere(3.1, 20, 20);

    glColor3f(0.96f, 0.52f, 0.15f); // Orange band
    glRotatef(90, 1, 0, 0);
    glutSolidTorus(0.45, 2.95, 12, 24);

    glPopMatrix();
}

// --- 10. Floating Balloons ---
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

// --- 11. Trees, Shrubs & Flowerbeds ---
void drawVegetation() {
    float treePos[][2] = {
        {-30, -18}, {-28, 6}, {-25, 18}, {28, -16}, {30, 8}, {-8, -22}, {12, -22}, {22, -22}
    };
    for (auto& p : treePos) {
        glPushMatrix();
        glTranslatef(p[0], 0, p[1]);

        glColor3f(0.42f, 0.25f, 0.12f);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        glutSolidCylinder(0.38, 3.2, 10, 2);
        glPopMatrix();

        glColor3f(isNight ? 0.05f : 0.18f, isNight ? 0.28f : 0.60f, isNight ? 0.08f : 0.22f);
        glTranslatef(0, 3.8f, 0);
        glutSolidSphere(2.1, 14, 14);
        glTranslatef(0, 1.3f, 0);
        glutSolidSphere(1.5, 14, 14);
        glPopMatrix();
    }

    // Colorful Flowerbeds Bordering the Plaza
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

// --- 12. Moving Birds with Flapping Wings ---
void drawBirds() {
    glColor3f(0.2f, 0.2f, 0.25f);
    for (const auto& bd : birds) {
        glPushMatrix();
        glTranslatef(bd.x, bd.y, bd.z);
        glScalef(0.6f, 0.2f, 0.25f);
        glutSolidSphere(0.5, 8, 8);
        glPopMatrix();

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

// --- Main Display Loop ---
void display() {
    float dayShift = 0.06f * sin(sunPulse);
    if (!isNight) {
        // Golden-hour warm sky gradient matching the render
        glClearColor(0.52f + dayShift, 0.78f + dayShift, 0.96f, 1.0f);
    } else {
        glClearColor(0.04f, 0.05f, 0.14f, 1.0f);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    float lookX = camX + sin(camAngleY);
    float lookZ = camZ - cos(camAngleY);
    gluLookAt(camX, camY, camZ, lookX, camY - 0.25f, lookZ, 0.0f, 1.0f, 0.0f);

    GLfloat lightPos[] = {4.0f, 32.0f, -50.0f, 1.0f};
    float sunIntensity = isNight ? 0.25f : (0.90f + 0.10f * sin(sunPulse));
    GLfloat diffuse[] = {sunIntensity, sunIntensity * 0.95f, isNight ? 0.45f : sunIntensity * 0.88f, 1.0f};
    GLfloat ambient[] = {isNight ? 0.15f : 0.38f, isNight ? 0.15f : 0.36f, isNight ? 0.25f : 0.36f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    drawGroundAndPlaza();
    drawCelestialBody();
    drawClouds();
    drawFerrisWheel();
    drawCircusTent();
    drawIceCreamStand();
    drawRollerCoaster();
    drawClowns();
    drawHotAirBalloon();
    drawFloatingBalloons();
    drawVegetation();
    drawBirds();

    glutSwapBuffers();
}

// --- Animation Timer (~60 FPS) ---
void update(int value) {
    if (!isPaused) {
        globalTime += 0.016f;
        ferrisAngle += 0.55f;
        if (ferrisAngle >= 360.0f) ferrisAngle -= 360.0f;

        coasterProgress += 0.022f;
        if (coasterProgress >= 2.0f * M_PI) coasterProgress -= 2.0f * M_PI;

        sunPulse += 0.02f;
        birdWingAngle = 35.0f * sin(globalTime * 10.0f);
        limbSwing += 0.08f;

        // Rising & Drifting Hot Air Balloon
        hotAirY += 0.018f;
        hotAirX += 0.012f * sin(globalTime * 0.5f);
        if (hotAirY > 36.0f) hotAirY = 10.0f;

        for (auto& b : floatingBalloons) {
            b.y += b.speedY;
            b.x += b.speedX;
            if (b.y > 32.0f) {
                b.y = 2.0f;
                b.x = -28.0f + (rand() % 56);
            }
        }

        for (auto& c : clouds) {
            c.x += c.speed;
            if (c.x > 52.0f) c.x = -52.0f;
        }

        for (auto& bd : birds) {
            bd.x += bd.speed;
            if (bd.x > 46.0f) bd.x = -46.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(48.0, (double)w / (double)h, 1.0, 220.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    float moveSpeed = 1.2f;
    switch (key) {
        case 'n': case 'N': isNight = !isNight; break;
        case ' ': isPaused = !isPaused; break;
        case 'w': case 'W': camX += sin(camAngleY) * moveSpeed; camZ -= cos(camAngleY) * moveSpeed; break;
        case 's': case 'S': camX -= sin(camAngleY) * moveSpeed; camZ += cos(camAngleY) * moveSpeed; break;
        case 'a': case 'A': camAngleY -= 0.06f; break;
        case 'd': case 'D': camAngleY += 0.06f; break;
        case 'q': case 'Q': camY += 0.8f; break;
        case 'e': case 'E': if (camY > 2.0f) camY -= 0.8f; break;
        case 27: exit(0); break;
    }
}

void specialKeys(int key, int x, int y) {
    float moveSpeed = 1.2f;
    switch (key) {
        case GLUT_KEY_UP: camX += sin(camAngleY) * moveSpeed; camZ -= cos(camAngleY) * moveSpeed; break;
        case GLUT_KEY_DOWN: camX -= sin(camAngleY) * moveSpeed; camZ += cos(camAngleY) * moveSpeed; break;
        case GLUT_KEY_LEFT: camAngleY -= 0.06f; break;
        case GLUT_KEY_RIGHT: camAngleY += 0.06f; break;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 800);
    glutCreateWindow("3D Amusement Park - FreeGLUT / Code::Blocks");

    initScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
