#ifndef GL_FUNCTIONS_DEF
#define GL_FUNCTIONS_DEF

#include <GL/gl.h>
#include <GL/glext.h>

#define FOR_GL_FUNCTIONS(X)\
    X(glBegin, begin)\
    X(glClear, clear)\
    X(glClearColor, clearColor)\
    X(glColor3f, color3f)\
    X(glEnd, end)\
    X(glVertex2f, vertex2f)

#endif // GL_FUNCTIONS_DEF
