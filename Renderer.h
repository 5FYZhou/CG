#ifndef RENDERER_H
#define RENDERER_H

#include "Graphics.h"
#include "Scene.h"
#include "FrameBuffer.h"
#include "Filler.h"
#include "ShadowMap.h"
using namespace std;

/// <summary>
/// 定义渲染器类，协调整个图形渲染管线（背面剔除、投影、裁剪、填充、光照、阴影、光栅化），支持多种渲染模式（无/有纹理、无/有光照、光线追踪直接光照），驱动从场景数据到屏幕像素的完整绘制流程。
/// </summary>

class Renderer {
public:
    Scene& scene;
    FrameBuffer& buffer;
    ShadowMapMgr shadowMgr;
    IClipStrategy2D* clipper;
    IFill2D& filler;
    Vector3 eyeDir;
    Camera* curCamera;

    Renderer(Scene& s, FrameBuffer& f, IClipStrategy2D* c, IFill2D& fill, Camera* ca) :scene(s), buffer(f), clipper(c), filler(fill), curCamera(ca) {
        filler.SetFrameBuffer(&buffer);
        shadowMgr.Init(1024, s);
    }

    void Render1() {
        cout << "------填充无纹理------" << endl;
        eyeDir = curCamera->lookAt - curCamera->eyePos;
        //重置帧缓存
        buffer.Refresh(curCamera->window);
        cout << "重置帧缓存" << endl;
        for (auto& mesh : scene.Mesh3Ds) {
            //背面剔除
            BackfaceElimination(*mesh);
            cout << "完成背面剔除" << endl;
            //投影
            ApplyProjection(*mesh);
            cout << "完成投影" << endl;
            //裁剪
            ApplyClipping(*mesh, curCamera->window, *clipper);
            cout << "完成裁剪" << endl;
            //填充
            FillMesh(*mesh, filler);
            cout << "完成填充" << endl;
        }
        //光栅化
        Rasterize();
        cout << "完成光栅化" << endl;
    }

    void Render2() {
        cout << "------填充有纹理------" << endl;
        eyeDir = curCamera->lookAt - curCamera->eyePos;
        //重置帧缓存
        buffer.Refresh(curCamera->window);
        cout << "重置帧缓存" << endl;
        for (auto& mesh : scene.Mesh3Ds) {
            //背面剔除
            BackfaceElimination(*mesh);
            cout << "完成背面剔除" << endl;
            //投影
            ApplyProjection(*mesh);
            cout << "完成投影" << endl;
            //裁剪
            ApplyClipping(*mesh, curCamera->window, *clipper);
            cout << "完成裁剪" << endl;
            //填充
            FillMeshWithTexture(*mesh, filler);
            cout << "完成填充" << endl;
        }
        //光栅化
        Rasterize();
        cout << "完成光栅化" << endl;
    }

    void Render3() {
        cout << "------光照无纹理------" << endl;
        eyeDir = curCamera->lookAt - curCamera->eyePos;
        //重置帧缓存
        buffer.Refresh(curCamera->window);
        cout << "重置帧缓存" << endl;
        //刷新shadow map
        shadowMgr.Refresh(scene);
        cout << "刷新shadow map" << endl;
        for (auto& mesh : scene.Mesh3Ds) {
            //背面剔除
            BackfaceElimination(*mesh);
            cout << "完成背面剔除" << endl;
            //ClipNearZ(*mesh, curCamera->viewMatrix, curCamera->nearZ);
            //ProjectMeshFacesAfterC(*mesh, curCamera->projMatrix);
            //投影
            ApplyProjection(*mesh);
            cout << "完成投影" << endl;
            //裁剪
            ApplyClipping(*mesh, curCamera->window, *clipper);
            cout << "完成裁剪" << endl;
            //光照
            LightWithoutTexture(*mesh);
            cout << "完成光照" << endl;
        }
        //光栅化
        Rasterize();
        cout << "完成光栅化" << endl;
    }

    void Render4() {
        cout << "------光照有纹理------" << endl;
        eyeDir = curCamera->lookAt - curCamera->eyePos;
        //重置帧缓存
        buffer.Refresh(curCamera->window);
        cout << "重置帧缓存" << endl;
        //刷新shadow map
        shadowMgr.Refresh(scene);
        cout << "刷新shadow map" << endl;
        for (auto& mesh : scene.Mesh3Ds) {
            //背面剔除
            BackfaceElimination(*mesh);
            cout << "完成背面剔除" << endl;
            //ClipNearZ(*mesh, curCamera->viewMatrix, curCamera->nearZ);
            //ProjectMeshFacesAfterC(*mesh, curCamera->projMatrix);
            //投影
            ApplyProjection(*mesh);
            cout << "完成投影" << endl;
            //裁剪
            ApplyClipping(*mesh, curCamera->window, *clipper);
            cout << "完成裁剪" << endl;
			//光照
			Light(*mesh);
			cout << "完成光照" << endl;
        }
        //光栅化
        Rasterize();
        cout << "完成光栅化" << endl;
        cout << "------------" << endl;
    }

    void Render5() {
        cout << "------离线------" << endl;
        eyeDir = curCamera->lookAt - curCamera->eyePos;
        //重置帧缓存
        buffer.Refresh(curCamera->window);
        cout << "重置帧缓存" << endl;
        //直接光照
        RayTraceDirectLighting();
		cout << "完成直接光照" << endl;
        //光栅化
        Rasterize();
        cout << "完成光栅化" << endl;
    }

    void BackfaceElimination(Mesh3D& meshs) {
        for (auto& f : meshs.faces) {
            f.canBeSee = false;
            if (f.normal.dot(eyeDir) <= 0) {
                f.canBeSee = true;
            }
        }
    }

    void ApplyProjection(Mesh3D& s) {
        ProjectMeshFaces(s, curCamera->viewProjMatrix);
    }
    void ApplyClipping(Mesh3D& s, const Polygon2D& win, IClipStrategy2D& clipper) {
        ClipMesh(s, win, clipper);
    }
    void Rasterize() {
        //应用眩光
        buffer.ApplyBloomOptimized(2, 0.05f, 0.05f);
        for (int i = 0; i < buffer.height; i++) {
            for (int j = 0; j < buffer.width; j++) {
                glColor3f(buffer.color[i][j].r, buffer.color[i][j].g, buffer.color[i][j].b);
                glVertex2i(j + buffer.x0, i + buffer.y0);
            }
        }
    }

    void Light(const Mesh3D& mesh) {
        vector<Face3D> transparentFace;
        for (auto& face : mesh.clippedFaces) {
            //透明先跳过
            if (face.mat.t < 1.0) {
                transparentFace.push_back(face);
                continue;
            }
            vector<Vertex> Vs;
            for (auto& index : face.indices)
                Vs.push_back(mesh.clippedPoints[index]);
            if (Vs.size() < 3)continue;
            ScanlineInterpolation(Vs, [this, face](int x, int y, double z, Vertex& v) {
                Color_my finalColor = scene.ambientLight * face.mat.ka;
                for (auto& light : scene.lights) {
                    pair<double, Color_my> shadows = { 0,{1,1,1} };
                    if (light->castShadow) {
                        shadows = shadowMgr.SampleShadow(light->shadowIndex, v.point, v.normal);
                    }
                    Color_my direct = light->ComputeBlinnPhong(v.point, v.normal.normalized(), curCamera->eyePos, face.mat, v.uvPos);

                    Color_my incoming = direct * (float)(1 - shadows.first) + direct * (float)shadows.first * shadows.second;

                    finalColor += incoming;
                }
                buffer.SetPixel(x, y, z, finalColor.clamp(), face.mat.t, face.mat.ke);
                }
            );
        }
        // 计算半透明面
        if (transparentFace.size() <= 0)return;
        sort(transparentFace.begin(), transparentFace.end(), [](Face3D a, Face3D b) {
            return a.center.z > b.center.z; // 从远到近
            });

        for (auto& face : transparentFace) {
            vector<Vertex> Vs;
            for (auto& index : face.indices)
                Vs.push_back(mesh.clippedPoints[index]);

            ScanlineInterpolation(Vs, [this, face](int x, int y, double z, Vertex& v) {
                Color_my lightAccum = { 0,0,0 };
                for (auto& light : scene.lights) {
                    pair<double, Color_my> shadows = { 0,{1,1,1} };
                    if (light->castShadow) {
                        shadows = shadowMgr.SampleShadow(light->shadowIndex, v.point, v.normal);
                    }
                    Color_my direct = light->ComputeBlinnPhong(v.point, v.normal.normalized(), curCamera->eyePos, face.mat, v.uvPos);

                    Color_my incoming = direct * (float)(1 - shadows.first) + direct * (float)shadows.first * shadows.second;

                    lightAccum += incoming;
                }
                Color_my lighting = scene.ambientLight * face.mat.ka + lightAccum;
                Color_my surface = lighting * (float)face.mat.t + face.mat.kd * (float)(1 - face.mat.t);

                buffer.SetPixel(x, y, z, surface.clamp(), face.mat.t);
                }
            );
        }
    }
    void LightWithoutTexture(const Mesh3D& mesh) {
        vector<Face3D> transparentFace;
        for (auto& face : mesh.clippedFaces) {
            //透明先跳过
            if (face.mat.t < 1.0) {
                transparentFace.push_back(face);
                continue;
            }
            vector<Vertex> Vs;
            for (auto& index : face.indices)
                Vs.push_back(mesh.clippedPoints[index]);
            if (Vs.size() < 3)continue;
            ScanlineInterpolation(Vs, [this, face](int x, int y, double z, Vertex& v) {
                Color_my finalColor = scene.ambientLight * face.mat.ka;
                for (auto& light : scene.lights) {
                    pair<double, Color_my> shadows = { 0,{1,1,1} };
                    if (light->castShadow) {
                        shadows = shadowMgr.SampleShadow(light->shadowIndex, v.point, v.normal);
                    }
                    Color_my direct = light->ComputeBlinnPhong(v.point, v.normal.normalized(), curCamera->eyePos, face.mat);

                    Color_my incoming = direct * (float)(1 - shadows.first) + direct * (float)shadows.first * shadows.second;

                    finalColor += incoming;
                }
                buffer.SetPixel(x, y, z, finalColor.clamp(), face.mat.t, face.mat.ke);
                }
            );
        }
        // 计算半透明面
        if (transparentFace.size() <= 0)return;
        sort(transparentFace.begin(), transparentFace.end(), [](Face3D a, Face3D b) {
            return a.center.z > b.center.z; // 从远到近
            });

        for (auto& face : transparentFace) {
            vector<Vertex> Vs;
            for (auto& index : face.indices)
                Vs.push_back(mesh.clippedPoints[index]);

            ScanlineInterpolation(Vs, [this, face](int x, int y, double z, Vertex& v) {
                Color_my lightAccum = { 0,0,0 };
                for (auto& light : scene.lights) {
                    pair<double, Color_my> shadows = { 0,{1,1,1} };
                    if (light->castShadow) {
                        shadows = shadowMgr.SampleShadow(light->shadowIndex, v.point, v.normal);
                    }
                    Color_my direct = light->ComputeBlinnPhong(v.point, v.normal.normalized(), curCamera->eyePos, face.mat);

                    Color_my incoming = direct * (float)(1 - shadows.first) + direct * (float)shadows.first * shadows.second;

                    lightAccum += incoming;
                }
                Color_my lighting = scene.ambientLight * face.mat.ka + lightAccum;
                Color_my surface = lighting * (float)face.mat.t + face.mat.kd * (float)(1 - face.mat.t);

                buffer.SetPixel(x, y, z, surface.clamp(), face.mat.t);
                }
            );
        }
    }

    void RayTraceDirectLighting() {
        BuildTriangleCache(scene.Mesh3Ds);

        vector<int> all;
        for (int i = 0; i < (int)gTriangles.size(); ++i)
            all.push_back(i);

        BVHNode* bvh = BuildBVH(all);

#pragma omp parallel for schedule(dynamic, 8)
        for (int y = 0; y < buffer.height; ++y) {
            for (int x = 0; x < buffer.width; ++x) {
                Vector3 p = { (double)(x + buffer.x0 + 0.5), (double)(y + buffer.y0 + 0.5), 0.0 };

                //p = ApplyTransformToVec3(curCamera->projMatrix, p);

                Ray ray(curCamera->eyePos, (p - curCamera->eyePos).normalized());
                double z;
                Color_my c = RayTracePixelBVH(ray, bvh, scene.lights, 0, 5, z);
                buffer.SetPixel(x + buffer.x0, y + buffer.y0, z, c);
            }
			if (y % 50 == 0) {
				cout << "Ray tracing row " << y << " / " << buffer.height << endl;
			}
        }

        delete bvh;
    }
};

#endif