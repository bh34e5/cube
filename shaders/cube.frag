#version 420 core

in vec3 col;

out vec4 v4_frag_out;

void main()
{
  v4_frag_out = vec4(col, 1.0f);
}
