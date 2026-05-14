#ifndef GL_FUNCTIONS_hh
#define GL_FUNCTIONS_hh

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#define FOR_GL_FUNCTIONS(X)\
    X(glGetError, getError)\
    X(glCullFace, cullFace)\
    X(glEnable, enable)\
    X(glFrontFace, frontFace)\
    X(glAttachShader, attachShader)\
    X(glCompileShader, compileShader)\
    X(glCreateShader, createShader)\
    X(glDeleteShader, deleteShader)\
    X(glGetShaderInfoLog, getShaderInfoLog)\
    X(glGetShaderiv, getShaderiv)\
    X(glShaderSource, shaderSource)\
    X(glCreateProgram, createProgram)\
    X(glDeleteProgram, deleteProgram)\
    X(glGetProgramInfoLog, getProgramInfoLog)\
    X(glGetProgramiv, getProgramiv)\
    X(glLinkProgram, linkProgram)\
    X(glUseProgram, useProgram)\
    X(glClear, clear)\
    X(glClearColor, clearColor)\
    X(glBindVertexArray, bindVertexArray)\
    X(glDeleteVertexArrays, deleteVertexArrays)\
    X(glGenVertexArrays, genVertexArrays)\
    X(glBindBuffer, bindBuffer)\
    X(glBufferData, bufferData)\
    X(glDeleteBuffers, deleteBuffers)\
    X(glGenBuffers, genBuffers)\
    X(glEnableVertexAttribArray, enableVertexAttribArray)\
    X(glGetAttribLocation, getAttribLocation)\
    X(glVertexAttribDivisor, vertexAttribDivisor)\
    X(glVertexAttribPointer, vertexAttribPointer)\
    X(glGetUniformLocation, getUniformLocation)\
    X(glUniformMatrix4fv, uniformMatrix4fv)\
    X(glDrawArraysInstanced, drawArraysInstanced)

#endif // GL_FUNCTIONS_hh
