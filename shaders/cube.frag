#version 420 core

in vec3 tex;
in flat int face_num;

out vec4 v4_frag_out;

uniform sampler2D cube_texture;

float get_color_scale(vec2 tex_coord);
vec2 get_base(int fn);
vec2 get_v2_from_tex(vec3 t, int fn);
vec2 hor_flip(vec2 i);
vec2 ver_flip(vec2 i);

#define PI 3.14159265

uniform float side_count;

void main()
{
  vec2 tex_coord = get_v2_from_tex(tex, face_num) / 4 + get_base(face_num);
  float factor = get_color_scale(tex_coord);
  v4_frag_out = vec4(factor * texture(cube_texture, tex_coord).xyz, 1.0);
}

float get_color_scale(vec2 tex_coord) {
  vec2 foo = fract(tex_coord * side_count * 4.0);
  vec2 bar = abs(1.0 - 2.0 * foo);
  float m = max(bar.x, bar.y);

  return m > 0.9 ? 0.0 : 1.0;
}

vec2 get_base(int fn)
{
  switch (fn)
  {
    case 0: return vec2(0.25f, 0.00f); // white
    case 1: return vec2(0.25f, 0.25f); // red
    case 2: return vec2(0.50f, 0.25f); // blue
    case 3: return vec2(0.75f, 0.25f); // orange
    case 4: return vec2(0.00f, 0.25f); // green
    case 5: return vec2(0.25f, 0.50f); // yellow
  }
}

vec2 get_v2_from_tex(vec3 t, int fn)
{
  switch (fn)
  {
    case 0: return t.yx;
    case 1: return ver_flip(t.yz);
    case 2: return ver_flip(hor_flip(t.xz));
    case 3: return ver_flip(hor_flip(t.yz));
    case 4: return ver_flip(t.xz);
    case 5: return ver_flip(t.yx);
  }
}

vec2 hor_flip(vec2 i)
{
  return vec2(1.0f - i.x, i.y);
}

vec2 ver_flip(vec2 i)
{
  return vec2(i.x, 1.0f - i.y);
}
