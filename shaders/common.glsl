mat4 rotFromQuaternion(vec4 q) {
  float r00 = 1 - 2 * (q.b * q.b + q.a * q.a);
  float r01 = 2 * (q.g * q.b - q.a * q.r);
  float r02 = 2 * (q.g * q.a + q.b * q.r);

  float r10 = 2 * (q.g * q.b + q.a * q.r);
  float r11 = 1 - 2 * (q.g * q.g + q.a * q.a);
  float r12 = 2 * (q.b * q.a - q.g * q.r);

  float r20 = 2 * (q.g * q.a - q.b * q.r);
  float r21 = 2 * (q.b * q.a + q.g * q.r);
  float r22 = 1 - 2 * (q.g * q.g + q.b * q.b);

  mat4 res = mat4(
    r00, r01, r02, 0,
    r10, r11, r12, 0,
    r20, r21, r22, 0,
    0,   0,   0,   1
  );
  return res;
}

mat4 translationMat(vec3 translation) {
  mat4 mat = mat4(1.0);
  mat[3].xyz = translation;

  return mat;
}
