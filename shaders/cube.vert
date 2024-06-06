#version 420 core

layout (location = 0) in vec3 v3_pos;

out vec3 tex;

struct ViewInformation
{
    vec3 x_dir;
    vec3 y_dir;
    vec3 cube_center;

    float screen_cube_ratio;
};

uniform ViewInformation view_information;

vec3 decompose(vec3 target, vec3 x_dir, vec3 y_dir, vec3 z_dir);

void main()
{
  vec3 cube_center = view_information.cube_center;

  vec3 x_dir = view_information.x_dir;
  vec3 y_dir = view_information.y_dir;
  vec3 z_dir = normalize(cube_center);

  vec3 corner_pos = cube_center + v3_pos;

  float scale_factor =
      (view_information.screen_cube_ratio * dot(z_dir, z_dir))
      / dot(corner_pos, z_dir);

  vec3 projected = scale_factor * corner_pos;

  vec3 components = decompose(
          projected,
          normalize(x_dir),
          normalize(y_dir),
          normalize(z_dir));

  vec3 res = normalize(components);

  gl_Position = vec4(res, 1.0f);
  tex = 0.5f * (v3_pos + vec3(1.0f));
}

vec3 decompose(vec3 target, vec3 x_dir, vec3 y_dir, vec3 z_dir) {
    float dotted_x = dot(target, x_dir);
    float dotted_y = dot(target, y_dir);
    float dotted_z = dot(target, z_dir);

    return vec3(dotted_x, dotted_y, dotted_z);
}
