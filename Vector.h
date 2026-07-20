#ifndef VECTOR_H
#define VECTOR_H

#include <iostream>
#include <cassert>
#include <cmath>
#include <map>
#include "Constants.h"
#include "Texture.h"

/// <summary>
/// 提供三维图形计算所需的基础数学类型与扫描线插值算法，包括二维/三维/四维向量、顶点结构以及带透视校正的扫描线插值工具。
/// </summary>

struct Vector2 {
    double x, y;
    Vector2(double _x = 0, double _y = 0) : x(_x), y(_y) {}
    Vector2 operator+(const Vector2& o) const { return { x + o.x, y + o.y }; }
    Vector2& operator+=(const Vector2& o) { x += o.x; y += o.y; return *this; }
    Vector2 operator-(const Vector2& o) const { return { x - o.x, y - o.y }; }
    Vector2& operator-=(const Vector2& o) { x -= o.x; y -= o.y; return *this; }
    Vector2 operator*(double s) const { return { x * s, y * s }; }
    Vector2& operator*=(double s) { x *= s; y *= s; return *this; }
    Vector2 operator/(double s) const { return { x / s, y / s }; }
    Vector2& operator/=(double s) { x /= s; y /= s; return *this; }
    double dot(const Vector2& v) const { return x * v.x + y * v.y; }
};
inline Vector2 operator*(double s, const Vector2& v) {
    return Vector2(v.x * s, v.y * s);
}

struct Vector3 {
    double x, y, z;
    double ratio;       //1/w = 1/(1 - v.z / d)
    Vector3(double _x = 0, double _y = 0, double _z = 0, double r = 1) : x(_x), y(_y), z(_z), ratio(r) {}
    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3& operator+=(const Vector3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3& operator-=(const Vector3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vector3 operator-() const { return { -x, -y, -z, -ratio }; }
    Vector3 operator*(double s) const { return { x * s, y * s, z * s }; }
    Vector3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    Vector3 operator/(double s) const { return { x / s, y / s, z / s }; }
    Vector3& operator/=(double s) { x /= s; y /= s; z /= s; return *this; }
    double& operator[](int i) {
        return i == 0 ? x : (i == 1 ? y : z);
    }
    const double& operator[](int i) const {
        return i == 0 ? x : (i == 1 ? y : z);
    }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vector3 normalized() const {
        double len = length();
        return (len < EPSILON) ? Vector3(0, 0, 0) : Vector3(x / len, y / len, z / len);
    }
    double dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vector3 cross(const Vector3& o) const {
        return Vector3(
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        );
    }
};
inline Vector3 operator*(double s, const Vector3& v) {
    return Vector3(v.x * s, v.y * s, v.z * s);
}

struct Vector4 {
    double x, y, z, w;
    Vector4(double _x = 0, double _y = 0, double _z = 0, double _w = 0) : x(_x), y(_y), z(_z), w(_w) {}
    Vector4 operator*(double s) const { return{ x * s, y * s, z * s, w * s }; }
    Vector4& operator*=(double s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vector4 operator/(double s) const { return{ x / s, y / s, z / s, w / s }; }
    Vector4& operator/=(double s) { x /= s; y /= s; z /= s; w /= s; return *this; }
};
inline ostream& operator<< (ostream& os, const Vector2& v) {
    os << "Vector2(" << v.x << ", " << v.y << ")";
    return os;
}
inline ostream& operator<< (ostream& os, const Vector3& v) {
    os << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
    os << " [ratio: " << v.ratio << "]";
    return os;
}
inline ostream& operator<< (ostream& os, const Vector4& v) {
    os << "Vector4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
    return os;
}

struct Vertex {
    Vector3 point;   // 世界空间坐标  
    Vector3 projectedPoint;    // 视图空间
    Vector2 uvPos;        // 纹理坐标

    Vector3 normal;     // 点的世界法向

    Color_my color;              // 顶点颜色
    Vertex(Vector3 p = { 0,0,0 }) :point(p) {}
    Vertex(Vector3 p, Vector3 pp, Vector3 n = { 0,0,0 }, Color_my c = { 0,0,0 }) :point(p), projectedPoint(pp), normal(n), color(c) {}
    Vertex(Vector3 p, Vector3 pp, Vector2 u, Vector3 n = { 0,0,0 }, Color_my c = { 0,0,0 }) :point(p), projectedPoint(pp), uvPos(u), normal(n), color(c) {}
};

struct ScanlineNodeV {
    double x;       // 屏幕 x
    Vertex cur;     // 顶点属性，存储 pos*ratio, normal*ratio, uv*ratio
    double r;       // ratio = 1/w
    double z_over_w; // cur.point.z (已经乘 ratio)
};

template<typename FragmentFunc>
void ScanlineInterpolation(const vector<Vertex>& vertices, FragmentFunc fragFunc) {
    assert(vertices.size() >= 3);
    int n = vertices.size();

    // 1. 构建边表
    map<int, vector<ScanlineNodeV>> edgeTable;
    for (int i = 0; i < n; i++) {
        const Vertex& v0 = vertices[i];
        const Vertex& v1 = vertices[(i + 1) % n];

        double y0 = v0.projectedPoint.y;
        double y1 = v1.projectedPoint.y;
        if (fabs(y0 - y1) < EPSILON) continue; // 水平边跳过

        const Vertex* top = &v0;
        const Vertex* bottom = &v1;
        if (y0 > y1) swap(top, bottom);

        double dy = bottom->projectedPoint.y - top->projectedPoint.y;
        int yStartEdge = (int)ceil(top->projectedPoint.y);
        int yEndEdge = (int)floor(bottom->projectedPoint.y);

        for (int y = yStartEdge; y <= yEndEdge; ++y) {
            double t = (y - top->projectedPoint.y) / dy;

            Vertex cur;
            cur.point = top->point * top->projectedPoint.ratio +
                (bottom->point * bottom->projectedPoint.ratio - top->point * top->projectedPoint.ratio) * t;
            cur.normal = top->normal * top->projectedPoint.ratio +
                (bottom->normal * bottom->projectedPoint.ratio - top->normal * top->projectedPoint.ratio) * t;
            cur.uvPos = top->uvPos * top->projectedPoint.ratio +
                (bottom->uvPos * bottom->projectedPoint.ratio - top->uvPos * top->projectedPoint.ratio) * t;
            cur.projectedPoint.x = top->projectedPoint.x + t * (bottom->projectedPoint.x - top->projectedPoint.x);
            cur.projectedPoint.y = y;

            double r = top->projectedPoint.ratio + t * (bottom->projectedPoint.ratio - top->projectedPoint.ratio);
            double z_over_w = cur.point.z;

            edgeTable[y].push_back({ cur.projectedPoint.x, cur, r, z_over_w });
        }
    }

    // 2. 遍历扫描线填充
    for (auto& [y, inter] : edgeTable) {
        if (inter.size() < 2) continue;

        // 按 x 排序
        sort(inter.begin(), inter.end(), [](auto& a, auto& b) { return a.x < b.x; });

        for (size_t k = 0; k + 1 < inter.size(); k += 2) {
            auto& L = inter[k];
            auto& R = inter[k + 1];

            int xStart = (int)ceil(L.x);
            int xEnd = (int)floor(R.x);

            for (int x = xStart; x <= xEnd; ++x) {
                double t_x = (R.x == L.x) ? 0.0 : (x - L.x) / (R.x - L.x);

                double r = L.r + (R.r - L.r) * t_x;
                if (fabs(r) < EPSILON) continue;

                Vertex cur;
                cur.point = L.cur.point + (R.cur.point - L.cur.point) * t_x;
                cur.normal = (L.cur.normal + (R.cur.normal - L.cur.normal) * t_x).normalized();
                cur.uvPos = L.cur.uvPos + (R.cur.uvPos - L.cur.uvPos) * t_x;

                // 透视修正
                cur.point /= r;
                cur.normal /= r;
                cur.uvPos /= r;
                double Z = (L.z_over_w + (R.z_over_w - L.z_over_w) * t_x) / r;

                fragFunc(x, y, Z, cur);
            }
        }
    }
}

struct ScanlineNodeP {
    double x;       // 屏幕 x
    double y;       // 当前扫描线 y
    double z_over_w; // 透视修正 z
    double r;       // ratio = 1/w
};

template<typename FragmentFunc>
void ScanlineInterpolation(const vector<Vector3>& points, FragmentFunc fragFunc) {
    assert(points.size() >= 3);
    int n = points.size();

    map<int, vector<ScanlineNodeP>> edgeTable;

    // 1. 构建边表
    for (int i = 0; i < n; ++i) {
        const Vector3& v0 = points[i];
        const Vector3& v1 = points[(i + 1) % n];

        double y0 = v0.y;
        double y1 = v1.y;
        if (fabs(y0 - y1) < EPSILON) continue; // 水平边跳过

        const Vector3* top = &v0;
        const Vector3* bottom = &v1;
        if (y0 > y1) swap(top, bottom);

        double dy = bottom->y - top->y;
        int yStartEdge = (int)ceil(top->y);
        int yEndEdge = (int)floor(bottom->y);

        for (int y = yStartEdge; y <= yEndEdge; ++y) {
            double t = (y - top->y) / dy;
            double r = top->ratio + t * (bottom->ratio - top->ratio);

            double x = top->x + t * (bottom->x - top->x);
            double z_over_w = top->z * top->ratio + t * ((bottom->z * bottom->ratio) - (top->z * top->ratio));

            edgeTable[y].push_back({ x, (double)y, z_over_w, r });
        }
    }

    // 2. 遍历扫描线填充
    for (auto& [y, inter] : edgeTable) {
        if (inter.size() < 2) continue;

        sort(inter.begin(), inter.end(), [](const ScanlineNodeP& a, const ScanlineNodeP& b) { return a.x < b.x; });

        for (size_t k = 0; k + 1 < inter.size(); k += 2) {
            auto& L = inter[k];
            auto& R = inter[k + 1];

            int xStart = (int)ceil(L.x);
            int xEnd = (int)floor(R.x);

            for (int x = xStart; x <= xEnd; ++x) {
                double t_x = (R.x == L.x) ? 0.0 : (x - L.x) / (R.x - L.x);
                double r = L.r + (R.r - L.r) * t_x;
                if (fabs(r) < EPSILON) continue;

                double Z = (L.z_over_w + (R.z_over_w - L.z_over_w) * t_x) / r;

                fragFunc(x, y, Z);
            }
        }
    }
}

#endif