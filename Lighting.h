#ifndef LIGHTING_H
#define LIGHTING_H

#include <omp.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <random>
#include <functional>
#include "Vector.h"
#include "Texture.h"
#include "Graphics.h"
using namespace std;

/// <summary>
/// 定义光源基类与具体点光源实现，提供 Flat 着色与 Blinn-Phong 光照模型计算（含纹理支持），用于Face3D的漫反射、镜面反射与自发光合成。
/// </summary>

class Light {
public:
    bool castShadow;
    int shadowIndex = -1;
    Vector3 position;
    Vector3 dir;

	virtual void Flat(Mesh3D& mesh) = 0;
    //virtual Color_my ComputeBlinnPhong(const Vector3& pos, const Vector3& normal, const Vector3& eyePos, const Material& mat) = 0;
};

class PointLight :public Light {
public:
    Color_my color;
    Color_my Ia = { 0.1f,0.1f,0.1f };
    Color_my Id;
    Color_my Is;
    double radius = 0.05;       // 软阴影半径 0.02~0.1 之间

    Color_my diffuseLight;
    Color_my specularLight;

    PointLight(Vector3 p = { 0,0,0 }, Vector3 d = { 0,0,0 }, Color_my c = { 0,0,0 }, bool s = true) {
        position = p;
        dir = d.normalized();
        color = c;
        castShadow = s;
        Id = Color_my(1.0f, 1.0f, 1.0f); // 漫反射光强
        Is = Color_my(0.5f, 0.5f, 0.5f); // 镜面反射光强

        diffuseLight = Id * color;
        specularLight = Is * color;
    }
    void Flat(Mesh3D& mesh) override {
        for (auto& face : mesh.clippedFaces) {

            // 计算面光照：Lambert漫反射
            Vector3 N = face.normal;   // 法线
            double intensity = max(0.0, N.dot(dir)); // Lambert
            Color_my faceColor;
            faceColor.r = (float)min(1.0, face.colorFill.r * intensity * color.r);
            faceColor.g = (float)min(1.0, face.colorFill.g * intensity * color.g);
            faceColor.b = (float)min(1.0, face.colorFill.b * intensity * color.b);

            face.colorFill = faceColor;
            // 使用扫描线填充面
            //FillPolygon(buffer, verts2D, faceColor);
        }
    }

    Color_my ComputeBlinnPhong(const Vector3& pos, const Vector3& normal, const Vector3& eyePos, const Material& mat)
    {
        Vector3 N = normal;
        Vector3 L = (position - pos).normalized();
        Vector3 V = (eyePos - pos).normalized();
        Vector3 H = (L + V).normalized();

        // 漫反射
        double diff = max(0.0, N.dot(L));

        // 高光
        double spec = pow(max(0.0, N.dot(H)), mat.shininess);

        Color_my result = mat.kd * diffuseLight * (float)diff + mat.ks * specularLight * (float)spec;
        result += mat.ke;

        return result.clamp();
    }
    Color_my ComputeBlinnPhong(const Vector3& pos, const Vector3& normal, const Vector3& eyePos, const Material& mat, const Vector2 uvPos)
    {
        Vector3 N = normal;
        Vector3 L = (position - pos).normalized();
        Vector3 V = (eyePos - pos).normalized();
        Vector3 H = (L + V).normalized();

        // 漫反射
        double diff = max(0.0, N.dot(L));

        // 高光
        double spec = pow(max(0.0, N.dot(H)), mat.shininess);

        Color_my kd = mat.diffuseMap?
            (mat.diffuseMap->loaded ? mat.diffuseMap->Sample(uvPos.x, uvPos.y) : mat.kd)
            :mat.kd;

        Color_my result = kd * diffuseLight * (float)diff + mat.ks * specularLight * (float)spec;
        result += mat.ke;

        return result.clamp();
    }
};


#pragma region 光线追踪
// 射线
struct Ray {
    Vector3 orig;
    Vector3 dir;
    bool insideMedium = false;  // 是否在玻璃内部

    Ray() {}
    Ray(const Vector3& o, const Vector3& d, bool inside = false)
        : orig(o), dir(d.normalized()), insideMedium(inside) {
    }
};


// ================= RNG =================
static thread_local std::mt19937 tl_rng(
    (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count()
    + omp_get_thread_num()
);

inline double Rand01() {
    return std::uniform_real_distribution<double>(0.0, 1.0)(tl_rng);
}

// ================= Triangle =================
struct Triangle {
    Vector3 v0, v1, v2;
    Vector3 n;

    Vector3 n0, n1, n2;
    Vector2 uv0, uv1, uv2;

    const Mesh3D* mesh;
    const Face3D* face;
};

static vector<Triangle> gTriangles;

static double PolygonArea2D(
    const vector<int>& poly,
    const vector<Vertex>& verts,
    int axis
) {
    double area = 0;
    int n = poly.size();
    for (int i = 0; i < n; ++i) {
        const Vector3& p = verts[poly[i]].point;
        const Vector3& q = verts[poly[(i + 1) % n]].point;
        if (axis == 0)      area += (p.y * q.z - q.y * p.z);
        else if (axis == 1) area += (p.x * q.z - q.x * p.z);
        else                area += (p.x * q.y - q.x * p.y);
    }
    return area * 0.5;
}

static int DominantAxis(const Vector3& n) {
    Vector3 a = { fabs(n.x), fabs(n.y), fabs(n.z) };
    if (a.x > a.y && a.x > a.z) return 0; // drop X
    if (a.y > a.z) return 1;              // drop Y
    return 2;                             // drop Z
}

static double Cross2D(
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    int dropAxis
) {
    if (dropAxis == 0) // YZ
        return (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
    if (dropAxis == 1) // XZ
        return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
    // XY
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool PointInTriangle2D(
    const Vector3& p,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    int axis
) {
    double c1 = Cross2D(a, b, p, axis);
    double c2 = Cross2D(b, c, p, axis);
    double c3 = Cross2D(c, a, p, axis);
    return (c1 >= 0 && c2 >= 0 && c3 >= 0) ||
        (c1 <= 0 && c2 <= 0 && c3 <= 0);
}

static bool IsEar(
    int prev, int curr, int next,
    const vector<int>& poly,
    const vector<Vertex>& verts,
    int axis,
    bool isCCW
) {
    const Vector3& A = verts[poly[prev]].point;
    const Vector3& B = verts[poly[curr]].point;
    const Vector3& C = verts[poly[next]].point;

    double cross = Cross2D(A, B, C, axis);

    // 根据多边形方向判断“凸”
    if (isCCW) {
        if (cross <= 1e-12) return false;
    }
    else {
        if (cross >= -1e-12) return false;
    }

    // 点在三角形内测试（方向无关）
    for (int i : poly) {
        if (i == poly[prev] || i == poly[curr] || i == poly[next])
            continue;

        if (PointInTriangle2D(verts[i].point, A, B, C, axis))
            return false;
    }
    return true;
}

static void TriangulateFaceEarClipping(
    const Face3D& face,
    const Mesh3D& mesh,
    vector<array<int, 3>>& outTris
) {
    vector<Vertex> verts;
    vector<int> poly;

    for (int idx : face.indices) {
        verts.push_back(mesh.points[idx]);
        poly.push_back((int)verts.size() - 1);
    }

    int axis = DominantAxis(face.normal);

    // 🔥 判断投影后的方向（解决 xoz 平行面）
    bool isCCW = PolygonArea2D(poly, verts, axis) > 0;

    while (poly.size() > 3) {
        bool cut = false;

        for (int i = 0; i < (int)poly.size(); ++i) {
            int p = (i - 1 + poly.size()) % poly.size();
            int n = (i + 1) % poly.size();

            if (IsEar(p, i, n, poly, verts, axis, isCCW)) {
                outTris.push_back({ poly[p], poly[i], poly[n] });
                poly.erase(poly.begin() + i);
                cut = true;
                break;
            }
        }

        // 🔥 兜底：避免整个面消失
        if (!cut) {
            for (int i = 1; i + 1 < poly.size(); ++i) {
                outTris.push_back({ poly[0], poly[i], poly[i + 1] });
            }
            return;
        }
    }

    if (poly.size() == 3) {
        outTris.push_back({ poly[0], poly[1], poly[2] });
    }
}


static void BuildTriangleCache(const vector<Mesh3D*>& meshes) {
    gTriangles.clear();

    for (const auto* mesh : meshes) {
        for (const auto& face : mesh->faces) {

            Vector3 fn = face.normal.normalized();

            // 已经是三角形
            if (face.indices.size() == 3) {
                Triangle t;
                int i0 = face.indices[0];
                int i1 = face.indices[1];
                int i2 = face.indices[2];

                t.v0 = mesh->points[i0].point;
                t.v1 = mesh->points[i1].point;
                t.v2 = mesh->points[i2].point;

                t.n0 = mesh->points[i0].normal;
                t.n1 = mesh->points[i1].normal;
                t.n2 = mesh->points[i2].normal;

                t.uv0 = mesh->points[i0].uvPos;
                t.uv1 = mesh->points[i1].uvPos;
                t.uv2 = mesh->points[i2].uvPos;

                t.n = fn;
                t.mesh = mesh;
                t.face = &face;

                Vector3 geoN = (t.v1 - t.v0).cross(t.v2 - t.v0);
                if (geoN.dot(fn) < 0) {
                    // 交换 v1 v2（翻转三角形）
                    std::swap(t.v1, t.v2);
                    std::swap(t.n1, t.n2);
                    std::swap(t.uv1, t.uv2);
                }

                gTriangles.push_back(t);
            }
            else {
                // 多边形 → Ear Clipping 一次
                vector<array<int, 3>> tris;
                TriangulateFaceEarClipping(face, *mesh, tris);

                for (auto& tri : tris) {
                    Triangle t;

                    int i0 = face.indices[tri[0]];
                    int i1 = face.indices[tri[1]];
                    int i2 = face.indices[tri[2]];

                    t.v0 = mesh->points[i0].point;
                    t.v1 = mesh->points[i1].point;
                    t.v2 = mesh->points[i2].point;

                    t.n0 = mesh->points[i0].normal;
                    t.n1 = mesh->points[i1].normal;
                    t.n2 = mesh->points[i2].normal;

                    t.uv0 = mesh->points[i0].uvPos;
                    t.uv1 = mesh->points[i1].uvPos;
                    t.uv2 = mesh->points[i2].uvPos;

                    t.n = fn;
                    t.mesh = mesh;
                    t.face = &face;

                    Vector3 geoN = (t.v1 - t.v0).cross(t.v2 - t.v0);
                    if (geoN.dot(fn) < 0) {
                        // 交换 v1 v2（翻转三角形）
                        std::swap(t.v1, t.v2);
                        std::swap(t.n1, t.n2);
                        std::swap(t.uv1, t.uv2);
                    }

                    gTriangles.push_back(t);
                }
            }
        }
    }

    cout << "[RT] Triangle count = " << gTriangles.size() << endl;
}


// ----------------------------- 优化辅助函数 -----------------------------

struct AABB {
    Vector3 minV, maxV;

    void Expand(const Vector3& p) {
        minV.x = std::min(minV.x, p.x);
        minV.y = std::min(minV.y, p.y);
        minV.z = std::min(minV.z, p.z);
        maxV.x = std::max(maxV.x, p.x);
        maxV.y = std::max(maxV.y, p.y);
        maxV.z = std::max(maxV.z, p.z);
    }

    bool Intersect(const Ray& ray, double tMaxLimit) const {
        double tmin = 0.0;
        double tmax = tMaxLimit;

        for (int axis = 0; axis < 3; ++axis) {
            double orig = ray.orig[axis];
            double dir = ray.dir[axis];
            double minp = minV[axis];
            double maxp = maxV[axis];

            if (fabs(dir) < 1e-12) {
                // 射线与 slab 平行
                if (orig < minp || orig > maxp)
                    return false;
            }
            else {
                double invD = 1.0 / dir;
                double t0 = (minp - orig) * invD;
                double t1 = (maxp - orig) * invD;
                if (t0 > t1) std::swap(t0, t1);

                tmin = std::max(tmin, t0);
                tmax = std::min(tmax, t1);

                if (tmin > tmax)
                    return false;
            }
        }
        return true;
    }

};


// ----------------------------- Blinn-Phong -----------------------------
static Color_my ShadeBlinnPhongPointLight(
    const PointLight* pl,
    const Vector3& pos,
    const Vector3& N,
    const Vector3& V,
    const Material& mat,
    const Vector2& uvPos
) {
    Vector3 L = (pl->position - pos).normalized();
    Vector3 H = (L + V).normalized();
    double diff = std::max(0.0, N.dot(L));
    double spec = pow(std::max(0.0, N.dot(H)), mat.shininess);

    Color_my kd = mat.diffuseMap ?
        (mat.diffuseMap->loaded ? mat.diffuseMap->Sample(uvPos.x, uvPos.y) : mat.kd)
        : mat.kd;

    Color_my result;
    //result = mat.ka * pl->Ia + kd * pl->Id * diff + mat.ks * pl->Is * spec;
    result = kd * pl->Id * diff + mat.ks * pl->Is * spec;
    return result;
}

// ----------------------------- BVH -----------------------------
struct BVHNode {
    AABB box;
    BVHNode* left = nullptr;
    BVHNode* right = nullptr;
    vector<int> triIndices;

    ~BVHNode() {
        delete left;
        delete right;
    }
};

// 构建 BVH（递归）
static BVHNode* BuildBVH(const vector<int>& tris, int depth = 0) {
    const int maxLeafTris = 4;
    const int maxDepth = 32;

    BVHNode* node = new BVHNode();

    // 计算 AABB
    node->box.minV = node->box.maxV = gTriangles[tris[0]].v0;
    for (int id : tris) {
        const Triangle& t = gTriangles[id];
        node->box.Expand(t.v0);
        node->box.Expand(t.v1);
        node->box.Expand(t.v2);
    }

    if (tris.size() <= maxLeafTris || depth >= maxDepth) {
        node->triIndices = tris;
        return node;
    }

    Vector3 ext = node->box.maxV - node->box.minV;
    int axis = (ext.y > ext.x && ext.y > ext.z) ? 1 : (ext.z > ext.x ? 2 : 0);

    double mid = 0;
    for (int id : tris) {
        const Triangle& t = gTriangles[id];
        Vector3 c = (t.v0 + t.v1 + t.v2) / 3.0;
        mid += c[axis];
    }
    mid /= tris.size();

    vector<int> L, R;
    for (int id : tris) {
        Vector3 c = (gTriangles[id].v0 +
            gTriangles[id].v1 +
            gTriangles[id].v2) / 3.0;
        (c[axis] < mid ? L : R).push_back(id);
    }

    if (L.empty() || R.empty()) {
        node->triIndices = tris;
        return node;
    }

    node->left = BuildBVH(L, depth + 1);
    node->right = BuildBVH(R, depth + 1);
    return node;
}

static bool RayTriangleMT(
    const Ray& r,
    const Triangle& t,
    double& outT,
    double& u,
    double& v
) {
    const double EPS = 1e-8;

    Vector3 e1 = t.v1 - t.v0;
    Vector3 e2 = t.v2 - t.v0;

    Vector3 p = r.dir.cross(e2);
    double det = e1.dot(p);

    if (fabs(det) < EPS)
        return false;

    double invDet = 1.0 / det;

    Vector3 s = r.orig - t.v0;
    u = s.dot(p) * invDet;
    if (u < 0.0 || u > 1.0)
        return false;

    Vector3 q = s.cross(e1);
    v = r.dir.dot(q) * invDet;
    if (v < 0.0 || u + v > 1.0)
        return false;

    double tHit = e2.dot(q) * invDet;
    if (tHit <= EPS)
        return false;

    outT = tHit;
    return true;
}




// ----------------------------- BVH 光线查询 -----------------------------
static bool IntersectBVH(
    const Ray& ray,
    BVHNode* node,
    double& nearestT,
    const Triangle*& hitTri,
    double& hitU,
    double& hitV
) {
    if (!node || !node->box.Intersect(ray, nearestT))
        return false;

    bool hit = false;

    if (!node->triIndices.empty()) {
        for (int id : node->triIndices) {
            double t, u, v;
            if (RayTriangleMT(ray, gTriangles[id], t, u, v) && t < nearestT) {
                nearestT = t;
                hitTri = &gTriangles[id];
                hitU = u;
                hitV = v;
                hit = true;
            }
        }
        return hit;
    }

    hit |= IntersectBVH(ray, node->left, nearestT, hitTri, hitU, hitV);
    hit |= IntersectBVH(ray, node->right, nearestT, hitTri, hitU, hitV);
    return hit;
}

static Color_my IntersectBVH_ShadowColored_Once(
    const Ray& ray,
    BVHNode* bvh,
    double maxT
) {
    const double EPS = 1e-4;

    Color_my T(1, 1, 1);
    Ray curRay = ray;

    while (maxT > EPS) {
        double nearestT = maxT;
        const Triangle* hitTri = nullptr;
        double u, v;

        if (!IntersectBVH(curRay, bvh, nearestT, hitTri, u, v))
            break;

        const Material& mat = hitTri->face->mat;

        // 不透明：完全挡光
        if (1.0 - mat.t < EPSILON) {
            return Color_my(0, 0, 0);
        }

        // 彩色透射
        Color_my tint =
            Color_my(1, 1, 1) * (1 - mat.t) +
            mat.kd * (mat.t);

        T *= tint;

        if (T.maxComponent() < 1e-3)
            return Color_my(0, 0, 0);

        Vector3 P = curRay.orig + curRay.dir * nearestT;

        // 🔥 稳定偏移（关键）
        curRay.orig = P + curRay.dir * EPS;
        maxT -= nearestT;
    }

    return T;
}



static Vector3 SampleSphere(const Vector3& center, double radius) {
    double u1 = Rand01();
    double u2 = Rand01();

    double z = 1.0 - 2.0 * u1;
    double r = sqrt(std::max(0.0, 1.0 - z * z));
    double phi = 2.0 * PI * u2;

    Vector3 dir(
        r * cos(phi),
        r * sin(phi),
        z
    );

    return center + dir * radius;
}

static Color_my SoftShadowTransmission(
    const Vector3& P,
    const Vector3& Ng,
    const PointLight* pl,
    BVHNode* bvh,
    int shadowSamples
) {
    const double EPS = 1e-4;
    Color_my sumT(0, 0, 0);

    for (int i = 0; i < shadowSamples; ++i) {

        // ① 在光源球面上随机采样一个点
        Vector3 lightPos =
            SampleSphere(pl->position, pl->radius);

        Vector3 L = (lightPos - P);
        double dist = L.length();
        L.normalized();

        double cosNL = std::abs(Ng.dot(L));
        double bias = std::max(1e-4, 1e-3 * (1.0 - cosNL));

        // ② 阴影射线
        Ray shadowRay(P + Ng * bias, L);

        // ③ 彩色透射阴影
        Color_my T =
            IntersectBVH_ShadowColored_Once(
                shadowRay, bvh, dist - bias
            );

        sumT += T;
    }

    // ④ 平均
    return sumT * (1.0 / shadowSamples);
}



static bool Refract(
    const Vector3& I,     // 入射方向（已归一化，指向物体）
    const Vector3& N,     // 表面法线（指向外）
    double ior,           // 材质折射率
    Vector3& T            // 输出：折射方向
) {
    double cosi = std::clamp(I.dot(N), -1.0, 1.0);
    double etai = 1.0, etat = ior;
    Vector3 n = N;

    // 判断进出介质
    if (cosi < 0.0) {
        cosi = -cosi;
    }
    else {
        std::swap(etai, etat);
        n = -N;
    }

    double eta = etai / etat;
    double k = 1.0 - eta * eta * (1.0 - cosi * cosi);

    if (k < 0.0)
        return false; // 全反射

    T = I * eta + n * (eta * cosi - sqrt(k));
    return true;
}


static double FresnelSchlick(
    double cosTheta,
    double ior
) {
    double r0 = (1.0 - ior) / (1.0 + ior);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}


// ----------------------------- 修改 RayTracePixel 使用 BVH -----------------------------
static Color_my RayTracePixelBVH(
    const Ray& r,
    BVHNode* bvh,
    const vector<PointLight*>& lights,
    int depth,
    int maxDepth,
    double& outZ
) {
    if (depth >= maxDepth) {
        outZ = 0;
        return Color_my(1, 1, 1);
    }

    double nearestT = 1e30;
    const Triangle* tri = nullptr;
    double u, v;

    if (!IntersectBVH(r, bvh, nearestT, tri, u, v)) {
        outZ = 0;
        return Color_my(1, 1, 1);
    }

    double w = 1.0 - u - v;
    Vector3 P = r.orig + r.dir * nearestT;
    Vector3 N = (tri->n0 * w + tri->n1 * u + tri->n2 * v).normalized();
    Vector2 uv = tri->uv0 * w + tri->uv1 * u + tri->uv2 * v;

    Vector3 V = (r.orig - P).normalized();

    const Material& mat = tri->face->mat;
    Vector3 Ng = tri->n;
    Vector3 I = r.dir.normalized();

    int shadowSamples = 16; // 8 / 16 / 32

    //Color_my C(0, 0, 0);
    Color_my C = mat.ke;

    //玻璃内部的光线
    if (r.insideMedium) {
        if (depth + 1 >= maxDepth)
            return Color_my(1, 1, 1);

        // 保证法线方向
        Vector3 Ng = tri->n;
        if (Ng.dot(I) > 0.0) Ng = -Ng;

        Vector3 T;
        if (depth < maxDepth && Refract(I, Ng, mat.ior, T)) {

            Ray tr(
                P - Ng * 1e-4,
                T.normalized(),
                false // 🔥 必须离开介质
            );

            double dummyZ;
            Color_my refractColor =
                RayTracePixelBVH(tr, bvh, lights, depth + 1, maxDepth, dummyZ);

            C = refractColor;   // 🔥 不要 Fresnel
            C.clamp();
            return C;
        }

        // 全反射兜底
        Vector3 R = I - Ng * 2.0 * I.dot(Ng);
        Ray rr(P + Ng * 1e-4, R.normalized(), true);
        double dummyZ;
        return RayTracePixelBVH(rr, bvh, lights, depth + 1, maxDepth, dummyZ);
    }

    Color_my localColor(0, 0, 0);
    Color_my reflectColor(0, 0, 0);
    Color_my refractColor(0, 0, 0);

    for (auto* pl : lights) {
        Vector3 L = (pl->position - P).normalized();
        double cosNL = std::abs(Ng.dot(L));
        double bias = std::max(1e-4, 1e-3 * (1.0 - cosNL));

        Ray shadow(P + Ng * bias, L);
        double maxT = (pl->position - P).length() - bias;

        Color_my shadowT =
            SoftShadowTransmission(
                P, Ng, pl, bvh, shadowSamples
            );

        if (shadowT.maxComponent() > EPSILON) {
            // 半透明挡光
            Color_my light = ShadeBlinnPhongPointLight(pl, P, N, V, mat, uv);
            localColor += light * shadowT;
        }
    }
    
    // 半透明物体再计算反射和折射
    if (mat.t < 1 - EPSILON) {
        double cosTheta = std::abs(I.dot(Ng));
        double kr = FresnelSchlick(cosTheta, mat.ior);
        double trans = 1.0 - mat.t;
        double kt = (1.0 - kr) * trans;

        // 反射
        if (kr > 1e-4 && depth < maxDepth) {
            Vector3 R = I - Ng * 2.0 * I.dot(Ng);
            Ray rr(P + Ng * 1e-4, R.normalized());
            double dummyZ;
            reflectColor = RayTracePixelBVH(
                rr, bvh, lights, depth + 1, maxDepth, dummyZ
            );
        }

        // 折射
        Vector3 T;
        if (kt > 1e-4 && depth < maxDepth && Refract(I, Ng, mat.ior, T)) {
            bool entering = I.dot(Ng) < 0.0;

            // 如果是进入玻璃 → insideMedium = true
            // 如果是离开玻璃 → insideMedium = false
            Ray tr(
                P - Ng * 1e-4,
                T.normalized(),
                entering ? true : false
            );

            double dummyZ;
            refractColor = RayTracePixelBVH(
                tr, bvh, lights, depth + 1, maxDepth, dummyZ
            );

        }

        Color_my specColor =
            reflectColor * kr +
            refractColor * (1.0 - kr);

        C =
            localColor * mat.t
            + specColor * (1.0 - mat.t);
    }

    C.clamp();
    outZ = P.z;
    return C;
}
#pragma endregion

#endif