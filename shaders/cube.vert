#version 420 core

layout (location = 0) in vec3 v3_pos;
layout (location = 1) in float in_face_num;
layout (location = 2) in float intersecting_triangle;

out vec3 tex;
out flat int face_num;
out float mouse_in;

struct ViewInformation
{
    vec3 x_dir;
    vec3 y_dir;
    vec3 cube_center;

    float screen_cube_ratio;
    vec2 screen_dims;
};

uniform ViewInformation view_information;

vec2 decompose(vec3 target, vec3 x_dir, vec3 y_dir);

void main()
{
  vec3 cube_center = view_information.cube_center;

  vec3 x_dir = view_information.x_dir;
  vec3 y_dir = view_information.y_dir;

  vec3 corner_pos = cube_center + v3_pos;

  float cube_center_dist_sq = dot(cube_center, cube_center);
  float numerator =
    view_information.screen_cube_ratio * cube_center_dist_sq;
  float corner_dot_center = dot(corner_pos, cube_center);
  float scale_factor = numerator / corner_dot_center;

  vec3 projected = scale_factor * corner_pos;

  vec2 components = decompose(
          projected,
          normalize(x_dir),
          normalize(y_dir));

  vec2 screen = components / normalize(view_information.screen_dims);

  float depth = dot(projected, cube_center) / cube_center_dist_sq;
  vec3 res = vec3(screen, depth);

  gl_Position = vec4(res, 1.0f);
  tex = 0.5f * (v3_pos + vec3(1.0f));
  face_num = int(in_face_num);
  mouse_in = intersecting_triangle;
}

vec2 decompose(vec3 target, vec3 x_dir, vec3 y_dir)
{
    float dotted_x = dot(target, x_dir);
    float dotted_y = dot(target, y_dir);

    return vec2(dotted_x, dotted_y);
}
