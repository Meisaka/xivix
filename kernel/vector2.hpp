/* ***
 * vector2.hpp - 2D point and vector operations.
 * Copyright (C) 2014-2015  Meisaka Yukara
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 *     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * or visit: http://www.gnu.org/licenses/gpl-2.0.txt
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

