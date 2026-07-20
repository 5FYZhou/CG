#ifndef FRAMEBUFFRT_H
#define FRAMEBUFFRT_H

#include <GL/freeglut.h>
#include "Constants.h"
#include "Vector.h"
#include "Graphics.h"

/// <summary>
/// 定义帧缓冲区类，管理颜色、深度与 Bloom 高亮缓冲，支持窗口自适应尺寸、像素级深度测试与透明度混合，并能累积自发光颜色用于后期泛光效果。
/// </summary>

class FrameBuffer {
public:
    int x0, y0;
    int width, height;
    vector<vector<double>> depth;
    vector<vector<Color_my>> color;    
    // 新增：Bloom 高亮缓冲
    vector<vector<Color_my>> bloomBuffer;

    FrameBuffer(Polygon2D& win) {
        Refresh(win);
    }

    void Refresh(Polygon2D& win) {
        width = (int)(win.points[1].x - win.points[0].x);
        height = (int)(win.points[3].y - win.points[0].y);
        x0 = (int)(win.points[0].x);
        y0 = (int)(win.points[0].y);

        depth.assign(height, vector<double>(width, MININT));
        color.assign(height, vector<Color_my>(width, Color_my(1, 1, 1)));
        bloomBuffer.assign(height, vector<Color_my>(width, Color_my(0, 0, 0)));
    }

    void SetPixel(int xx, int yy, double z, const Color_my& c) {
        int x = xx - x0 + 1, y = yy - y0 + 1;           //因为移到左边时 有部分填充超出了裁剪窗口
        if (x >= width || y >= height)return;
        if (x < 0 || y < 0)return;
        if (depth[y][x] < z) {
            depth[y][x] = z;
            color[y][x] = c;
        }
    }
    void SetPixel(int xx, int yy, double z, const Color_my& c, double alpha) {
        int x = xx - x0 + 1, y = yy - y0 + 1;
        if (x >= width || y >= height)return;
        if (x < 0 || y < 0)return;
        // 先检查深度
        if (depth[y][x] < z) {
            // 如果 alpha < 1，则做混合
            if (alpha < 1.0) {
                Color_my dst = color[y][x];
                Color_my blended = c * (float)alpha + dst * (float)(1.0 - alpha);
                color[y][x] = blended.clamp();
                // 半透明不写深度，避免遮挡后面的物体
            }
            else {
                color[y][x] = c.clamp();
                depth[y][x] = z;
            }
        }
    }
    void SetPixel(int xx, int yy, double z, const Color_my& c, double alpha, const Color_my& ke) {
        int x = xx - x0 + 1, y = yy - y0 + 1;
        if (x < 0 || x >= width || y < 0 || y >= height) return;

        // 深度判断
        if (depth[y][x] < z) {
            // 半透明混合
            if (alpha < 1.0) {
                Color_my dst = color[y][x];
                Color_my blended = c * (float)alpha + dst * (float)(1.0 - alpha);
                color[y][x] = blended.clamp();
                // 半透明不写深度
            }
            else {
                color[y][x] = c.clamp();
                depth[y][x] = z;
                //不是自发光就重置
                if (ke.r < EPSILON && ke.g < EPSILON && ke.b < EPSILON) {
                    bloomBuffer[y][x] = Color_my(0, 0, 0);
                }
                else {
                    // 处理 Bloom 高亮
                    float brightness = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
                    float keBrightness = 0.2126f * ke.r + 0.7152f * ke.g + 0.0722f * ke.b;
                    if (keBrightness > EPSILON) {
                        bloomBuffer[y][x] = bloomBuffer[y][x] + ke; // 累积自发光
                    }
                }
            }
        }
    }

    void ApplyBloomOptimized(int levels = 3, float intensity = 1.0f, float threshold = 0.05f) {
        // Step 1: 提取高亮区域
        vector<vector<Color_my>> bright = bloomBuffer;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float lum = 0.2126f * bright[y][x].r + 0.7152f * bright[y][x].g + 0.0722f * bright[y][x].b;
                if (lum < threshold) bright[y][x] = Color_my(0, 0, 0);
            }
        }

        // Step 2: 多级下采样
        vector<vector<vector<Color_my>>> mip(levels);
        vector<int> w(levels), h(levels);
        w[0] = width; h[0] = height;
        mip[0] = bright;

        for (int i = 1; i < levels; ++i) {
            w[i] = max(1, w[i - 1] / 2);
            h[i] = max(1, h[i - 1] / 2);
            mip[i].assign(h[i], vector<Color_my>(w[i], Color_my(0, 0, 0)));

            for (int y = 0; y < h[i]; ++y) {
                for (int x = 0; x < w[i]; ++x) {
                    int srcX = min(x * 2, w[i - 1] - 1);
                    int srcY = min(y * 2, h[i - 1] - 1);
                    Color_my sum(0, 0, 0);
                    sum += mip[i - 1][srcY][srcX];
                    if (srcX + 1 < w[i - 1]) sum += mip[i - 1][srcY][srcX + 1];
                    if (srcY + 1 < h[i - 1]) sum += mip[i - 1][srcY + 1][srcX];
                    if (srcX + 1 < w[i - 1] && srcY + 1 < h[i - 1]) sum += mip[i - 1][srcY + 1][srcX + 1];
                    mip[i][y][x] = sum * 0.25f;
                }
            }
        }

        // Step 3: 高斯模糊函数
        auto GaussianBlur = [](vector<vector<Color_my>>& buf, int radius) {
            int w = buf[0].size(), h = buf.size();
            vector<vector<Color_my>> temp = buf;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    Color_my sum(0, 0, 0);
                    float wSum = 0.0f;
                    for (int dy = -radius; dy <= radius; ++dy) {
                        for (int dx = -radius; dx <= radius; ++dx) {
                            int nx = x + dx;
                            int ny = y + dy;
                            if (nx < 0) nx = 0; if (nx >= w) nx = w - 1;
                            if (ny < 0) ny = 0; if (ny >= h) ny = h - 1;
                            float weight = exp(-(dx * dx + dy * dy) / (2.0f * radius * radius));
                            sum += temp[ny][nx] * weight;
                            wSum += weight;
                        }
                    }
                    buf[y][x] = sum / wSum;
                }
            }
            };

        // Step 4: 对每一级进行模糊
        for (int i = 1; i < levels; ++i) GaussianBlur(mip[i], 2);

        // Step 5: 上采样并叠加回上一层
        for (int i = levels - 1; i > 0; --i) {
            for (int y = 0; y < h[i - 1]; ++y) {
                for (int x = 0; x < w[i - 1]; ++x) {
                    int srcX = min(x / 2, w[i] - 1);
                    int srcY = min(y / 2, h[i] - 1);
                    mip[i - 1][y][x] = mip[i - 1][y][x] + mip[i][srcY][srcX];
                }
            }
        }

        // Step 6: 叠加回原始颜色
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                color[y][x] = (color[y][x] + mip[0][y][x] * intensity).clamp();
            }
        }
    }
};
#endif
