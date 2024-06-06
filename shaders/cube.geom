#version 420 core

in vec3 tex[];

flat out int face_num;
out vec3 tex2;

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

void write_out(int i, int fn)
{
  face_num = fn;
  gl_Position = gl_in[i].gl_Position;
  EmitVertex();
}

void main()
{
  int fn;
  vec3 xross = cross(tex[2] - tex[0], tex[1] - tex[0]);

  if (xross.x > 0) {
    fn = 5;
  } else if (xross.x < 0) {
    fn = 2;
  } else if (xross.y > 0) {
    fn = 3;
  } else if (xross.y < 0) {
    fn = 1;
  } else if (xross.z > 0) {
    fn = 0;
  } else if (xross.z < 0) {
    fn = 4;
  }

  write_out(0, fn);
  write_out(1, fn);
  write_out(2, fn);

  EndPrimitive();
}
