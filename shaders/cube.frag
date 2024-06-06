#version 420 core

in vec3 tex2;
flat in int face_num;

out vec4 v4_frag_out;

uniform sampler2D cube_texture;

vec2 get_base(int fn);

void main()
{
  v4_frag_out = texture(cube_texture, tex2.xy / 4 + get_base(face_num));
}

vec2 get_base(int fn) {
  switch (fn)
  {
    case 0: return vec2(0.00f, 0.25f);
    case 1: return vec2(0.25f, 0.00f);
    case 2: return vec2(0.25f, 0.25f);
    case 3: return vec2(0.25f, 0.50f);
    case 4: return vec2(0.25f, 0.75f);
    case 5: return vec2(0.00f, 0.00f);
  }
}
