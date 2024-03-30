#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdlib.h>

void init(void) {
    glClearColor(1.0, 1.0, 1.0, 1.0);
}

void drawBitmapText(char* string, float x, float y, float z) {
    char* c;
    glRasterPos2f(x, y);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);
    }
}

void draw(GLfloat ctrlpoints[4][3]) {
    glShadeModel(GL_FLAT);
    glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, &ctrlpoints[0][0]);
    glEnable(GL_MAP1_VERTEX_3);
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 30; i++)
        glEvalCoord1f((GLfloat)i / 30.0);
    glEnd();
    glFlush();
}

void display(void) {
    // ... (previous control points remain unchanged)

    // Lissajous curve control points
    GLfloat lissajousCtrlPoints[4][3] = {
        {0.0, 0.0, 0.0},
        {0.5, 0.5, 0.0},
        {0.5, -0.5, 0.0},
        {0.0, 0.0, 0.0}
    };
    draw(lissajousCtrlPoints);

    // Additional segments of the Lissajous curve
    // You can experiment with different control points to create variations
    // ...

    glFlush();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Lissajous Curve");
    init();
    glutDisplayFunc(display);

    // Usage instructions
    printf("Usage: %s\n", argv[0]);
    printf("Press 'q' to quit.\n");

    glutMainLoop();
    return 0;
}
