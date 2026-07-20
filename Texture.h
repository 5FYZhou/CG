#ifndef TEXTURE_H
#define TEXTURE_H

#include <iostream>
#include <objidl.h>        // IStream
#include <gdiplus.h>       // GDI+
#pragma comment(lib, "gdiplus.lib")
using namespace std;
using namespace Gdiplus;

struct Color_my {
    float r;
    float g;
    float b;
    Color_my(float rr = 0, float gg = 0, float bb = 0) :r(rr), g(gg), b(bb){}
    Color_my operator+(const Color_my& o) const { return Color_my(r + o.r, g + o.g, b + o.b); }
    Color_my& operator+=(const Color_my& o) { r += o.r; g += o.g; b += o.b; return *this; }
    Color_my operator-(const Color_my& o) const { return Color_my(r - o.r, g - o.g, b - o.b); }
    Color_my& operator-=(const Color_my& o) { r -= o.r; g -= o.g; b -= o.b; return *this; }
    Color_my operator*(const Color_my& o) const { return Color_my(r * o.r, g * o.g, b * o.b); }
    Color_my& operator*=(const Color_my& o) { r *= o.r; g *= o.g; b *= o.b; return *this; }
    Color_my operator*(float s) const { return Color_my(r * s, g * s, b * s); }
    Color_my& operator*=(float s) { r *= s; g *= s; b *= s; return *this; }
    Color_my operator/(float s) const { return Color_my(r / s, g / s, b / s); }
    Color_my& operator/=(float s) { r /= s; g /= s; b /= s; return *this; }
    bool operator==(const Color_my& c) const { return fabs(r - c.r) < EPSILON && fabs(g - c.g) < EPSILON && fabs(b - c.b) < EPSILON; }
    Color_my clamp() const {
        return Color_my(
            (r > 1.0f) ? 1.0f : ((r < 0.0f) ? 0.0f : r),
            (g > 1.0f) ? 1.0f : ((g < 0.0f) ? 0.0f : g),
            (b > 1.0f) ? 1.0f : ((b < 0.0f) ? 0.0f : b)
        );
    }
    float maxComponent() const {
        return std::max({ r, g, b });
    }
};
inline ostream& operator<< (ostream& os, const Color_my& c) {
    os << "Color(" << c.r << ", " << c.g << ", " << c.b << ", " << ")";
    return os;
}

class Texture {
public:
    int width, height;
    vector<Color_my> pixels;
    bool loaded;
    wstring name = L"";
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = default;
    Texture& operator=(Texture&&) = default;

    Texture() : width(0), height(0) { loaded = false; }
    Texture(const wstring& filename) {
        name = filename;
        loaded = false;
        Bitmap bmp(filename.c_str());
        //cout << "status = " << bmp.GetLastStatus() << endl;
        width = bmp.GetWidth();
        height = bmp.GetHeight();
        if (width == 0 || height == 0) {
            cout << "Failed to load texture: " << string(filename.begin(), filename.end()) << endl;
            return;
        }
        pixels.resize(width * height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Gdiplus::Color c;
                bmp.GetPixel(x, y, &c);
                pixels[y * width + x] = Color_my(c.GetRed() / 255.0f, c.GetGreen() / 255.0f, c.GetBlue() / 255.0f);
            }
        }
        loaded = true;
        cout << "Loded texture: " << string(filename.begin(), filename.end()) << " (" << width << "x" << height << ")\n";
    }

    Color_my Sample(double u, double v) const {
        if (pixels.empty()) return Color_my(0, 0, 0);

        // Clamp UV 到 [0,1]
        if (u < 0.0) u = 0.0;
        if (u > 1.0) u = 1.0;
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;

        int x = int(u * (width - 1));
        int y = int((1.0 - v) * (height - 1)); // v 翻转

        return pixels[y * width + x];
    }
};

shared_ptr<Texture> DefaultWhiteTexture()
{
    static shared_ptr<Texture> whiteTex = [] {
        auto t = make_shared<Texture>();
        t->width = 1;
        t->height = 1;
        t->pixels.push_back(Color_my(1, 1, 1));
        return t;
        }();
    return whiteTex;
}

struct Material {
    Color_my ka;          // 环境光
    Color_my kd;          // 漫反射
    Color_my ks;          // 高光颜色
    double shininess;  // 高光强度

    double reflectivity; // 0~1 真实反射率
    double t; // 0~1透明度 1为不透明
    double ior;          // 折射率（1.0=空气）
    Color_my ke;

    shared_ptr<Texture> diffuseMap; // 漫反射贴图

    Material(
        Color_my a = Color_my(0.1f, 0.1f, 0.1f),
        Color_my d = Color_my(0.8f, 0.8f, 0.8f),
        Color_my s = Color_my(0.5f, 0.5f, 0.5f),
        double sh = 32.0,
        double r = 0.0,
        double t = 0.0,
        double i = 1.0,
        Color_my e = Color_my(0.0f, 0.0f, 0.0f)
    ) : ka(a), kd(d), ks(s), shininess(sh), reflectivity(r), t(t), ior(i), ke(e) {
        // 只在没有贴图时才用白纹理
        if (!diffuseMap) {
            diffuseMap = DefaultWhiteTexture();
        }
    }
    //拷贝构造函数
    Material(const Material& o)
        : ka(o.ka), kd(o.kd), ks(o.ks),
        shininess(o.shininess),
        reflectivity(o.reflectivity),
        t(o.t), ior(o.ior), ke(o.ke),
        diffuseMap(o.diffuseMap) {
    }
};

#endif