
#include <GL/freeglut.h>
#include <cmath>

const float PI = 3.14159265f;

// ---------------- CIRCLE ----------------

void drawCircle(float x, float y, float radius)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);

    for (int i = 0; i <= 100; i++)
    {
        float angle = 2 * PI * i / 100;
        glVertex2f(x + cos(angle) * radius,
                   y + sin(angle) * radius);
    }
    glEnd();
}

// ---------------- RECTANGLE ----------------

void drawRect(float x1, float y1, float x2, float y2)
{
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

// ---------------- SKYLINE ----------------

void drawSkyline()
{
    // Distant city buildings
    float buildings[][4] = {
        {0, 240, 65, 340},
        {65, 240, 125, 390},
        {125, 240, 185, 320},
        {185, 240, 245, 370},
        {245, 240, 310, 345},
        {310, 240, 375, 400},
        {375, 240, 435, 330},
        {435, 240, 500, 375},
        {500, 240, 560, 335},
        {560, 240, 620, 390},
        {620, 240, 680, 345},
        {680, 240, 745, 380},
        {745, 240, 800, 330}
    };

    for (int i = 0; i < 13; i++)
    {
        float r = 0.48f + (i % 3) * 0.06f;
        float g = 0.53f + (i % 2) * 0.05f;
        float b = 0.68f + (i % 3) * 0.04f;

        glColor3f(r, g, b);

        drawRect(buildings[i][0], buildings[i][1],
                 buildings[i][2], buildings[i][3]);

        // Building windows
        glColor3f(0.85f, 0.88f, 0.95f);

        for (float x = buildings[i][0] + 12;
             x < buildings[i][2] - 8; x += 20)
        {
            for (float y = 255;
                 y < buildings[i][3] - 15; y += 25)
            {
                drawRect(x, y, x + 8, y + 12);
            }
        }
    }
}

// ---------------- FERRIS WHEEL ----------------

void drawFerrisWheel()
{
    float cx = 635;
    float cy = 365;
    float radius = 105;

    // Supporting legs
    glColor3f(0.35f, 0.25f, 0.35f);

    glLineWidth(9);
    glBegin(GL_LINES);
        glVertex2f(cx, cy);
        glVertex2f(580, 245);

        glVertex2f(cx, cy);
        glVertex2f(690, 245);
    glEnd();

    // Outer wheel
    glColor3f(0.85f, 0.2f, 0.35f);
    glLineWidth(6);

    drawCircle(cx, cy, radius);

    // Inner wheel
    glColor3f(0.95f, 0.7f, 0.2f);
    glLineWidth(3);
    drawCircle(cx, cy, radius - 12);

    // Spokes and passenger cabins
    float colors[8][3] = {
        {1.0f, 0.2f, 0.2f},
        {1.0f, 0.6f, 0.1f},
        {0.2f, 0.7f, 1.0f},
        {0.3f, 0.9f, 0.4f},
        {0.8f, 0.3f, 1.0f},
        {1.0f, 0.4f, 0.7f},
        {0.9f, 0.8f, 0.1f},
        {0.2f, 0.8f, 0.8f}
    };

    for (int i = 0; i < 8; i++)
    {
        float angle = 2 * PI * i / 8;

        float x = cx + cos(angle) * radius;
        float y = cy + sin(angle) * radius;

        glColor3f(0.95f, 0.85f, 0.35f);

        glBegin(GL_LINES);
            glVertex2f(cx, cy);
            glVertex2f(x, y);
        glEnd();

        // Cabin
        glColor3f(colors[i][0], colors[i][1], colors[i][2]);
        drawRect(x - 11, y - 12, x + 11, y + 8);

        glColor3f(0.25f, 0.2f, 0.25f);
        drawRect(x - 11, y + 7, x + 11, y + 10);
    }

    // Center hub
    glColor3f(0.95f, 0.8f, 0.2f);
    drawCircle(cx, cy, 13);
}

// ---------------- TREES ----------------

void drawTree(float x, float y, float size)
{
    // Trunk
    glColor3f(0.4f, 0.22f, 0.1f);
    drawRect(x - 8 * size, y,
             x + 8 * size, y + 55 * size);

    // Leaves
    glColor3f(0.1f, 0.45f, 0.17f);
    drawCircle(x, y + 75 * size, 28 * size);

    glColor3f(0.15f, 0.58f, 0.2f);
    drawCircle(x - 18 * size, y + 65 * size, 23 * size);
    drawCircle(x + 18 * size, y + 65 * size, 23 * size);
    drawCircle(x, y + 95 * size, 23 * size);
}

// ---------------- PLANTS ----------------

void drawPlant(float x, float y)
{
    glColor3f(0.1f, 0.4f, 0.1f);

    glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(x, y + 22);

        glVertex2f(x, y + 10);
        glVertex2f(x - 10, y + 18);

        glVertex2f(x, y + 13);
        glVertex2f(x + 11, y + 21);
    glEnd();

    glColor3f(1.0f, 0.35f, 0.55f);
    drawCircle(x - 10, y + 18, 4);
    drawCircle(x + 11, y + 21, 4);

    glColor3f(1.0f, 0.85f, 0.2f);
    drawCircle(x, y + 22, 4);
}

// ---------------- STREETLAMPS ----------------

void drawStreetLamp(float x, float y)
{
    glColor3f(0.2f, 0.2f, 0.25f);
    glLineWidth(5);

    glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(x, y + 115);
        glVertex2f(x, y + 115);
        glVertex2f(x + 18, y + 115);
    glEnd();

    // Lamp
    glColor3f(1.0f, 0.9f, 0.4f);
    drawCircle(x + 18, y + 108, 9);

    glColor3f(0.3f, 0.3f, 0.3f);
    drawRect(x + 8, y + 118, x + 28, y + 123);
}

// ---------------- FENCING ----------------

void drawFence()
{
    glColor3f(0.45f, 0.25f, 0.15f);
    glLineWidth(4);

    // Horizontal rails
    glBegin(GL_LINES);
        glVertex2f(0, 55);
        glVertex2f(800, 55);

        glVertex2f(0, 90);
        glVertex2f(800, 90);
    glEnd();

    // Fence posts
    for (int x = 0; x <= 800; x += 35)
    {
        glBegin(GL_LINES);
            glVertex2f(x, 35);
            glVertex2f(x, 110);
        glEnd();

        drawCircle(x, 110, 4);
    }
}

// ---------------- CLOUDS ----------------

void drawCloud(float x, float y)
{
    glColor3f(1, 1, 1);

    drawCircle(x, y, 23);
    drawCircle(x + 25, y + 10, 28);
    drawCircle(x + 55, y, 23);
    drawCircle(x + 28, y - 8, 20);
}

// ---------------- DISPLAY ----------------

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Sky
    glColor3f(0.45f, 0.75f, 1.0f);
    drawRect(0, 240, 800, 600);

    // Clouds
    drawCloud(100, 480);
    drawCloud(330, 520);
    drawCloud(500, 455);

    // City skyline
    drawSkyline();

    // Ground
    glColor3f(0.25f, 0.65f, 0.25f);
    drawRect(0, 0, 800, 240);

    // Pathway
    glColor3f(0.72f, 0.65f, 0.52f);
    drawRect(0, 100, 800, 155);

    // Ferris wheel on right
    drawFerrisWheel();

    // Decorative trees
    drawTree(55, 155, 0.8f);
    drawTree(220, 155, 0.7f);
    drawTree(420, 155, 0.8f);
    drawTree(755, 155, 0.65f);

    // Streetlamps
    drawStreetLamp(120, 155);
    drawStreetLamp(330, 155);
    drawStreetLamp(515, 155);
    drawStreetLamp(735, 155);

    // Plants
    for (int x = 20; x < 800; x += 45)
    {
        drawPlant(x, 165);
    }

    // Foreground fence
    drawFence();

    glutSwapBuffers();
}

// ---------------- RESHAPE ----------------

void reshape(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 800, 0, 600, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ---------------- MAIN ----------------

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);

    glutCreateWindow("Interactive Amusement Park");

    glClearColor(1, 1, 1, 1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    glutMainLoop();

    return 0;
}
