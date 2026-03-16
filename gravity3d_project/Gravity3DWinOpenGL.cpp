//The Diyarl Project
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <random>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

using std::max;
using std::min;
using std::shared_ptr;
using std::make_shared;
using std::string;
using std::vector;

const int WINDOW_WIDTH = 1400;
const int WINDOW_HEIGHT = 900;
const double G = 6.67430e-11;
const double C_LIGHT = 299792458.0;
const double AU = 1.496e11;
const double PI = 3.14159265358979323846;

struct Vec3 {
    double x, y, z;
    Vec3(double x_=0, double y_=0, double z_=0) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(double s) const { return {x/s, y/s, z/s}; }

    Vec3& operator+=(const Vec3& o) {
        x += o.x; y += o.y; z += o.z;
        return *this;
    }

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vec3 normalized() const {
        double m = length();
        return m > 0.0 ? *this / m : Vec3();
    }
};

struct Planet {
    string name;
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;
    double mass;
    double radiusMeters;
    float drawRadius;
    float color[3];
    vector<Vec3> trail;

    float spinAngle = 0.0f;
    float spinSpeed = 0.0f;
    float axialTilt = 0.0f;

    Planet(const string& n, Vec3 p, Vec3 v, double m, double r, float drawR,
           float cr, float cg, float cb,
           float spinSpd = 15.0f, float tilt = 0.0f)
        : name(n), position(p), velocity(v), mass(m), radiusMeters(r), drawRadius(drawR),
          spinSpeed(spinSpd), axialTilt(tilt) {
        color[0] = cr;
        color[1] = cg;
        color[2] = cb;
    }

    void applyForce(const Vec3& force) {
        acceleration += force / mass;
    }

    void update(double dt) {
        velocity += acceleration * dt;
        position += velocity * dt;
        acceleration = Vec3();

        spinAngle += spinSpeed * (float)dt;
        while (spinAngle >= 360.0f) spinAngle -= 360.0f;
        while (spinAngle < 0.0f) spinAngle += 360.0f;

        trail.push_back(position);
        if (trail.size() > 480) trail.erase(trail.begin());
    }
};

struct GravityEngine {
    vector<shared_ptr<Planet>> planets;
    bool relativity = true;

    void add(const shared_ptr<Planet>& p) {
        planets.push_back(p);
    }

    void calculateForces() {
        for (auto& p : planets) p->acceleration = Vec3();

        for (size_t i = 0; i < planets.size(); ++i) {
            for (size_t j = 0; j < planets.size(); ++j) {
                if (i == j) continue;

                Vec3 dir = planets[j]->position - planets[i]->position;
                double dist = dir.length();
                if (dist < 1.0) continue;

                double force = (G * planets[i]->mass * planets[j]->mass) / (dist * dist);

                if (relativity) {
                    double rs = (2.0 * G * planets[j]->mass) / (C_LIGHT * C_LIGHT);
                    force *= (1.0 + (3.0 * rs) / (2.0 * dist));
                }

                planets[i]->applyForce(dir.normalized() * force);
            }
        }
    }
};

struct Camera {
    Vec3 pos = Vec3(0.0, 320.0, 980.0);
    double yaw = -90.0;
    double pitch = -18.0;
    bool captureMouse = true;

    Vec3 forward() const {
        double cy = std::cos(yaw * PI / 180.0);
        double sy = std::sin(yaw * PI / 180.0);
        double cp = std::cos(pitch * PI / 180.0);
        double sp = std::sin(pitch * PI / 180.0);
        return Vec3(cy * cp, sp, sy * cp).normalized();
    }

    Vec3 right() const {
        Vec3 f = forward();
        Vec3 up(0, 1, 0);
        return Vec3(
            f.z * up.y - f.y * up.z,
            f.x * up.z - f.z * up.x,
            f.y * up.x - f.x * up.y
        ).normalized();
    }
};

class App {
public:
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
    bool running = true;
    bool paused = false;
    bool keys[256] = {};
    RECT clientRect{};
    GravityEngine engine;
    Camera camera;
    double simTime = 0.0;
    double daysPerSecond = 15.0;
    double worldScale = 72.0 / AU;
    POINT mouseCenter{};
    bool firstMouseFrame = true;
    bool focused = true;
    vector<Vec3> stars;

    void setupSolarSystem() {
        engine.add(make_shared<Planet>(
            "Sun", Vec3(0, 0, 0), Vec3(0, 0, 0),
            1.989e30, 696340000, 11.5f,
            1.0f, 0.58f, 0.12f,
            4.0f, 7.25f
        ));

        engine.add(make_shared<Planet>(
            "Mercury", Vec3(AU * 0.387, 0, 0), Vec3(0, 0, 47360),
            3.301e23, 2439700, 1.8f,
            0.62f, 0.62f, 0.65f,
            16.0f, 0.03f
        ));

        engine.add(make_shared<Planet>(
            "Venus", Vec3(AU * 0.723, 0, 0), Vec3(0, 0, 35020),
            4.867e24, 6051800, 2.8f,
            0.84f, 0.66f, 0.34f,
            -10.0f, 177.0f
        ));

        engine.add(make_shared<Planet>(
            "Earth", Vec3(AU * 1.000, 0, 0), Vec3(0, 0, 29780),
            5.972e24, 6371000, 3.0f,
            0.26f, 0.56f, 1.00f,
            40.0f, 23.44f
        ));

        engine.add(make_shared<Planet>(
            "Mars", Vec3(AU * 1.524, 0, 0), Vec3(0, 0, 24070),
            6.417e23, 3389500, 2.4f,
            0.82f, 0.37f, 0.25f,
            38.0f, 25.19f
        ));

        engine.add(make_shared<Planet>(
            "Jupiter", Vec3(AU * 5.203, 0, 0), Vec3(0, 0, 13070),
            1.898e27, 69911000, 6.1f,
            0.83f, 0.66f, 0.43f,
            85.0f, 3.13f
        ));

        engine.add(make_shared<Planet>(
            "Saturn", Vec3(AU * 9.537, 0, 0), Vec3(0, 0, 9680),
            5.683e26, 58232000, 5.4f,
            0.86f, 0.78f, 0.58f,
            75.0f, 26.73f
        ));
    }

    void generateStars() {
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> angle(0.0, 2.0 * PI);
        std::uniform_real_distribution<double> elev(-0.45 * PI, 0.45 * PI);
        std::uniform_real_distribution<double> radius(1400.0, 2200.0);

        for (int i = 0; i < 2400; ++i) {
            double a = angle(rng);
            double e = elev(rng);
            double r = radius(rng);
            stars.push_back(Vec3(
                std::cos(a) * std::cos(e) * r,
                std::sin(e) * r,
                std::sin(a) * std::cos(e) * r
            ));
        }
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        App* app = reinterpret_cast<App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        if (msg == WM_NCCREATE) {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = reinterpret_cast<App*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        if (!app) return DefWindowProc(hwnd, msg, wParam, lParam);

        switch (msg) {
            case WM_DESTROY:
                PostQuitMessage(0);
                app->running = false;
                return 0;

            case WM_KEYDOWN:
                if (wParam < 256) app->keys[wParam] = true;
                app->onKeyDown((int)wParam);
                return 0;

            case WM_KEYUP:
                if (wParam < 256) app->keys[wParam] = false;
                return 0;

            case WM_SETFOCUS:
                app->focused = true;
                app->firstMouseFrame = true;
                return 0;

            case WM_KILLFOCUS:
                app->focused = false;
                return 0;

            case WM_SIZE:
                app->resizeLOWORDHIWORD(LOWORD(lParam), HIWORD(lParam));
                return 0;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    bool createWindowAndGL(HINSTANCE hInst) {
        WNDCLASSA wc = {};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = "Gravity3DOpenGL";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClassA(&wc)) return false;

        hwnd = CreateWindowA(
            "Gravity3DOpenGL",
            "Gravity 3D - Free Camera OpenGL",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            WINDOW_WIDTH, WINDOW_HEIGHT,
            nullptr, nullptr, hInst, this
        );
        if (!hwnd) return false;

        hdc = GetDC(hwnd);

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pf = ChoosePixelFormat(hdc, &pfd);
        if (!pf) return false;
        if (!SetPixelFormat(hdc, pf, &pfd)) return false;

        hglrc = wglCreateContext(hdc);
        if (!hglrc) return false;
        if (!wglMakeCurrent(hdc, hglrc)) return false;

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        GetClientRect(hwnd, &clientRect);
        centerMouse();
        initGL();
        return true;
    }

    void initGL() {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_NORMALIZE);
        glShadeModel(GL_SMOOTH);
        glClearColor(0.01f, 0.02f, 0.06f, 1.0f);

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);

        GLfloat ambient[]  = {0.06f, 0.06f, 0.10f, 1.0f};
        GLfloat diffuse[]  = {1.00f, 0.72f, 0.38f, 1.0f};
        GLfloat specular[] = {1.00f, 0.85f, 0.65f, 1.0f};

        glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

        resizeLOWORDHIWORD(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    }

    void resizeLOWORDHIWORD(int w, int h) {
        if (w <= 0 || h <= 0) return;
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(70.0, double(w) / double(h), 0.1, 5000.0);
        glMatrixMode(GL_MODELVIEW);
    }

    void centerMouse() {
        RECT rect;
        GetClientRect(hwnd, &rect);
        POINT p{ (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
        ClientToScreen(hwnd, &p);
        mouseCenter = p;
        SetCursorPos(mouseCenter.x, mouseCenter.y);
    }

    Vec3 simToRender(const Vec3& p) const {
        return p * worldScale;
    }

    void onKeyDown(int key) {
        switch (key) {
            case VK_ESCAPE:
                running = false;
                DestroyWindow(hwnd);
                break;

            case 'P':
                paused = !paused;
                break;

            case 'R':
                engine.relativity = !engine.relativity;
                break;

            case 'M':
                camera.captureMouse = !camera.captureMouse;
                firstMouseFrame = true;
                if (camera.captureMouse) centerMouse();
                break;

            case VK_OEM_PLUS:
            case VK_ADD:
                daysPerSecond = min(300.0, daysPerSecond * 1.25);
                break;

            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                daysPerSecond = max(1.0, daysPerSecond / 1.25);
                break;
        }
    }

    void updateCamera(double dt) {
        if (focused && camera.captureMouse) {
            POINT p;
            GetCursorPos(&p);

            if (firstMouseFrame) {
                centerMouse();
                firstMouseFrame = false;
            } else {
                int dx = p.x - mouseCenter.x;
                int dy = p.y - mouseCenter.y;
                const double sens = 0.12;
                camera.yaw += dx * sens;
                camera.pitch -= dy * sens;
                camera.pitch = max(-89.0, min(89.0, camera.pitch));
                centerMouse();
            }
            ShowCursor(FALSE);
        } else {
            ShowCursor(TRUE);
        }

        double speed = (keys[VK_SHIFT] ? 260.0 : 110.0) * dt;
        Vec3 f = camera.forward();
        Vec3 r = camera.right();

        if (keys['W']) camera.pos += f * speed;
        if (keys['S']) camera.pos += f * -speed;
        if (keys['A']) camera.pos += r * -speed;
        if (keys['D']) camera.pos += r * speed;
        if (keys[VK_SPACE])   camera.pos.y += speed;
        if (keys[VK_CONTROL]) camera.pos.y -= speed;
    }

    void updateSim(double dt) {
        updateCamera(dt);

        if (!paused) {
            engine.calculateForces();
            double simDt = dt * 86400.0 * daysPerSecond;
            for (auto& p : engine.planets) p->update(simDt);
            simTime += simDt;
        }
    }

    void setCameraView() {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        Vec3 f = camera.forward();
        Vec3 center = camera.pos + f;

        gluLookAt(camera.pos.x, camera.pos.y, camera.pos.z,
                  center.x, center.y, center.z,
                  0.0, 1.0, 0.0);
    }

    void drawSphere(float radius, int slices, int stacks) {
        for (int i = 0; i < stacks; ++i) {
            double lat0 = PI * (-0.5 + double(i) / stacks);
            double z0 = std::sin(lat0), zr0 = std::cos(lat0);
            double lat1 = PI * (-0.5 + double(i + 1) / stacks);
            double z1 = std::sin(lat1), zr1 = std::cos(lat1);

            glBegin(GL_QUAD_STRIP);
            for (int j = 0; j <= slices; ++j) {
                double lng = 2.0 * PI * double(j) / slices;
                double x = std::cos(lng), y = std::sin(lng);

                glNormal3d(x * zr0, z0, y * zr0);
                glVertex3d(radius * x * zr0, radius * z0, radius * y * zr0);

                glNormal3d(x * zr1, z1, y * zr1);
                glVertex3d(radius * x * zr1, radius * z1, radius * y * zr1);
            }
            glEnd();
        }
    }

    void drawRing(float innerR, float outerR, int segments) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segments; ++i) {
            double a = 2.0 * PI * i / segments;
            double c = std::cos(a), s = std::sin(a);
            glNormal3f(0.f, 1.f, 0.f);
            glVertex3f((float)(innerR * c), 0.f, (float)(innerR * s));
            glVertex3f((float)(outerR * c), 0.f, (float)(outerR * s));
        }
        glEnd();
    }

    void drawStars() {
        glDisable(GL_LIGHTING);
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (size_t i = 0; i < stars.size(); ++i) {
            if (i % 11 == 0) glColor3f(0.65f, 0.78f, 1.0f);
            else glColor3f(0.92f, 0.93f, 1.0f);
            glVertex3d(stars[i].x, stars[i].y, stars[i].z);
        }
        glEnd();
        glEnable(GL_LIGHTING);
    }

    void drawTrails() {
        glDisable(GL_LIGHTING);
        for (const auto& p : engine.planets) {
            if (p->name == "Sun" || p->trail.size() < 2) continue;

            glColor4f(p->color[0], p->color[1], p->color[2], 0.75f);
            glBegin(GL_LINE_STRIP);
            for (const auto& t : p->trail) {
                Vec3 q = simToRender(t);
                glVertex3d(q.x, q.y, q.z);
            }
            glEnd();
        }
        glEnable(GL_LIGHTING);
    }

    void drawOrbitGuides() {
        glDisable(GL_LIGHTING);
        glColor3f(0.32f, 0.44f, 0.92f);

        const double aus[] = {0.387, 0.723, 1.0, 1.524, 5.203, 9.537};
        for (double au : aus) {
            double r = au * AU * worldScale;
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 180; ++i) {
                double a = 2.0 * PI * i / 180.0;
                glVertex3d(std::cos(a) * r, 0.0, std::sin(a) * r);
            }
            glEnd();
        }

        glEnable(GL_LIGHTING);
    }

    void drawGravityWell() {
        glDisable(GL_CULL_FACE);
        glEnable(GL_LIGHTING);

        const int grid = 220;
const float span = 2600.0f;
const float half = span * 0.5f;
const float baseDepth = 150.0f;
const float soft = 220.0f;
const float yOffset = -28.0f;

        auto depthFn = [&](float x, float z) {
            float d2 = x * x + z * z;
            float centerWell = -baseDepth / std::sqrt((d2 / soft) + 1.0f);

            float ripple = 0.0f;
            if (d2 > 1.0f) {
                float d = std::sqrt(d2);
                ripple = -4.0f * std::exp(-d / 420.0f) * std::cos(d * 0.055f);
            }

            return yOffset + centerWell + ripple;
        };

        for (int iz = 0; iz < grid - 1; ++iz) {
            float z0 = -half + span * iz / (grid - 1);
            float z1 = -half + span * (iz + 1) / (grid - 1);

            glBegin(GL_TRIANGLE_STRIP);
            for (int ix = 0; ix < grid; ++ix) {
                float x = -half + span * ix / (grid - 1);

                for (int row = 0; row < 2; ++row) {
                    float z = (row == 0) ? z0 : z1;
                    float y = depthFn(x, z);

                    float eps = 1.8f;
                    float dx = depthFn(x + eps, z) - depthFn(x - eps, z);
                    float dz = depthFn(x, z + eps) - depthFn(x, z - eps);

                    Vec3 n(-dx, 2.0f * eps, -dz);
                    n = n.normalized();
                    glNormal3d(n.x, n.y, n.z);

                    float glow = 0.55f + 0.45f * (float)std::exp(-(x * x + z * z) / 180000.0f);
                    glColor3f(0.05f * glow, 0.20f * glow, 0.72f * glow);

                    glVertex3f(x, y, z);
                }
            }
            glEnd();
        }

        glDisable(GL_LIGHTING);
        glColor3f(0.22f, 0.50f, 1.0f);

        for (int i = 0; i < grid; i += 5) {
            float z = -half + span * i / (grid - 1);
            glBegin(GL_LINE_STRIP);
            for (int ix = 0; ix < grid; ++ix) {
                float x = -half + span * ix / (grid - 1);
                glVertex3f(x, depthFn(x, z) + 0.15f, z);
            }
            glEnd();
        }

        for (int i = 0; i < grid; i += 5) {
            float x = -half + span * i / (grid - 1);
            glBegin(GL_LINE_STRIP);
            for (int iz = 0; iz < grid; ++iz) {
                float z = -half + span * iz / (grid - 1);
                glVertex3f(x, depthFn(x, z) + 0.15f, z);
            }
            glEnd();
        }
    }

    void drawPlanets() {
        GLfloat lightPos[] = {0.f, 0.f, 0.f, 1.f};
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

        for (const auto& p : engine.planets) {
            Vec3 q = simToRender(p->position);

            glPushMatrix();
            glTranslated(q.x, q.y, q.z);

            glRotatef(p->axialTilt, 0.f, 0.f, 1.f);
            glRotatef(p->spinAngle, 0.f, 1.f, 0.f);

            glColor3fv(p->color);

            if (p->name == "Sun") {
                glDisable(GL_LIGHTING);
                for (int i = 6; i >= 1; --i) {
                    float s = 1.0f + i * 0.16f;
                    glColor4f(1.0f, 0.42f, 0.10f, 0.06f * i);
                    drawSphere(p->drawRadius * s, 20, 18);
                }
                glEnable(GL_LIGHTING);
                glColor3fv(p->color);
                drawSphere(p->drawRadius, 32, 24);
            } else {
                if (p->name == "Saturn") {
                    glPushMatrix();
                    glDisable(GL_LIGHTING);
                    glColor4f(0.82f, 0.78f, 0.62f, 0.92f);
                    glRotatef(90.f, 1.f, 0.f, 0.f);
                    drawRing(p->drawRadius * 1.45f, p->drawRadius * 2.25f, 90);
                    glEnable(GL_LIGHTING);
                    glPopMatrix();
                }

                drawSphere(p->drawRadius, 26, 20);
            }

            glPopMatrix();
        }
    }

    void drawOverlayText() {
        HDC dc = GetDC(hwnd);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(235, 235, 235));

        char line1[256], line2[256], line3[256];
        std::snprintf(line1, sizeof(line1), "WASD move | Mouse look | Space/Ctrl up/down | Shift fast | M mouse lock | P pause | R relativity | +/- speed | Esc exit");
        std::snprintf(line2, sizeof(line2), "Time: %.2f years | Sim speed: %.1f days/sec | Mouse: %s | Relativity: %s",
                      simTime / (365.25 * 86400.0), daysPerSecond,
                      camera.captureMouse ? "LOCKED" : "FREE",
                      engine.relativity ? "ON" : "OFF");
        std::snprintf(line3, sizeof(line3), "Camera: x %.1f  y %.1f  z %.1f", camera.pos.x, camera.pos.y, camera.pos.z);

        TextOutA(dc, 12, 10, line1, (int)strlen(line1));
        TextOutA(dc, 12, 32, line2, (int)strlen(line2));
        TextOutA(dc, 12, 54, line3, (int)strlen(line3));
        ReleaseDC(hwnd, dc);
    }

    void render() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        setCameraView();
        drawStars();
        drawGravityWell();
        drawOrbitGuides();
        drawTrails();
        drawPlanets();
        SwapBuffers(hdc);
        drawOverlayText();
    }

    int run() {
        MSG msg{};
        LARGE_INTEGER freq{}, prev{}, now{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&prev);

        while (running) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) running = false;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            QueryPerformanceCounter(&now);
            double dt = double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart);
            prev = now;
            if (dt > 0.033) dt = 0.033;

            updateSim(dt);
            render();
            Sleep(1);
        }
        return 0;
    }

    void cleanup() {
        if (hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(hglrc);
            hglrc = nullptr;
        }
        if (hdc && hwnd) {
            ReleaseDC(hwnd, hdc);
            hdc = nullptr;
        }
    }
};

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    App app;
    app.setupSolarSystem();
    app.generateStars();

    if (!app.createWindowAndGL(hInst)) {
        MessageBoxA(nullptr, "Failed to create OpenGL window.", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    int result = app.run();
    app.cleanup();
    return result;
}
