#ifndef CLIPPER_H
#define CLIPPER_H

#include <vector>
#include "Constants.h"
#include "Vector.h"
#include "Texture.h"
#include "Graphics.h"

/// <summary>
/// 定义2D图形裁剪策略接口及Cohen-Sutherland（直线）、Sutherland-Hodgman（多边形/面）算法实现
/// </summary>

//裁剪策略接口
class IClipStrategy2D {
public:
    virtual ~IClipStrategy2D() {};
    virtual vector<Line2D> ClipLine(const Line2D& line, const Polygon2D& win) const = 0;
    virtual vector<Polygon2D> ClipPolygon(const Polygon2D& poly, const Polygon2D& win) const = 0;
    virtual void ClipLine(Line3D& line, const Polygon2D& win) const = 0;
    virtual void ClipMesh(Mesh3D& poly, const Polygon2D& win) const = 0;
};

#pragma region Cohen-Sutherland法裁剪直线
enum CSCode { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

inline int ComputeOutCode(double x, double y, double left, double right, double bottom, double top) {
    int code = INSIDE;
    if (x < left) code |= LEFT;
    else if (x > right) code |= RIGHT;
    if (y < bottom) code |= BOTTOM;
    else if (y > top) code |= TOP;
    return code;
}

inline void ExtractRectFromPolygon(const Polygon2D& poly, double& left, double& right, double& bottom, double& top) {
    if (poly.points.size() < 4) throw invalid_argument("Clip polygon must have >=4 points (rectangle)");
    left = right = poly.points[0].x; bottom = top = poly.points[0].y;
    for (const auto& p : poly.points) { left = min(left, p.x); right = max(right, p.x); bottom = min(bottom, p.y); top = max(top, p.y); }
}

class CohenSutherlandLineClip2D : public IClipStrategy2D {
public:
    vector<Line2D> ClipLine(const Line2D& line, const Polygon2D& window) const override {
        double left, right, bottom, top;
        ExtractRectFromPolygon(window, left, right, bottom, top);

        double x0 = line.a.x, y0 = line.a.y;
        double x1 = line.b.x, y1 = line.b.y;

        int out0 = ComputeOutCode(x0, y0, left, right, bottom, top);
        int out1 = ComputeOutCode(x1, y1, left, right, bottom, top);

        bool accept = false;
        while (true) {
            if ((out0 | out1) == 0) { accept = true; break; }
            else if (out0 & out1) break;
            else {
                double x, y;
                int out = out0 ? out0 : out1;
                if (out & TOP) { x = x0 + (x1 - x0) * (top - y0) / (y1 - y0); y = top; }
                else if (out & BOTTOM) { x = x0 + (x1 - x0) * (bottom - y0) / (y1 - y0); y = bottom; }
                else if (out & RIGHT) { y = y0 + (y1 - y0) * (right - x0) / (x1 - x0); x = right; }
                else { y = y0 + (y1 - y0) * (left - x0) / (x1 - x0); x = left; }

                if (out == out0) { x0 = x; y0 = y; out0 = ComputeOutCode(x0, y0, left, right, bottom, top); }
                else { x1 = x; y1 = y; out1 = ComputeOutCode(x1, y1, left, right, bottom, top); }
            }
        }

        if (accept)
            return { Line2D{Vector2{x0, y0}, Vector2{x1, y1}} };
        return {};
    }

    vector<Polygon2D> ClipPolygon(const Polygon2D& poly, const Polygon2D& window) const override { return {}; }

    void ClipLine(Line3D& line, const Polygon2D& win) const override {
        double left, right, bottom, top;
        ExtractRectFromPolygon(win, left, right, bottom, top);

        // ==== 注意：这里用 3D 原始值 ====
        double x0 = line.aProjected.x, y0 = line.aProjected.y;
        double z0 = line.a.z;
        double r0 = line.a.ratio;

        double x1 = line.bProjected.x, y1 = line.bProjected.y;
        double z1 = line.b.z;
        double r1 = line.b.ratio;

        int out0 = ComputeOutCode(x0, y0, left, right, bottom, top);
        int out1 = ComputeOutCode(x1, y1, left, right, bottom, top);

        bool accept = false;

        while (true) {
            if ((out0 | out1) == 0) {
                accept = true;
                break;
            }
            else if (out0 & out1) {
                break;
            }
            else {
                double x, y, t;
                int out = out0 ? out0 : out1;

                if (out & TOP) {
                    t = (top - y0) / (y1 - y0);
                    x = x0 + t * (x1 - x0);
                    y = top;
                }
                else if (out & BOTTOM) {
                    t = (bottom - y0) / (y1 - y0);
                    x = x0 + t * (x1 - x0);
                    y = bottom;
                }
                else if (out & RIGHT) {
                    t = (right - x0) / (x1 - x0);
                    y = y0 + t * (y1 - y0);
                    x = right;
                }
                else { // LEFT
                    t = (left - x0) / (x1 - x0);
                    y = y0 + t * (y1 - y0);
                    x = left;
                }

                // ====== ★★★ 关键：3D z 和 ratio 插值 ★★★ ======
                double z = z0 + t * (z1 - z0);
                double r = r0 + t * (r1 - r0);

                if (out == out0) {
                    x0 = x; y0 = y;
                    z0 = z; r0 = r;
                    out0 = ComputeOutCode(x0, y0, left, right, bottom, top);
                }
                else {
                    x1 = x; y1 = y;
                    z1 = z; r1 = r;
                    out1 = ComputeOutCode(x1, y1, left, right, bottom, top);
                }
            }
        }

        if (accept) {
            line.clippedPoints.push_back({ x0, y0, z0, r0 });
            line.clippedPoints.push_back({ x1, y1, z1, r1 });
        }
    }

    void ClipMesh(Mesh3D& mesh, const Polygon2D& window) const override {}
};
#pragma endregion

//Sutherland-Hodgman法
class SutherlandHodgman_PolygonClip2D : public IClipStrategy2D {
public:
    vector<Line2D> ClipLine(const Line2D& line, const Polygon2D& window) const override {
        Polygon2D temp{ {line.a, line.b} };
        auto result = ClipPolygon(temp, window);
        vector<Line2D> lines;
        if (!result.empty() && result[0].points.size() >= 2) {
            Vector3 d = line.b - line.a;
            auto& pts = result[0].points;
            sort(pts.begin(), pts.end(), [&](const Vector3& p1, const Vector3& p2) {
                return (p1.x - line.a.x) * d.x + (p1.y - line.a.y) * d.y <
                    (p2.x - line.a.x) * d.x + (p2.y - line.a.y) * d.y;
                });
            lines.push_back(Line2D{ pts.front(), pts.back() });
        }
        return lines;
    }

    vector<Polygon2D> ClipPolygon(const Polygon2D& poly, const Polygon2D& window) const override {
        const auto& clip = window.points;
        if (clip.size() < 3 || poly.points.empty()) return {};

        vector<Vector3> output = poly.points;

        auto inside = [&](const Vector3& a, const Vector3& b, const Vector3& p) -> bool {
            double cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
            return cross >= -EPSILON;
            };

        auto intersect = [&](const Vector3& a, const Vector3& b, const Vector3& p, const Vector3& q) -> Vector3 {
            double A1 = b.y - a.y, B1 = a.x - b.x, C1 = A1 * a.x + B1 * a.y;
            double A2 = q.y - p.y, B2 = p.x - q.x, C2 = A2 * p.x + B2 * p.y;
            double det = A1 * B2 - A2 * B1;
            if (fabs(det) < EPSILON) return a;
            return { (B2 * C1 - B1 * C2) / det, (A1 * C2 - A2 * C1) / det };
            };

        for (size_t i = 0; i < clip.size(); i++) {
            Vector3 A = clip[i], B = clip[(i + 1) % clip.size()];
            vector<Vector3> input = move(output);
            output.clear();
            if (input.empty()) break;

            Vector3 S = input.back();
            for (auto& E : input) {
                bool Ein = inside(A, B, E);
                bool Sin = inside(A, B, S);
                if (Ein) {
                    if (!Sin) output.push_back(intersect(S, E, A, B));
                    output.push_back(E);
                }
                else if (Sin) {
                    output.push_back(intersect(S, E, A, B));
                }
                S = E;
            }
        }

        if (output.empty()) return {};
        return { Polygon2D{output} };
    }

    void ClipLine(Line3D& line, const Polygon2D& win) const override {
        // 1. 构造两点多边形
        Polygon2D tempPoly{ {line.aProjected, line.bProjected} };

        // 2. 用现有 Sutherland-Hodgman 裁剪
        auto clippedPolys = ClipPolygon(tempPoly, win);
        if (clippedPolys.empty() || clippedPolys[0].points.size() < 2) {
            line.clippedPoints.clear();
            return;
        }

        auto& pts2D = clippedPolys[0].points;
        Vector3 dir = line.bProjected - line.aProjected;

        // 3. 对裁剪点排序（沿原线方向）
        sort(pts2D.begin(), pts2D.end(), [&](const Vector3& p1, const Vector3& p2) {
            double t1 = (fabs(dir.x) > fabs(dir.y)) ? (p1.x - line.aProjected.x) / dir.x
                : (p1.y - line.aProjected.y) / dir.y;
            double t2 = (fabs(dir.x) > fabs(dir.y)) ? (p2.x - line.aProjected.x) / dir.x
                : (p2.y - line.aProjected.y) / dir.y;
            return t1 < t2;
            });

        // 4. 生成裁剪后的线段，并进行 z / ratio 插值
        line.clippedPoints.clear();
        for (size_t i = 0; i + 1 < pts2D.size(); ++i) {
            const Vector3& p0 = pts2D[i];
            const Vector3& p1 = pts2D[i + 1];

            // 计算参数 t0, t1 对应 line 原始 3D 点
            auto computeT = [&](const Vector3& p) {
                double t;
                if (fabs(dir.x) > fabs(dir.y)) t = (p.x - line.aProjected.x) / dir.x;
                else t = (p.y - line.aProjected.y) / dir.y;
                return t;
                };

            double t0 = computeT(p0);
            double t1 = computeT(p1);

            auto interpolate3D = [&](double t) -> Vector3 {
                double r = line.a.ratio + t * (line.b.ratio - line.a.ratio);
                double x = line.a.x * line.a.ratio + t * (line.b.x * line.b.ratio - line.a.x * line.a.ratio);
                double y = line.a.y * line.a.ratio + t * (line.b.y * line.b.ratio - line.a.y * line.a.ratio);
                double z = line.a.z * line.a.ratio + t * (line.b.z * line.b.ratio - line.a.z * line.a.ratio);
                return { x / r, y / r, z / r, r };
                };

            line.clippedPoints.push_back(interpolate3D(t0));
            line.clippedPoints.push_back(interpolate3D(t1));
        }
    }

    void ClipMesh(Mesh3D& mesh, const Polygon2D& window) const override {
        mesh.clippedPoints.clear();
        mesh.clippedFaces.clear();

        const auto& clip = window.points;
        if (clip.size() < 3) return;
        auto inside = [&](const Vector3& a, const Vector3& b, const Vector3& p) -> bool {
            // 透视矫正：还原齐次坐标
            double px = p.x * p.ratio;
            double py = p.y * p.ratio;
            double ax = a.x * a.ratio;
            double ay = a.y * a.ratio;
            double bx = b.x * b.ratio;
            double by = b.y * b.ratio;

            double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            return cross >= -EPSILON;
            };

        // 计算投影空间 S->E 与 A->B 的交点参数 t 与投影坐标 out_pt
        auto computeIntersectionParam = [&](const Vertex& SS, const Vertex& EE, const Vector3& S, const Vector3& E, const Vector3& A, const Vector3& B, double& out_t, Vector3& out_pt) -> bool {
            double A1 = E.y - S.y, B1 = S.x - E.x, C1 = A1 * S.x + B1 * S.y;
            //double A1 = EE.point.y - SS.point.y, B1 = SS.point.x - EE.point.x, C1 = A1 * SS.point.x + B1 * SS.point.y;
            double A2 = B.y - A.y, B2 = A.x - B.x, C2 = A2 * A.x + B2 * A.y;
            double det = A1 * B2 - A2 * B1;
            if (fabs(det) < EPSILON) return false;
            double x = (B2 * C1 - B1 * C2) / det;
            double y = (A1 * C2 - A2 * C1) / det;
            //double t = (fabs(EE.point.x*EE.point.ratio - SS.point.x*SS.point.ratio) > fabs(EE.point.y * EE.point.ratio - SS.point.y * SS.point.ratio)) ? (x - SS.point.x * SS.point.ratio) / (EE.point.x * EE.point.ratio - SS.point.x * SS.point.ratio) : (y - SS.point.y * SS.point.ratio) / (EE.point.y * EE.point.ratio - SS.point.y * SS.point.ratio);
            double t = (fabs(E.x - S.x) > fabs(E.y - S.y)) ? (x - S.x) / (E.x - S.x) : (y - S.y) / (E.y - S.y);
            if (t < 0) t = 0; if (t > 1) t = 1;

            double rs = S.ratio, re = E.ratio;
            double r = rs + t * (re - rs);
            double xr = S.x * rs + t * (E.x * re - S.x * rs);
            double yr = S.y * rs + t * (E.y * re - S.y * rs);
            double zr = S.z * rs + t * (E.z * re - S.z * rs);

            if (fabs(r) < EPSILON) {
                out_pt = { xr, yr, zr, r };
            }
            else {
                out_pt = { xr / r, yr / r, zr / r, r };
            }
            out_t = t;
            return true;
            };

        // 对每个面单独处理：先把 face 的 projectedPoints（Vertex）拷贝到局部 workingVerts
        for (const Face3D& face : mesh.projectedFaces) {
            if (face.indices.empty()) continue;

            // 构建局部顶点数组（用于裁剪操作，保存完整 Vertex）
            vector<Vertex> workingVerts;
            workingVerts.reserve(face.indices.size());
            bool bad = false;
            for (int idx : face.indices) {
                if (idx < 0 || idx >= (int)mesh.projectedPoints.size()) { bad = true; break; }
                workingVerts.push_back(mesh.projectedPoints[idx]); // copy whole Vertex
            }
            if (bad || workingVerts.empty()) continue;

            // Sutherland-Hodgman：对 window 的每一条边裁剪 workingVerts
            for (size_t i = 0; i < clip.size(); ++i) {
                Vector3 A = clip[i];
                Vector3 B = clip[(i + 1) % clip.size()];

                vector<Vertex> input = move(workingVerts);
                workingVerts.clear();
                if (input.empty()) break;

                Vertex S = input.back();
                Vector3 Sproj = S.projectedPoint;

                for (const Vertex& E : input) {
                    Vector3 Eproj = E.projectedPoint;
                    bool Ein = inside(A, B, Eproj);
                    bool Sin = inside(A, B, Sproj);

                    if (Ein) {
                        if (!Sin) {
                            // S out, E in -> 插入交点（基于 S,E 插值产生 newV）
                            double t; Vector3 iproj;
                            if (computeIntersectionParam(S,E,Sproj, Eproj, A, B, t, iproj)) {
                                // 透视正确插值属性（world, normal, color 可按需）
                                double rs = Sproj.ratio, re = Eproj.ratio;
                                double r = rs + t * (re - rs);
                                if (fabs(r) < EPSILON) r = EPSILON;

                                Vertex newV;
                                newV.projectedPoint = iproj;

                                newV.point.x = (S.point.x * rs + t * (E.point.x * re - S.point.x * rs)) / r;
                                newV.point.y = (S.point.y * rs + t * (E.point.y * re - S.point.y * rs)) / r;
                                newV.point.z = (S.point.z * rs + t * (E.point.z * re - S.point.z * rs)) / r;
                                newV.point.ratio = r;

                                // 法线透视插值后归一化
                                newV.normal.x = (S.normal.x * rs + t * (E.normal.x * re - S.normal.x * rs)) / r;
                                newV.normal.y = (S.normal.y * rs + t * (E.normal.y * re - S.normal.y * rs)) / r;
                                newV.normal.z = (S.normal.z * rs + t * (E.normal.z * re - S.normal.z * rs)) / r;
                                newV.normal = newV.normal.normalized();

                                // color 简单线性插值（或做透视校正）
                                newV.color.r = (float)(S.color.r + t * (E.color.r - S.color.r));
                                newV.color.g = (float)(S.color.g + t * (E.color.g - S.color.g));
                                newV.color.b = (float)(S.color.b + t * (E.color.b - S.color.b));

                                // ===== UV 透视正确插值 =====
                                newV.uvPos.x = (S.uvPos.x * rs + t * (E.uvPos.x * re - S.uvPos.x * rs)) / r;
                                newV.uvPos.y = (S.uvPos.y * rs + t * (E.uvPos.y * re - S.uvPos.y * rs)) / r;

                                workingVerts.push_back(newV);
                            }
                            else {
                                // 平行或数值问题，退化：直接把 E 作为新点（减少错误）
                                workingVerts.push_back(E);
                            }
                        }
                        // 把 E 加入输出（原封不动或可复制）
                        workingVerts.push_back(E);
                    }
                    else if (Sin) {
                        // S in, E out -> 插入交点
                        double t; Vector3 iproj;
                        if (computeIntersectionParam(S,E,Sproj, Eproj, A, B, t, iproj)) {
                            Vertex newV;
                            newV.projectedPoint = iproj;
                            double rs = Sproj.ratio, re = Eproj.ratio;
                            double r = rs + t * (re - rs);
                            if (fabs(r) < EPSILON) r = EPSILON;

                            newV.point.x = (S.point.x * rs + t * (E.point.x * re - S.point.x * rs)) / r;
                            newV.point.y = (S.point.y * rs + t * (E.point.y * re - S.point.y * rs)) / r;
                            newV.point.z = (S.point.z * rs + t * (E.point.z * re - S.point.z * rs)) / r;
                            newV.point.ratio = r;

                            newV.normal.x = (S.normal.x * rs + t * (E.normal.x * re - S.normal.x * rs)) / r;
                            newV.normal.y = (S.normal.y * rs + t * (E.normal.y * re - S.normal.y * rs)) / r;
                            newV.normal.z = (S.normal.z * rs + t * (E.normal.z * re - S.normal.z * rs)) / r;
                            newV.normal = newV.normal.normalized();

                            newV.color.r = (float)(S.color.r + t * (E.color.r - S.color.r));
                            newV.color.g = (float)(S.color.g + t * (E.color.g - S.color.g));
                            newV.color.b = (float)(S.color.b + t * (E.color.b - S.color.b));

                            newV.uvPos.x = (S.uvPos.x * rs + t * (E.uvPos.x * re - S.uvPos.x * rs)) / r;
                            newV.uvPos.y = (S.uvPos.y * rs + t * (E.uvPos.y * re - S.uvPos.y * rs)) / r;

                            workingVerts.push_back(newV);
                        }
                        // else 忽略
                    }
                    // advance
                    S = E;
                    Sproj = Eproj;
                }
            } // end clip edges

            if (workingVerts.size() < 3) continue;

            // 将 workingVerts 追加到 mesh.clippedPoints，并生成新的 face 索引
            int baseIndex = mesh.clippedPoints.size();
            for (const auto& v : workingVerts) 
                mesh.clippedPoints.push_back(v);

            Face3D newFace = face;
            newFace.indices.resize(workingVerts.size());
            for (int i = 0; i < (int)workingVerts.size(); ++i) {
                newFace.indices[i] = baseIndex + i;
            }
            mesh.clippedFaces.push_back(move(newFace));
        } // end faces
    }
};

//统一调用
inline vector<Line2D> ClipLine(const Line2D& line, const Polygon2D& win, const IClipStrategy2D& clipper) {
    return clipper.ClipLine(line, win);
}

inline vector<Polygon2D> ClipPolygon(const Polygon2D& poly, const Polygon2D& win, const IClipStrategy2D& clipper) {
    return clipper.ClipPolygon(poly, win);
}

inline void ClipLine(Line3D& line, const Polygon2D& win, const IClipStrategy2D& clipper) {
    clipper.ClipLine(line, win);
}

inline void ClipMesh(Mesh3D& mesh, const Polygon2D& win, const IClipStrategy2D& clipper) {
    clipper.ClipMesh(mesh, win);
}
#endif