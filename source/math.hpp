#pragma once

#include <cmath>

namespace math {

constexpr float pi = 3.14159265358979323846f;

inline float radians(float degrees) {
	return degrees * (pi / 180.0f);
}

struct Vec3 {
	float x, y, z;
};

struct Vec4 {
	float x, y, z, w;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(Vec3 v, float s) { return { v.x * s, v.y * s, v.z * s }; }
inline Vec3 operator*(float s, Vec3 v) { return v * s; }

inline float dot(Vec3 a, Vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(Vec3 a, Vec3 b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

inline float length(Vec3 v) {
	return std::sqrt(dot(v, v));
}

inline Vec3 normalize(Vec3 v) {
	const float len = length(v);
	return len > 0.0f ? v * (1.0f / len) : v;
}

struct Mat4 {
	float m[4][4];

	static Mat4 identity() {
		Mat4 result = {};
		for (int i = 0; i < 4; ++i) {
			result.m[i][i] = 1.0f;
		}
		return result;
	}

	static Mat4 rows(const float (&r)[4][4]) {
		Mat4 result = {};
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[col][row] = r[row][col];
			}
		}
		return result;
	}
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
	Mat4 result = {};
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) {
				sum += a.m[k][r] * b.m[c][k];
			}
			result.m[c][r] = sum;
		}
	}
	return result;
}

inline Vec4 operator*(const Mat4& a, Vec4 v) {
	const float in[4] = { v.x, v.y, v.z, v.w };
	float out[4] = {};
	for (int r = 0; r < 4; ++r) {
		for (int k = 0; k < 4; ++k) {
			out[r] += a.m[k][r] * in[k];
		}
	}
	return { out[0], out[1], out[2], out[3] };
}

inline Mat4 translate(Vec3 t) {
	Mat4 result = Mat4::identity();
	result.m[3][0] = t.x;
	result.m[3][1] = t.y;
	result.m[3][2] = t.z;
	return result;
}

inline Mat4 scale(Vec3 s) {
	Mat4 result = Mat4::identity();
	result.m[0][0] = s.x;
	result.m[1][1] = s.y;
	result.m[2][2] = s.z;
	return result;
}

inline Mat4 rotateX(float angle) {
	const float c = std::cos(angle), s = std::sin(angle);
	Mat4 result = Mat4::identity();
	result.m[1][1] = c;
	result.m[1][2] = s;
	result.m[2][1] = -s;
	result.m[2][2] = c;
	return result;
}

inline Mat4 rotateY(float angle) {
	const float c = std::cos(angle), s = std::sin(angle);
	Mat4 result = Mat4::identity();
	result.m[0][0] = c;
	result.m[0][2] = -s;
	result.m[2][0] = s;
	result.m[2][2] = c;
	return result;
}

inline Mat4 rotateZ(float angle) {
	const float c = std::cos(angle), s = std::sin(angle);
	Mat4 result = Mat4::identity();
	result.m[0][0] = c;
	result.m[0][1] = s;
	result.m[1][0] = -s;
	result.m[1][1] = c;
	return result;
}

inline Mat4 rotateEuler(Vec3 angles) {
	return rotateZ(angles.z) * rotateY(angles.y) * rotateX(angles.x);
}

inline Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
	const Vec3 z = normalize(target - eye); // тут вглубь
	const Vec3 x = normalize(cross(z, up)); // нормализирую вправо
	const Vec3 y = cross(z, x);             // нормализирую вниз

	return Mat4::rows({
		{ x.x, x.y, x.z, -dot(x, eye) },
		{ y.x, y.y, y.z, -dot(y, eye) },
		{ z.x, z.y, z.z, -dot(z, eye) },
		{ 0.0f, 0.0f, 0.0f, 1.0f },
	});
}

inline Mat4 orthographic(float l, float r, float t, float b, float n, float f) {
	return Mat4::rows({
		{ 2.0f / (r - l), 0.0f,           0.0f,           -(r + l) / (r - l) },
		{ 0.0f,           2.0f / (b - t), 0.0f,           -(b + t) / (b - t) },
		{ 0.0f,           0.0f,           1.0f / (f - n), -n / (f - n) },
		{ 0.0f,           0.0f,           0.0f,           1.0f },
	});
}

inline Mat4 frustumToBox(float n, float f) {
	return Mat4::rows({
		{ n,    0.0f, 0.0f,  0.0f },
		{ 0.0f, n,    0.0f,  0.0f },
		{ 0.0f, 0.0f, f + n, -f * n },
		{ 0.0f, 0.0f, 1.0f,  0.0f },
	});
}

inline Mat4 perspective(float fov_y, float aspect, float n, float f) {
	const float b = n * std::tan(fov_y * 0.5f);
	const float r = aspect * b;
	return orthographic(-r, r, -b, b, n, f) * frustumToBox(n, f);
}

}
