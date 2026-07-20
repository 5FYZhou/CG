#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <GL/freeglut.h>
#include "Constants.h"
#include "Vector.h"
#include "Texture.h"
#include "matrix.h"
using namespace std;

/// <summary>
/// 定义2D/3D图形基类体系与具体形状（线、多边形、面、网格、金字塔），封装几何变换、线框绘制及面法线计算
/// </summary>

template<typename VecT>
class ShapeBase {
    public:
        Color_my colorWireframe;
        Color_my colorFill;
        virtual ~ShapeBase() {}
        virtual void Transform(const Matrix_my& M) = 0;
        virtual void DrawWireframe() = 0;


        virtual void Line_Bresenham(int x1, int y1, int x2, int y2)
        {
            int dx = abs(x2 - x1);
            int dy = abs(y2 - y1);
            int sx = (x2 >= x1) ? 1 : -1; // x方向增量符号
            int sy = (y2 >= y1) ? 1 : -1; // y方向增量符号
            int err = dx - dy;

            int x = x1, y = y1;

            while (true) {
                glVertex2i(x, y); // 绘制像素点

                if (x == x2 && y == y2) break; // 到达终点

                int e2 = 2 * err;
                if (e2 > -dy) { // 调整x方向
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) { // 调整y方向
                    err += dx;
                    y += sy;
                }
            }
        }
        virtual void Line_Bresenham(Vector2 v1, Vector2 v2)
        {
            int x1 = (int)(v1.x + 0.5), y1 = (int)(v1.y + 0.5), x2 = (int)(v2.x + 0.5), y2 = (int)(v2.y + 0.5);
            int dx = abs(x2 - x1);
            int dy = abs(y2 - y1);
            int sx = (x2 >= x1) ? 1 : -1; // x方向增量符号
            int sy = (y2 >= y1) ? 1 : -1; // y方向增量符号
            int err = dx - dy;

            int x = x1, y = y1;

            while (true) {
                glVertex2i(x, y); // 绘制像素点

                if (x == x2 && y == y2) break; // 到达终点

                int e2 = 2 * err;
                if (e2 > -dy) { // 调整x方向
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) { // 调整y方向
                    err += dx;
                    y += sy;
                }
            }
        }
        virtual void Line_Bresenham(Vector3 v1, Vector3 v2)
        {
            int x1 = (int)(v1.x + 0.5), y1 = (int)(v1.y + 0.5), x2 = (int)(v2.x + 0.5), y2 = (int)(v2.y + 0.5);
            int dx = abs(x2 - x1);
            int dy = abs(y2 - y1);
            int sx = (x2 >= x1) ? 1 : -1; // x方向增量符号
            int sy = (y2 >= y1) ? 1 : -1; // y方向增量符号
            int err = dx - dy;

            int x = x1, y = y1;

            while (true) {
                glVertex2i(x, y); // 绘制像素点

                if (x == x2 && y == y2) break; // 到达终点

                int e2 = 2 * err;
                if (e2 > -dy) { // 调整x方向
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) { // 调整y方向
                    err += dx;
                    y += sy;
                }
            }
        }
};

class Shape2D : public ShapeBase<Vector2> {
};

class Line2D : public Shape2D {
    public:
        Vector3 a, b;
        Line2D(const Vector2& p0, const Vector2& p1) :a({p0.x,p0.y,0}), b({p1.x,p1.y,0}) {}
        Line2D(const Vector3& p0 = { 0,0,0 }, const Vector3& p1 = { 0,0,0 }) :a(p0), b(p1) {}

        void Transform(const Matrix_my& M) override { 
            Vector2 tmpa = { a.x,a.y }, tmpb{ b.z,b.y };
            tmpa = ApplyTransformToVec2(M, tmpa); 
            tmpb = ApplyTransformToVec2(M, tmpb); 
            a.x = tmpa.x; a.y = tmpa.y;
            b.x = tmpb.x; b.y = tmpb.y;
        }

        void DrawWireframe() override {
            Line_Bresenham(a, b);
        }
};

class Polygon2D : public Shape2D {
    public:
        vector<Vector3> points;
        Color_my color;

        Polygon2D() {}
        Polygon2D(const vector<Vector2>& p, Color_my c = {0,0,0}) {
            for (auto& t : p) {
                points.push_back({ t.x,t.y,0 });
            }
        }
        Polygon2D(const vector<Vector3>& p, Color_my c = {0,0,0}): points(p), color(c) {}

        void Transform(const Matrix_my& M) override { 
            for (auto& v : points) {
                Vector2 tmp = { v.x,v.y };
                tmp = ApplyTransformToVec2(M, tmp);
                v.x = tmp.x; v.y = tmp.y;
            }
        }

        void DrawWireframe() override {
            //if (points.size() <= 2)return;
            int n = points.size();
            for (int i = 0; i < n; i++) {
                Line_Bresenham(points[i], points[(i + 1) % n]);
            }
        }
};

class Shape3D : public ShapeBase<Vector3> {
    public:
        virtual ~Shape3D() {}
        virtual void DrawWireframe()override {}
};

class Line3D : public Shape3D {
    public:
        Vector3 a, b;
        Vector3 aProjected, bProjected;
        vector<Vector3> clippedPoints;

        Line3D(const Vector3& p0 = { 0,0,0 }, const Vector3& p1 = { 0,0,0 }) : a(p0), b(p1) {}

        void Transform(const Matrix_my& M) override {
            a = ApplyTransformToVec3(M, a);
            b = ApplyTransformToVec3(M, b);
        }
        
        void DrawWireframe() override {
        }
};

class Face3D{
public:
    vector<int> indices;
    Vector3 normal;
    Vector3 center;
    Material mat;
    bool canBeSee;

    Color_my colorFill;
    Color_my colorWireframe;

    Face3D() { canBeSee = true; }
    Face3D(const vector<int>& p, const Material& m) :indices(p), mat(m) { canBeSee = true; }

    void ComputeFaceNormals(const vector<Vector3>& points) {
        Vector3 N = { 0, 0, 0 };
        int n = indices.size();

        for (int i = 0; i < n; i++) {
            const Vector3& cur = points[indices[i]];
            const Vector3& next = points[indices[(i + 1) % n]];

            N.x += (cur.y - next.y) * (cur.z + next.z);
            N.y += (cur.z - next.z) * (cur.x + next.x);
            N.z += (cur.x - next.x) * (cur.y + next.y);
        }
        normal = N.normalized();
        if (abs(normal.x) < EPSILON)normal.x = 0;
        if (abs(normal.y) < EPSILON)normal.y = 0;
        if (abs(normal.z) < EPSILON)normal.z = 0;
    }

    void ComputeFaceNormals(const vector<Vertex>& verts) {
        Vector3 N = { 0,0,0 };
        int n = indices.size();

        for (int i = 0; i < n; i++) {
            const Vector3& cur = verts[indices[i]].point;
            const Vector3& next = verts[indices[(i + 1) % n]].point;

            N.x += (cur.y - next.y) * (cur.z + next.z);
            N.y += (cur.z - next.z) * (cur.x + next.x);
            N.z += (cur.x - next.x) * (cur.y + next.y);
        }
        normal = N.normalized();
    }

    void ComputeCenter(const vector<Vector3>& points) {
        Vector3 c{ 0,0,0 };
        for (const auto& i : indices)
            c += points[i];
        center = c * (1.0f / indices.size());
    }
};

class Mesh3D : public Shape3D {
    public:
        vector<Vertex> points;
        vector<Face3D> faces;

        vector<Vertex> projectedPoints;
        vector<Face3D> projectedFaces;

        vector<Vertex> clippedPoints;
        vector<Face3D> clippedFaces;

        Mesh3D() {}
        Mesh3D(const vector<Vector3>p, const vector<Face3D>& fcs) :faces(fcs) {
            for (auto& pp : p) {
                points.push_back({ pp });
            }
            for (auto& f : faces) {
                f.ComputeFaceNormals(p);
                f.ComputeCenter(p);
                for (int vid : f.indices) {
                    points[vid].normal = points[vid].normal + f.normal;  // 累加
                }
            }
            for (auto& v : points) {
                v.normal = v.normal.normalized();
            }
        }
		Mesh3D(const vector<Vector3>p, const vector<Face3D>& fcs, const vector<Vector3> pn, const vector<Vector2> vt) :faces(fcs) {
			if (p.size() != pn.size() || p.size() != vt.size()) {
				throw invalid_argument("points size != point normal size() or texcoord size()");
				return;
			}
			for (size_t i = 0; i < p.size(); i++) {
				points.push_back({ p[i],pn[i],vt[i] });
			}
			for (auto& f : faces) {
				f.ComputeFaceNormals(p);
                f.ComputeCenter(p);
                for (int vid : f.indices) {
                    points[vid].normal = points[vid].normal + f.normal;  // 累加
                }
			}
			for (auto& v : points) {
				v.normal = v.normal.normalized();
			}
		}

        void Transform(const Matrix_my& M) override {
            Matrix_my normalMatrix = M.Inverse().Transpose();

            for (auto& p : points) {
                // 变换顶点位置
                p.point = ApplyTransformToVec3(M, p.point);
                // 变换顶点法线
                p.normal = ApplyTransformToVec3(normalMatrix, p.normal).normalized();
            }

            // 变换面中心，并重新计算面法线
            for (auto& f : faces) {
                f.center = ApplyTransformToVec3(M, f.center);
                f.normal = Vector3{ 0,0,0 };
                for (int vid : f.indices)
                    f.normal += points[vid].normal;
                f.normal = f.normal.normalized();
            }
        }
};

class Pyramid : public Mesh3D {
public:
    Pyramid(const vector<Vector3>& pts, const vector<Color_my>& colors) {
        /*
        faces = {
            Face3D({ 0,2,1 }, colors[0]), // 底面
            Face3D({ 0,1,3 }, colors[1]), // 侧面1
            Face3D({ 0,3,2 }, colors[2]), // 侧面2
            Face3D({ 1,2,3 }, colors[3])  // 侧面3
        };
        for (auto& f : faces) {
            f.ComputeFaceNormals(points);
        }*/
        // 1. 创建 Vertex（world 点）
        points.resize(pts.size());
        for (size_t i = 0; i < pts.size(); i++) {
            points[i].point = pts[i];
            points[i].normal = { 0,0,0 };   // 先置 0，稍后累加面法向
        }

        // 2. 构建 face（索引不变）
        faces = {
            Face3D({ 0,2,1 }, colors[0]), // 底面
            Face3D({ 0,1,3 }, colors[1]), // 侧面1
            Face3D({ 0,3,2 }, colors[2]), // 侧面2
            Face3D({ 1,2,3 }, colors[3])  // 侧面3
        };

        // 3. 计算所有面的法向（使用世界坐标）
        for (auto& f : faces) {
            f.ComputeFaceNormals(points);  // Face3D::ComputeFaceNormals(vector<Vertex>&)
        }

        // 4. 顶点法向 = 参与该顶点的所有面法向之和（光滑化）
        for (auto& f : faces) {
            for (int vid : f.indices) {
                points[vid].normal = points[vid].normal + f.normal;  // 累加
            }
        }

        // 5. 归一化每个顶点的法向
        for (auto& v : points) {
            v.normal = v.normal.normalized();
        }
    }
};

#endif