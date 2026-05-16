#version 330 core

uniform sampler1DArray color_tex;

in float f_tex;
in float f_layer;

out vec4 color;

void main() {
  color = texture(color_tex, vec2(f_tex, f_layer));
}
