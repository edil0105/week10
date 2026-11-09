// =====================================================================
//  КОМПЬЮТЕРЛІК ГРАФИКА — бір файлдық жоба
//
//  1-АПТА:    терезе, фон анимациясы, FPS
//  СЕМИНАР 4: uniform, dt, цикл "жаңарту (update)" және "сызу (draw)"
//  5-АПТА:    индекс буфері (EBO), glDrawElements
//  7-АПТА:    координаттар кеңістіктері, MVP (uModel/uView/uProjection)
//  8-АПТА:    тереңдік буфері, back-face culling, бес куб
//  9-АПТА:    камера моделі, LookAt, пернетақтамен жылжу
//  10-АПТА:   толық еркін камера — тінтуір (yaw/pitch), дөңгелек (fov)
//             - базалық:     yaw/pitch/lastX/lastY/firstMouse/fov,
//                            mouse_callback, scroll_callback,
//                            perspective(radians(fov), ...)
//             - 1-тапсырма:  L — pitch шектеуін өшіру/қосу (тәжірибе)
//             - 2-тапсырма:  sensitivity = 0.1f (README-де неге)
//             - 3-тапсырма:  I — invert Y
//             - 4-тапсырма:  P — камераның толық күйі + |camFront|
//             - 5-тапсырма:  fov шегі 1..90
//             - қосымша:     Қ1 Shift — жүгіру, Қ2 терезе тақырыбы,
//                            Қ3 R — бастапқы күй, Қ4 yaw-ды шектеу
//
//  Басқару:
//      Тінтуір     — жан-жаққа қарау
//      Дөңгелек    — жақындату / алыстату (fov)
//      W / S       — алға / артқа (камера қараған жаққа)
//      A / D       — солға / оңға
//      Space       — жоғары
//      Left Ctrl   — төмен
//      Left Shift  — жүгіру (басулы тұрғанда, жылдамдық × 2)
//      I           — invert Y
//      P           — камераның күйін консольге шығару
//      R           — камераны бастапқы күйге қайтару
//      L           — pitch шектеуін өшіру/қосу (1-тапсырма)
//      M           — тінтуір режимі: FPS (курсор жасырын) / сүйреу (ЛКМ басулы)
//      F12         — скриншот (screenshots/ бумасына)
//      ESC         — шығу
// =====================================================================

#include <glad/gl.h>      // МІНДЕТТІ: glad әрқашан GLFW-дан БҰРЫН
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cmath>          // cos, sin — yaw/pitch-тен бағыт есептеу
#include <cstdio>         // snprintf, printf
#include <cstdlib>        // getenv (WSL-ді анықтау)
#include <cstring>        // strcmp
#include <iostream>
#include <string>
#include <vector>

#ifndef SCREENSHOT_DIR
#define SCREENSHOT_DIR "screenshots"
#endif

// ---------------------------------------------------------------------
//  Баптаулар
// ---------------------------------------------------------------------
int gWidth = 800, gHeight = 600;

// ---------------------------------------------------------------------
//  9-АПТА: КАМЕРА (pos / front / up)
// ---------------------------------------------------------------------
struct Camera {
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
};

const Camera START_CAMERA = {
    glm::vec3(0.0f, 0.0f,  3.0f),
    glm::vec3(0.0f, 0.0f, -1.0f),
    glm::vec3(0.0f, 1.0f,  0.0f)
};

Camera freeCam = START_CAMERA;
glm::vec3& camPos   = freeCam.pos;
glm::vec3& camFront = freeCam.front;
glm::vec3& camUp    = freeCam.up;     // әрқашан (0,1,0) — өзгертпейміз

// ---------------------------------------------------------------------
//  10-АПТА, 1-қадам: жаңа айнымалылар
//  Callback-тер осы айнымалыларды қолданады, сондықтан олар файлдың
//  басында, callback-терден ЖОҒАРЫ тұруы керек.
// ---------------------------------------------------------------------
const float START_YAW   = -90.0f;   // 0 болса, камера +X-ке қарайды — куб көрінбейді
const float START_PITCH =   0.0f;
const float START_FOV   =  45.0f;

float yaw   = START_YAW;
float pitch = START_PITCH;
float lastX = 400.0f;
float lastY = 300.0f;
bool  firstMouse = true;
float fov   = START_FOV;

// 5-тапсырма: fov шектері (жоғарғы шек 45-тен 90-ға көтерілді)
const float FOV_MIN =  1.0f;
const float FOV_MAX = 90.0f;

// 1-тапсырма: pitch шектеуі. L пернесімен өшіріп, тәжірибе жасаймыз.
// (Тапсырмада екі жолды комментарийге алу айтылған — бұл сол екі жол,
//  тек қайта құрастырмай ауыстыру үшін шартпен оралған.)
bool pitchLimit = true;

// 3-тапсырма: invert Y
bool invertY = false;

// Тінтуір режимі (M пернесі):
//  false — әдістемедегідей: GLFW_CURSOR_DISABLED, курсор жасырын, терезеде ұсталады;
//  true  — "сүйреу": курсор қалыпты, камера тек сол жақ батырма (ЛКМ) басулы тұрғанда бұрылады.
// WSL-де (WSLg) GLFW курсорды терезе ортасына қайтара алмайды: ығысу әр жолы ортадан
// есептеліп, жиналып қалады да, камера өздігінен айнала береді. Сондықтан WSL-де
// бағдарлама сүйреу режимінде басталады (WSL_DISTRO_NAME айнымалысы бар болса).
bool dragMode = std::getenv("WSL_DISTRO_NAME") != nullptr;

// Бір рет басылатын пернелер үшін алдыңғы күй (3-тапсырмадағы әдіс)
bool iPressedLast   = false;
bool pPressedLast   = false;
bool rPressedLast   = false;
bool lPressedLast   = false;
bool mPressedLast   = false;
bool f12PressedLast = false;

float angle = 0.0f;   // кубтардың айналу бұрышы (радиан)

// ---------------------------------------------------------------------
//  yaw / pitch -> camFront.  mouse_callback та, R пернесі де қолданады.
// ---------------------------------------------------------------------
void updateCameraFront() {
    glm::vec3 dir;
    // cos пен sin РАДИАН күтеді, ал yaw/pitch бізде градуспен
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(dir);
}

// ---------------------------------------------------------------------
//  3-қадам: mouse_callback — тінтуір жылжыған сайын шақырылады
// ---------------------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    // Сүйреу режимі: ЛКМ басылмаған кезде позицияны тек есте сақтаймыз —
    // батырманы басқан сәтте камера "секірмейді"
    if (dragMode && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }

    // Бірінші оқиғада lastX/lastY әлі терезе ортасы (400, 300), ал
    // курсор басқа жерде болуы мүмкін — камера "лақтырылмасын" деп
    // бірінші нүктені тек есте сақтаймыз.
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;   // экранның y-і төмен өседі, pitch — жоғары
    if (invertY) yoffset = -yoffset;       // 3-тапсырма
    lastX = (float)xpos;
    lastY = (float)ypos;

    // 2-тапсырма: 0.03 — тым баяу, 0.5 — тым секірмелі, 0.1 — таңдалды
    const float sensitivity = 0.1f;
    yaw   += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    // Қосымша Қ4: yaw шексіз өспесін (бағыт 360°-та қайталанады)
    if (yaw >  360.0f) yaw -= 360.0f;
    if (yaw < -360.0f) yaw += 360.0f;

    // Шектеу: ±90°-та camFront camUp-қа параллель болып, lookAt бұзылады
    if (pitchLimit) {
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    } else {
        // 1-тапсырма: 90°-қа жеткен сәтті көру үшін
        std::cout << "pitch = " << pitch << std::endl;
    }

    updateCameraFront();
}

// ---------------------------------------------------------------------
//  4-қадам: scroll_callback — дөңгелек. Алға айналдыру (yoffset > 0)
//  fov-ты кішірейтеді, яғни ЖАҚЫНДАТАДЫ.
// ---------------------------------------------------------------------
void scroll_callback(GLFWwindow*, double, double yoffset) {
    fov -= (float)yoffset;
    if (fov < FOV_MIN) fov = FOV_MIN;
    if (fov > FOV_MAX) fov = FOV_MAX;
}

void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    if (width == 0 || height == 0) return;
    gWidth  = width;
    gHeight = height;
    glViewport(0, 0, width, height);
}

// ---------------------------------------------------------------------
//  Кадр буферін PNG-ге сақтау (скриншоттар үшін)
// ---------------------------------------------------------------------
void saveScreenshot(const std::string& path) {
    std::vector<unsigned char> pixels((size_t)gWidth * gHeight * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, gWidth, gHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);   // OpenGL-де (0,0) — төменгі сол бұрыш
    if (stbi_write_png(path.c_str(), gWidth, gHeight, 3, pixels.data(), gWidth * 3))
        std::cout << "Скриншот: " << path << "\n";
    else
        std::cerr << "Скриншот сақталмады: " << path << "\n";
}

// Бір рет басу: перне басылған СӘТТЕ ғана true (ұстап тұрса — қайталанбайды)
bool keyPressedOnce(GLFWwindow* window, int key, bool& pressedLast) {
    bool now = glfwGetKey(window, key) == GLFW_PRESS;
    bool fired = now && !pressedLast;
    pressedLast = now;
    return fired;
}

// Курсор режимін қолдану: FPS — жасырын әрі ұсталған, сүйреу — қалыпты курсор
void applyMouseMode(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, dragMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    firstMouse = true;   // режим ауысқанда бірінші нүктені қайта есте сақтаймыз
}

void resetCamera() {
    freeCam = START_CAMERA;
    yaw   = START_YAW;
    pitch = START_PITCH;
    fov   = START_FOV;
    updateCameraFront();
}

void printCameraState() {
    std::printf("camPos   = (%.2f, %.2f, %.2f)\n", camPos.x, camPos.y, camPos.z);
    std::printf("camFront = (%.2f, %.2f, %.2f)\n", camFront.x, camFront.y, camFront.z);
    std::printf("yaw = %.1f   pitch = %.1f   fov = %.1f\n", yaw, pitch, fov);
    std::cout << "length = " << glm::length(camFront) << std::endl;
}

// ---------------------------------------------------------------------
//  Пернетақта — әр кадрда шақырылады
// ---------------------------------------------------------------------
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // --- Жылжу (9-апта). Басулы ТҰРҒАНДА жұмыс істейді ---
    float camSpeed = 2.5f * dt;
    // Қ1: Shift басулы — екі есе жылдам
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camSpeed *= 2.0f;

    glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= camSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += camSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camPos += camSpeed * glm::vec3(0.0f, 1.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camPos -= camSpeed * glm::vec3(0.0f, 1.0f, 0.0f);

    // --- Бір рет басылатын пернелер (алдыңғы күйді сақтаймыз) ---
    if (keyPressedOnce(window, GLFW_KEY_I, iPressedLast)) {          // 3-тапсырма
        invertY = !invertY;
        std::cout << "invertY = " << (invertY ? "true" : "false") << "\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_P, pPressedLast))            // 4-тапсырма
        printCameraState();
    if (keyPressedOnce(window, GLFW_KEY_R, rPressedLast)) {          // Қ3
        resetCamera();
        std::cout << "Камера бастапқы күйде\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_L, lPressedLast)) {          // 1-тапсырма
        pitchLimit = !pitchLimit;
        if (pitchLimit) {   // шектеуді қайтарғанда pitch-ті де ±89-ға кіргіземіз
            if (pitch >  89.0f) pitch =  89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
            updateCameraFront();
        }
        std::cout << "pitch шектеуі: " << (pitchLimit ? "ҚОСУЛЫ (±89°)" : "ӨШІРУЛІ") << "\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_M, mPressedLast)) {
        dragMode = !dragMode;
        applyMouseMode(window);
        std::cout << "Тінтуір режимі: " << (dragMode ? "сүйреу (ЛКМ басулы)" : "FPS") << "\n";
    }
    if (keyPressedOnce(window, GLFW_KEY_F12, f12PressedLast)) {
        static int shot = 1;
        saveScreenshot(std::string(SCREENSHOT_DIR) + "/shot-" + std::to_string(shot++) + ".png");
    }
}

// ---------------------------------------------------------------------
//  Шейдерлер — MVP (7-апта)
// ---------------------------------------------------------------------
const char* vertexShaderSrc = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vColor;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    vColor = aColor;
}
)glsl";

const char* fragmentShaderSrc = R"glsl(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)glsl";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

GLuint createShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ---------------------------------------------------------------------
//  --screenshots режимі: README-дегі төрт скриншотты автоматты түрде
//  түсіреді. Әр қадам — камераның күйі (тінтуірмен қол жеткізуге болатын)
//  және файл аты.
// ---------------------------------------------------------------------
struct ShotStep {
    const char* file;
    glm::vec3   pos;
    float       yaw, pitch, fov;
    bool        limit;
};

const ShotStep SHOTS[] = {
    {"01-default.png",    glm::vec3(0.0f,  0.0f, 3.0f), -90.0f,  0.0f, 45.0f, true },
    {"02-looking-up.png", glm::vec3(0.0f, -1.2f, 4.0f), -90.0f, 35.0f, 45.0f, true },
    {"03-zoomed.png",     glm::vec3(0.0f,  0.0f, 3.0f), -90.0f,  0.0f, 25.0f, true },
    {"04-broken.png",     glm::vec3(0.5f,  0.0f, 3.0f), -90.0f, 90.0f, 45.0f, false},
};

// ---------------------------------------------------------------------
//  Бағдарлар: еден-торы, айнала кубтар сақинасы, прицел.
//  Қай жаққа бұрылсаң да бірдеңе көрінеді — камераның бұрылғанын байқау оңай.
// ---------------------------------------------------------------------
const float FLOOR_Y   = -1.5f;   // еден биіктігі
const int   GRID_HALF = 20;      // тор -20..+20
const int   RING_N    = 12;      // сақинадағы кубтар саны
const float RING_R    = 10.0f;   // сақина радиусы

// Тор сызықтары (x y z  r g b): X осі — қызыл, Z осі — көк, әр 5-сызық ашығырақ
std::vector<float> makeGrid() {
    std::vector<float> v;
    auto line = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
        v.insert(v.end(), {a.x, a.y, a.z, c.r, c.g, c.b, b.x, b.y, b.z, c.r, c.g, c.b});
    };
    float h = (float)GRID_HALF;
    for (int i = -GRID_HALF; i <= GRID_HALF; i++) {
        float t = (float)i;
        glm::vec3 c = (i % 5 == 0) ? glm::vec3(0.45f) : glm::vec3(0.25f);
        line({t, FLOOR_Y, -h}, {t, FLOOR_Y, h}, i == 0 ? glm::vec3(0.2f, 0.4f, 1.0f) : c);  // Z бойымен
        line({-h, FLOOR_Y, t}, {h, FLOOR_Y, t}, i == 0 ? glm::vec3(1.0f, 0.3f, 0.3f) : c);  // X бойымен
    }
    return v;
}

// Экран ортасындағы прицел — бірден NDC-де (ақ түсті)
const float crosshair[] = {
    -0.03f,  0.0f,  0.0f,   1.0f, 1.0f, 1.0f,
     0.03f,  0.0f,  0.0f,   1.0f, 1.0f, 1.0f,
     0.0f,  -0.03f, 0.0f,   1.0f, 1.0f, 1.0f,
     0.0f,   0.03f, 0.0f,   1.0f, 1.0f, 1.0f,
};

// VAO + VBO жасау: барлық деректер бірдей форматта (xyz + rgb)
GLuint makeVao(const float* data, size_t bytes, GLuint& vbo) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return vao;
}

// =====================================================================
//  MAIN
// =====================================================================
int main(int argc, char** argv) {
    bool screenshotMode = (argc > 1 && std::strcmp(argv[1], "--screenshots") == 0);

    if (!glfwInit()) {
        std::cerr << "GLFW іске қосылмады\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(gWidth, gHeight,
                                          "Компьютерлік графика — 10-апта",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Терезе жасалмады. Видеокарта OpenGL 3.3-ті "
                     "қолдамауы мүмкін.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "GLAD жүктелмеді\n";
        glfwTerminate();
        return -1;
    }

    // 2-қадам: курсорды ұстау + callback-терді тіркеу (GLAD-тан кейін).
    // Әдепкіде GLFW_CURSOR_DISABLED; WSL-де — сүйреу режимі (applyMouseMode ішінде)
    applyMouseMode(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    // "Шикі" (raw) қозғалыс: жүйенің тінтуір үдеуінсіз — бұрылыс бірқалыпты
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    glfwGetFramebufferSize(window, &gWidth, &gHeight);
    glViewport(0, 0, gWidth, gHeight);

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "Тінтуір — қарау, дөңгелек — zoom, WASD — жүру, Space/Ctrl — жоғары/төмен,\n"
                 "Shift — жүгіру, I — invert Y, P — күй, R — бастапқы күй,\n"
                 "L — pitch шектеуі, M — тінтуір режимі, F12 — скриншот, ESC — шығу\n";
    if (dragMode)
        std::cout << "WSL: сүйреу режимі — камераны бұру үшін ЛКМ басып тұрып тартыңыз (M — ауыстыру)\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    GLuint shaderProgram = createShaderProgram();
    GLint  locModel = glGetUniformLocation(shaderProgram, "uModel");
    GLint  locView  = glGetUniformLocation(shaderProgram, "uView");
    GLint  locProj  = glGetUniformLocation(shaderProgram, "uProjection");

    // Куб: 36 вершина, xyz + rgb, CCW (8-апта)
    float vertices[] = {
        // артқы қыр (z = -0.5) — қызыл
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,
        // алдыңғы қыр (z = +0.5) — жасыл
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,
        // сол қыр (x = -0.5) — көк
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        // оң қыр (x = +0.5) — сары
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
        // астыңғы қыр (y = -0.5) — көгілдір
        -0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
        // үстіңгі қыр (y = +0.5) — күлгін
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 1.0f
    };

    GLuint vbo, gridVbo, crossVbo;
    GLuint vao = makeVao(vertices, sizeof(vertices), vbo);

    std::vector<float> grid = makeGrid();
    int    gridVerts = (int)(grid.size() / 6);
    GLuint gridVao  = makeVao(grid.data(), grid.size() * sizeof(float), gridVbo);
    GLuint crossVao = makeVao(crosshair, sizeof(crosshair), crossVbo);

    // Сахна — 8-аптадағы бес куб
    glm::vec3 positions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f),
        glm::vec3( 2.0f,  0.5f, -2.0f),
        glm::vec3(-1.8f, -0.3f, -1.0f),
        glm::vec3( 1.2f, -0.8f, -3.5f),
        glm::vec3(-2.2f,  1.0f, -4.0f)
    };

    double lastFrame = glfwGetTime();
    int    frame     = 0;
    int    shotIndex = 0;
    const int SHOT_COUNT = sizeof(SHOTS) / sizeof(SHOTS[0]);

    while (!glfwWindowShouldClose(window)) {
        double currentFrame = glfwGetTime();
        float  dt = (float)(currentFrame - lastFrame);
        lastFrame = currentFrame;

        // ------------------------- ЖАҢАРТУ ---------------------------
        if (screenshotMode) {
            // Кубтар әр скриншотта бірдей бұрышта тұрсын
            angle = 0.6f;
            const ShotStep& s = SHOTS[shotIndex];
            camPos = s.pos; yaw = s.yaw; pitch = s.pitch; fov = s.fov;
            pitchLimit = s.limit;
            updateCameraFront();
        } else {
            angle += dt;
            processInput(window, dt);
        }

        // view: камера тұрған жерден camFront бағытына қарайды (9-апта)
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

        // 5-қадам: проекция fov-қа байланған (тұрақты 45.0f емес)
        glm::mat4 projection = glm::perspective(glm::radians(fov),
            (float)gWidth / (float)gHeight, 0.1f, 100.0f);

        // Қ2: ағымдағы бұрыштар терезе тақырыбында
        char buf[128];
        std::snprintf(buf, sizeof(buf), "yaw=%.1f  pitch=%.1f  fov=%.1f%s%s",
                      yaw, pitch, fov,
                      invertY ? "  [invert Y]" : "",
                      pitchLimit ? "" : "  [pitch шектеусіз]");
        glfwSetWindowTitle(window, buf);

        // --------------------------- СЫЗУ ----------------------------
        glClearColor(0.10f, 0.12f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));
        glBindVertexArray(vao);

        for (int i = 0; i < 5; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, positions[i]);
            model = glm::rotate(model, angle * (0.5f + i * 0.3f), glm::vec3(0.3f, 1.0f, 0.0f));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Айнала кубтар сақинасы (центр — координаттар басы)
        for (int i = 0; i < RING_N; i++) {
            float a = glm::radians(360.0f / RING_N * i);
            glm::vec3 p(RING_R * cos(a), FLOOR_Y + 0.5f + (i % 3), RING_R * sin(a));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), p);
            model = glm::rotate(model, angle * 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));
            glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Еден-торы: model = бірлік матрица (тор әлемдік координаттарда берілген)
        glm::mat4 identity(1.0f);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(identity));
        glBindVertexArray(gridVao);
        glDrawArrays(GL_LINES, 0, gridVerts);

        // Прицел: бірден NDC-де, view/projection жоқ (бірлік матрицалар).
        // X бойынша aspect-ке қысамыз — крест шаршы болып көрінсін.
        // Тереңдік тестін өшіреміз — прицел әрқашан бәрінің үстінде.
        float aspect = (float)gWidth / (float)gHeight;
        glm::mat4 crossModel = glm::scale(identity, glm::vec3(1.0f / aspect, 1.0f, 1.0f));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(crossModel));
        glUniformMatrix4fv(locView,  1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(locProj,  1, GL_FALSE, glm::value_ptr(identity));
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(crossVao);
        glDrawArrays(GL_LINES, 0, 4);
        glEnable(GL_DEPTH_TEST);

        // Скриншот режимі: бірнеше кадрдан кейін (терезе орнықсын) түсіреміз
        if (screenshotMode && ++frame % 10 == 0) {
            const ShotStep& s = SHOTS[shotIndex];
            std::printf("%s: pitch=%.1f view[0][0]=%f\n", s.file, pitch, view[0][0]);
            saveScreenshot(std::string(SCREENSHOT_DIR) + "/" + s.file);
            if (++shotIndex == SHOT_COUNT) glfwSetWindowShouldClose(window, true);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &gridVao);
    glDeleteVertexArrays(1, &crossVao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &gridVbo);
    glDeleteBuffers(1, &crossVbo);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
