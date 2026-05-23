#version 330 core

uniform mat4 perspective;

uniform float camera_translate;
uniform mat4 camera_rotation;

in vec3 a_vert_position;
in vec3 a_cube_offset;
in vec4 a_cube_rotation;
in vec3 a_color;

out vec3 v_color;

mat4 rotFromQuaternion(vec4 q);
mat4 translationMat(vec3 translation);

void main() {
  mat4 rot = rotFromQuaternion(a_cube_rotation);
  mat4 tx = translationMat(a_cube_offset);

  vec3 cam_tx = vec3(0.0, 0.0, -camera_translate);
  mat4 camera_mat = translationMat(cam_tx) * camera_rotation;

  vec4 hom = vec4(a_vert_position, 1.0);
  vec4 screen = perspective * camera_mat * tx * rot * hom;

  gl_Position = screen;
  v_color = a_color;
}
