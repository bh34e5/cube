#version 420 core

layout (location = 0) in vec3 v3_pos;
layout (location = 1) in vec3 i_col;
layout (location = 2) in vec3 i_tex;

out vec3 col;
out vec3 tex;

void main()
{
  gl_Position = vec4(v3_pos, 1.0f);
  col = i_col;
  tex = i_tex;
}
