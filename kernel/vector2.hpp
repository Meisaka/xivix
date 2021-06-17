/* ***
 * vector2.hpp - 2D point and vector operations.
 * Copyright (c) 2014-2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef VECTOR2_HAI
#define VECTOR2_HAI

#include <stdint.h>

namespace xiv {

template<class T>
struct vector2 {
	T x;
	T y;
	vector2() : x(0), y(0) {}
	vector2(T init_x, T init_y) : x(init_x), y(init_y) {}
	vector2(const vector2<T> &r) = default;
	vector2(vector2<T> &&r) = default;
	vector2<T>& operator=(const vector2<T> &r) = default;
	vector2<T>& operator=(vector2<T> &&r) = default;
	vector2<T>& operator=(const T &r) {
		x = r;
		y = r;
		return *this;
	}
	vector2<T>& operator+=(const vector2<T> &r) {
		x += r.x;
		y += r.y;
		return *this;
	}
	vector2<T>& operator-=(const vector2<T> &r) {
		x -= r.x;
		y -= r.y;
		return *this;
	}
	vector2<T>& operator*=(const vector2<T> &r) {
		x *= r.x;
		y *= r.y;
		return *this;
	}
	vector2<T>& operator/=(const vector2<T> &r) {
		x /= r.x;
		y /= r.y;
		return *this;
	}
	vector2<T>& operator+=(const T &r) {
		x += r;
		y += r;
		return *this;
	}
	vector2<T>& operator-=(const T &r) {
		x -= r;
		y -= r;
		return *this;
	}
	vector2<T>& operator*=(const T &r) {
		x *= r;
		y *= r;
		return *this;
	}
	vector2<T>& operator/=(const T &r) {
		x /= r;
		y /= r;
		return *this;
	}
	vector2<T> operator+(const vector2<T> &r) const {
		return vector2<T>(x, y) += r;
	}
	vector2<T> operator-(const vector2<T> &r) const {
		return vector2<T>(x, y) -= r;
	}
	vector2<T> operator-() const {
		return vector2<T>(-x, -y);
	}
	vector2<T> operator*(const vector2<T> &r) const {
		return vector2<T>(x, y) *= r;
	}
	vector2<T> operator/(const vector2<T> &r) const {
		return vector2<T>(x, y) /= r;
	}
	vector2<T> operator*(const T &r) const {
		return vector2<T>(x, y) *= r;
	}
	vector2<T> operator/(const T &r) const {
		return vector2<T>(x, y) /= r;
	}
	vector2<T> operator>>(int r) const {
		return vector2<T>(x >> r, y >> r);
	}
	bool operator==(const vector2<T> &r) const {
		return (x == r.x) && (y == r.y);
	}
	bool operator!=(const vector2<T> &r) const {
		return (x != r.x) || (y != r.y);
	}
	bool operator==(const T &r) const {
		return (x == r) && (y == r);
	}
	static vector2<T> center_align(const vector2<T> &sz, const vector2<T> &in) {
		return (in >> 1) - (sz >> 1);
	}
};

typedef vector2<uint32_t> uvec2;

}

#endif

