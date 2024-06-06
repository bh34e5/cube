#version 420 core

in vec3 col;
flat in int face_num;

out vec4 v4_frag_out;

void main()
{
  float fn = float(face_num + 2) / 8.0f;
  v4_frag_out = vec4(fn, fn, fn, 1.0f);
}
