#version 330 core

uniform sampler1D color_tex;

in float f_tex;

out vec4 color;

void main() {
  // Color = texture(color_tex, f_tex);
  color = vec4(1.0);
}
