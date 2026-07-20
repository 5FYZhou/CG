#ifndef SHADOWMAP_H
#define SHADOWMAP_H

#include <vector>
#include "Constants.h"
#include "Vector.h"
#include "Matrix.h"
#include "Lighting.h"
#include "Scene.h"

/// <summary>
/// 实现基于阴影映射（Shadow Map）的软阴影技术（PCSS），支持硬阴影、半透明遮挡颜色累积，管理多光源阴影贴图，提供阴影因子与透射颜色的采样接口。
/// </summary>

static const Vector2 poisson[16] = {
    { -0.94201624, -0.39906216 }, { 0.94558609, -0.76890725 },
    { -0.09418410, -0.92938870 }, { 0.34495938,  0.29387760 },
    { -0.91588581,  0.45771432 }, { -0.81544232, -0.87912464 },
    { -0.38277543,  0.27676845 }, { 0.97484398,  0.75648379 },
    { 0.44323325, -0.97511554 }, { 0.53742981, -0.47373420 },
    { -0.26496911, -0.41893023 }, { 0.79197514,  0.19090188 },
    { -0.24188840,  0.99706507 }, { -0.81409955,  0.91437590 },
    { 0.19984126,  0.78641367 }, { 0.14383161, -0.14100790 }
};

class ShadowMap {
public:
    int resolution;
    vector<vector<double>> buffer;
    vector<vector<double>> bufferT;
    vector<vector<Color_my>> bufferC;

    Vector3 lightPos;
    double lightRadius;
    Matrix_my lightView;
    Matrix_my lightProj;

    ShadowMap(int r = 1024){
        Reset(r);
    }
    void Reset(int r) {
        resolution = r;
        buffer.assign(resolution, vector<double>(resolution, MAXINT));
        bufferT.assign(resolution, vector<double>(resolution, 1));
        bufferC.assign(resolution, vector<Color_my>(resolution,{1,1,1}));
    }

    void InitLightOfMy(const PointLight* light, const pair<Vector3, Vector3>& bound) {
        Vector3 minBound = bound.first;
        Vector3 maxBound = bound.second;
        lightPos = light->position;
        lightRadius = light->radius;

        lightView = Transform3D::LookAt(light->position, { 0,0,0 }, { 0,1,0 });
        // 转到光源视图空间
        Vector4 corners[8];
        corners[0] = ApplyTransformToVec4(lightView, { minBound.x, minBound.y, minBound.z });
        corners[1] = ApplyTransformToVec4(lightView, { minBound.x, minBound.y, maxBound.z });
        corners[2] = ApplyTransformToVec4(lightView, { minBound.x, maxBound.y, minBound.z });
        corners[3] = ApplyTransformToVec4(lightView, { minBound.x, maxBound.y, maxBound.z });
        corners[4] = ApplyTransformToVec4(lightView, { maxBound.x, minBound.y, minBound.z });
        corners[5] = ApplyTransformToVec4(lightView, { maxBound.x, minBound.y, maxBound.z });
        corners[6] = ApplyTransformToVec4(lightView, { maxBound.x, maxBound.y, minBound.z });
        corners[7] = ApplyTransformToVec4(lightView, { maxBound.x, maxBound.y, maxBound.z });

        // 重新计算光源空间下的最小包围盒
        Vector3 lightMin = { DBL_MAX, DBL_MAX, DBL_MAX };
        Vector3 lightMax = { -DBL_MAX, -DBL_MAX, -DBL_MAX };
        for (int i = 0; i < 8; ++i) {
            lightMin.x = min(lightMin.x, corners[i].x);
            lightMin.y = min(lightMin.y, corners[i].y);
            lightMin.z = min(lightMin.z, corners[i].z);

            lightMax.x = max(lightMax.x, corners[i].x);
            lightMax.y = max(lightMax.y, corners[i].y);
            lightMax.z = max(lightMax.z, corners[i].z);
        }

        // 生成正交投影
        OrthographicProjection proj;
        lightProj = proj.ProjectMatrix(
            lightMin.x, lightMax.x,
            lightMin.y, lightMax.y,
            lightMin.z, lightMax.z
        );
    }

    Vector3 Standardization(const Vector4 p) const {
        double sx = ((p.x * 0.5 + 0.5) * (resolution - 1));
        double sy = ((p.y * 0.5 + 0.5) * (resolution - 1));
        double z = (p.z * 0.5 + 0.5); // [-1,1] → [0,1]
        return { sx, sy, z };
    }
    
    void SetPixel(int x, int y, double z) {
        if (x < 0 || y < 0 || x >= resolution || y >= resolution)return;
        buffer[y][x] = min(z, buffer[y][x]);
        //bufferT[y][x] = min(t, bufferT[y][x]);
    }
    void SetPixelT(int x, int y, double t) {
        if (x < 0 || y < 0 || x >= resolution || y >= resolution)return;
        //bufferT[y][x] = min(t, bufferT[y][x]);
        bufferT[y][x] *= t;
    }
    void SetPixelC(int x, int y, Color_my c) {
        if (x < 0 || y < 0 || x >= resolution || y >= resolution)return;
        bufferC[y][x] = c;
    }

    double GetPixelT(double xx, double yy) const {
        int x = (int)(xx + 0.5);
        int y = (int)(yy + 0.5);
        if (x < 0 || y < 0 || x >= resolution || y >= resolution)return 1;
        return bufferT[y][x];
    }
    double GetPixelBilinear(double x, double y) const {
        int x0 = (int)floor(x);
        int y0 = (int)floor(y);
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        double sx = x - x0;
        double sy = y - y0;

        auto fetch = [&](int ix, int iy) {
            if (ix < 0 || iy < 0 || ix >= resolution || iy >= resolution)
                return 1.0;
            double d = buffer[iy][ix];
            return (d == MAXINT) ? 1.0 : d;
            };

        double d00 = fetch(x0, y0);
        double d10 = fetch(x1, y0);
        double d01 = fetch(x0, y1);
        double d11 = fetch(x1, y1);

        return
            d00 * (1 - sx) * (1 - sy) +
            d10 * sx * (1 - sy) +
            d01 * (1 - sx) * sy +
            d11 * sx * sy;
    }

    void Render(const PointLight* light, const vector<Mesh3D*> meshs) {
        for (auto& mesh : meshs) {
            vector<Face3D> facesT;
            for (auto& face : mesh->faces) {
                //半透明先跳过
                if (face.mat.t < 1) {
					facesT.push_back(face);
					continue;
                }
                vector<Vector3> Vs;
                for (auto& i : face.indices) {
                    // 光源 view 空间 再透视投影
                    Vector4 p = ApplyTransformToVec4(lightProj * lightView, mesh->points[i].point);
                    if (fabs(p.w) < EPSILON) continue;
                    p = p * (1 / p.w);
                    // 映射
                    Vector3 v = Standardization(p);
                    Vs.push_back(v);
                }
                if (Vs.size() < 3)continue;

                ScanlineInterpolation(Vs, [&](int x, int y, double z) {
					// 不透明，只写入深度
                    SetPixel(x, y, z);
                });
                
            }
            if (facesT.size() <= 0)continue;
            sort(facesT.begin(), facesT.end(), [](Face3D a, Face3D b) {
                return a.center.z > b.center.z; // 从远到近
                });
			// 处理半透明面
            for (auto& face : facesT) {
                vector<Vector3> Vs;
                for (auto& i : face.indices) {
                    // 光源 view 空间 再透视投影
                    Vector4 p = ApplyTransformToVec4(lightProj * lightView, mesh->points[i].point);
                    if (fabs(p.w) < EPSILON) continue;
                    p = p * (1 / p.w);
                    // 映射
                    Vector3 v = Standardization(p);
                    Vs.push_back(v);
                }
                if (Vs.size() < 3)continue;

                double alpha = face.mat.t;
                ScanlineInterpolation(Vs, [&](int x, int y, double z) {
					//半透明，不写入深度，只更新透明度和颜色
                    SetPixelT(x, y, alpha);
                    SetPixelC(x, y, bufferC[y][x] * (float)(1 - alpha) + face.mat.kd * (float)alpha);
                });
            }
        }
    }

    double FindBlockerPCSS(const Vector3& v, double receiverDepth) const {
        double avgDepth = 0.0;
        int count = 0;

        const double searchRadius = 3.0;

        for (int i = 0; i < 16; ++i) {
            Vector2 offset = poisson[i] * searchRadius;
            double d = GetPixelBilinear(v.x + offset.x, v.y + offset.y);

            if (d < receiverDepth) {
                avgDepth += d;
                count++;
            }
        }

        if (count == 0)
            return -1.0; // 没有 blocker

        return avgDepth / count;
    }

    pair<double,Color_my> PCSSShadow(const Vector3& v, double receiverDepth, double bias, const Vector3& worldPos) const {
        double blockerDepth = FindBlockerPCSS(v, receiverDepth);
        if (blockerDepth < 0.0)
            return { 0.0,{1,1,1} };

        //接收点离 blocker 越远 → 半影越大; blocker 离光源越近 → 半影越大
        double penumbra = (receiverDepth - blockerDepth) / blockerDepth * lightRadius;
        //把归一化的半影大小转换为 shadow map 的像素单位, 作为 PCF 采样半径
        double filterRadius = penumbra * resolution;
        //对 PCF 半径做安全限制： 最小 1 像素，避免无滤波导致锯齿 最大 12 像素，防止性能灾难和过度模糊
        filterRadius = min(max(filterRadius, 1.0), 12.0);

        //PCF
        double shadow = 0.0;
        Color_my transColor = { 0,0,0 };
        int hit = 0;
        for (int i = 0; i < 16; ++i) {
            Vector2 offset = poisson[i] * filterRadius;
            double sampleDepth = GetPixelBilinear(v.x + offset.x, v.y + offset.y);

            if (sampleDepth == MAXINT) sampleDepth = 1.0;

            // 光线被半透明遮挡时，按深度差和 alpha 缩减
            if (receiverDepth > sampleDepth + bias) {
                double T = GetPixelT(v.x + offset.x, v.y + offset.y);
                if (fabs(1 - T) < EPSILON) {
                    // 不透明直接全遮挡
                    shadow += 1.0;          
                }
                else {
                    //阴影强度按不透明比例累加
                    shadow += (1.0 - T); 
                    Color_my C = bufferC[(int)(v.y + offset.y + 0.5)][(int)(v.x + offset.x)];
                    //按透明度权重累加透射颜色 表示有多少光线穿过该遮挡物
                    transColor += C * (float)T;
                }
                hit++;
            }
        }
        //如果存在遮挡： 对累计的透射颜色取平均 防止颜色随采样数变暗
        if (hit > 0)
            transColor /= (float)hit;
        else
            transColor = { 1,1,1 };
        return { shadow / (16 * 1.0),transColor };
    }

    pair<double, Color_my> SampleShadow(const Vector3& worldPos, const Vector3& normal) const {
        Vector4 p = ApplyTransformToVec4(lightProj * lightView, worldPos);
        if (fabs(p.w) < EPSILON) return { 0.0,{1,1,1} };

        p = p * (1.0 / p.w);
        Vector3 v = Standardization(p);

        if (v.x < 0 || v.y < 0 || v.x >= resolution || v.y >= resolution)
            return { 0.0,{1,1,1} };

        double receiverDepth = v.z;

        Vector3 L = (lightPos - worldPos).normalized();
        Vector3 N = normal.normalized();
        double slopeBias = 0.002 * (1.0 - max(0.0, N.dot(L)));
        double bias = max(0.001, slopeBias);

        return PCSSShadow(v, receiverDepth, bias, worldPos);
    }
};

class ShadowMapMgr {
public:
    vector<ShadowMap> maps;

    int resolution;

    ShadowMapMgr(int r = 0) :resolution(r) {}

    pair<Vector3, Vector3> CAABB(const vector<Mesh3D*> meshs){
        Vector3 minBound = { DBL_MAX, DBL_MAX, DBL_MAX };
        Vector3 maxBound = { -DBL_MAX, -DBL_MAX, -DBL_MAX };
        for (auto& mesh : meshs) {
            for (auto& p : mesh->points) {
                minBound.x = min(minBound.x, p.point.x);
                minBound.y = min(minBound.y, p.point.y);
                minBound.z = min(minBound.z, p.point.z);
                maxBound.x = max(maxBound.x, p.point.x);
                maxBound.y = max(maxBound.y, p.point.y);
                maxBound.z = max(maxBound.z, p.point.z);
            }
        }
        return pair(minBound, maxBound);
    }

    void Init(const int r, const Scene& scene) {
        resolution = r;
        int i = 0;
        maps.clear();
        for (auto& light : scene.lights) {
            if (!light->castShadow)continue;

            ShadowMap map(resolution);
            maps.push_back(map);

            light->shadowIndex = i;
            i++;
        }
    }

    pair<double, Color_my> SampleShadow(int index, const Vector3& worldPos, const Vector3& normal) {
        return maps[index].SampleShadow(worldPos, normal);
    }

    void Refresh(const Scene& scene) {
        pair<Vector3, Vector3> bound = CAABB(scene.Mesh3Ds);
        for (auto& light : scene.lights) {
            if (!light->castShadow)continue;
            auto& map = maps[light->shadowIndex];
            map.Reset(map.resolution);
            map.InitLightOfMy(light, bound);
            map.Render(light, scene.Mesh3Ds);
        }
    }
};

#endif