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
