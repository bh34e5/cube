#version 330 core

uniform mat4 perspective;

uniform float camera_translate;
uniform mat4 camera_rotation;

in vec3 a_vert_position;
in float a_vert_tex_coord;
in vec3 a_cube_offset;
in vec3 a_cube_rotation;

out float f_tex;
out float f_layer;

mat4 xRotMat(float theta) {
  return mat4(
    1, 0,           0,           0,
    0, +cos(theta), +sin(theta), 0,
    0, -sin(theta), +cos(theta), 0,
    0, 0,           0,           1
  );
}

mat4 yRotMat(float theta) {
  return mat4(
    +cos(theta), 0, -sin(theta), 0,
    0,           1, 0,           0,
    +sin(theta), 0, +cos(theta), 0,
    0,           0, 0,           1
  );
}

mat4 zRotMat(float theta) {
  return mat4(
    +cos(theta), +sin(theta), 0, 0,
    -sin(theta), +cos(theta), 0, 0,
    0,           0,           1, 0,
    0,           0,           0, 1
  );
}

mat4 translationMat(vec3 translation) {
  mat4 mat = mat4(1.0);
  mat[3].xyz = translation;

  return mat;
}

void main() {
  mat4 x_rot = xRotMat(a_cube_rotation.x);
  mat4 y_rot = yRotMat(a_cube_rotation.y);
  mat4 z_rot = zRotMat(a_cube_rotation.z);
  mat4 tx = translationMat(a_cube_offset);

  vec3 cam_tx = vec3(0.0, 0.0, -camera_translate);
  mat4 camera_mat = translationMat(cam_tx) * camera_rotation;

  vec4 world = tx * x_rot * y_rot * z_rot * vec4(a_vert_position, 1.0);
  vec4 screen = perspective * camera_mat * world;

  gl_Position = screen;

  f_tex = (a_vert_tex_coord + 0.5) / 6.0;
  f_layer = gl_InstanceID;
}
