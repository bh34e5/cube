#version 330 core

uniform mat4 camera;

in vec3 a_vert_position;
in float a_vert_tex_coord;
in vec3 a_cube_offset;
in vec3 a_cube_rotation;

out float f_tex;

mat3 xRotMat(float theta) {
  return mat3(
    1, 0,           0,
    0, +cos(theta), +sin(theta),
    0, -sin(theta), +cos(theta)
  );
}

mat3 yRotMat(float theta) {
  return mat3(
    +cos(theta), 0, -sin(theta),
    0,           1, 0,
    +sin(theta), 0, +cos(theta)
  );
}

mat3 zRotMat(float theta) {
  return mat3(
    +cos(theta), +sin(theta), 0,
    -sin(theta), +cos(theta), 0,
    0,           0,           1
  );
}

void main() {
  mat3 x_rot = xRotMat(a_cube_rotation.x);
  mat3 y_rot = yRotMat(a_cube_rotation.y);
  mat3 z_rot = zRotMat(a_cube_rotation.z);

  vec3 rotated = x_rot * y_rot * z_rot * a_vert_position;
  vec3 shifted = rotated + a_cube_offset;

  vec4 world = vec4(shifted, 1.0);
  vec4 screen = camera * world;

  gl_Position = screen;

  f_tex = a_vert_tex_coord;
}
