#pragma once

#include <iostream>

#include "raylib.h"

inline std::ostream& operator<<(std::ostream& os, Vector2 v) {
    os << v.x << ' ' <<  v.y;
    return os;
}

inline bool operator== (const Vector2& v1, const Vector2& v2) {
    return v1.x == v2.x && v1.y == v2.y;
}

inline Vector2 operator-(const Vector2& v)  {
    return Vector2{-v.x, -v.y};
}

inline Vector3 operator- (const Vector3& v) {
    return Vector3{-v.x, -v.y, -v.z};
}

inline Vector2 operator+ (Vector2 v1, Vector2 v2) {
    return { v1.x + v2.x, v1.y + v2.y };
}

inline Vector3 operator+ (const Vector3& v1, const Vector3& v2) {
    return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}

inline Vector3 operator- (const Vector3& v1, const Vector3& v2) {
    return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

inline Vector3 operator+ (const Vector3& v1, float val) {
    return { v1.x + val, v1.y + val, v1.z + val,};
}

inline Vector2 operator- (Vector2 v1, Vector2 v2) {
    return { v1.x - v2.x, v1.y - v2.y };
}

inline Vector2 operator- (Vector2 v) {
    return { -v.x, -v.y };
}

inline void operator+=(Vector2& v1, Vector2 v2) {
    v1.x += v2.x;
    v1.y += v2.y;
}

inline void operator-=(Vector2& v1, Vector2 v2) {
    v1.x -= v2.x;
    v1.y -= v2.y;
}

inline Vector2 operator* (Vector2 v, const float& coeff) {
    return { coeff * v.x, coeff * v.y };
}

inline Vector2 operator* (const float& coeff, Vector2 v) {
    return v * coeff;
}

inline Vector2 operator/ (Vector2 v, const float& coeff) {
    return { v.x / coeff, v.y / coeff };
}

inline Vector2 operator/ (const float& coeff, Vector2 v) {
    return v / coeff;
}

inline Vector2 operator* (Vector2 v1, Vector2 v2) {
    return { v1.x * v2.x, v1.y * v2.y };
}

inline Vector2 operator/ (Vector2 v1, Vector2 v2) {
    return { v1.x / v2.x, v1.y / v2.y };
}

inline Vector3 operator* (const Vector3& v, const float& coeff) {
    return { coeff * v.x, coeff * v.y, coeff * v.z };
}

inline Vector3 operator* (const float& coeff, const Vector3& v) {
    return { coeff * v.x, coeff * v.y, coeff * v.z };
}

inline Vector3 operator* (Vector3 v1, Vector3 v2) {
    return { v1.x * v2.x, v1.y * v2.y, v1.z * v2.z };
}

inline Vector3 operator/ (Vector3 v, const float& coeff) {
    return { v.x / coeff, v.y / coeff, v.z / coeff };
}

inline Vector3 operator/ (const float& coeff, Vector3 v) {
    return { v.x / coeff, v.y / coeff, v.z / coeff };
}

inline Vector3 operator/ (Vector3 v1, Vector3 v2) {
    return { v1.x / v2.x, v1.y / v2.y, v1.z / v2.z };
}

inline std::ostream& operator<<(std::ostream& os, const Vector3& v) {
    os << v.x << ' ' <<  v.y << ' ' <<  v.z;
    return os;
}

inline bool operator== (const Vector3& v1, const Vector3& v2) {
    return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

inline bool operator== (const Quaternion& q1, const Quaternion& q2) {
    return q1.x == q2.x && q1.y == q2.y && q1.z == q2.z && q1.w == q2.w;
}

inline std::ostream& operator<<(std::ostream& os, const Matrix& m) {
    os << m.m0 << ' ' << m.m4 << ' ' << m.m8 << ' ' << m.m12 << '\n'
       << m.m1 << ' ' << m.m5 << ' ' << m.m9 << ' ' << m.m13 << '\n'
       << m.m2 << ' ' << m.m6 << ' ' << m.m10 << ' ' << m.m14 << '\n'
       << m.m3 << ' ' << m.m7 << ' ' << m.m11 << ' ' << m.m15;
    return os;
}