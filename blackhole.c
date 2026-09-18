#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
   Minimal C replacements for the GLM types/functions used by the
   original C++ program. The rendering/physics logic is unchanged.
   ================================================================ */

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float x, y, z, w;
} Vec4;

typedef struct {
    /* Column-major, matching GLM/OpenGL memory layout. */
    float m[16];
} Mat4;

static Vec3 vec3_make(float x, float y, float z)
{
    Vec3 v = { x, y, z };
    return v;
}

static Vec4 vec4_make(float x, float y, float z, float w)
{
    Vec4 v = { x, y, z, w };
    return v;
}

static Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec3 vec3_scale(Vec3 a, float s)
{
    return vec3_make(a.x * s, a.y * s, a.z * s);
}

static float vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return vec3_make(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static float vec3_length(Vec3 a)
{
    return sqrtf(vec3_dot(a, a));
}

static Vec3 vec3_normalize(Vec3 a)
{
    float len = vec3_length(a);
    if (len == 0.0f) {
        return vec3_make(0.0f, 0.0f, 0.0f);
    }
    return vec3_scale(a, 1.0f / len);
}

static float clampf_value(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static float radiansf(float degrees)
{
    return degrees * ((float)M_PI / 180.0f);
}

static Mat4 mat4_zero(void)
{
    Mat4 r;
    memset(&r, 0, sizeof(r));
    return r;
}

static Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up)
{
    Vec3 f = vec3_normalize(vec3_sub(center, eye));
    Vec3 s = vec3_normalize(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);

    Mat4 result = mat4_zero();

    /* GLM-compatible column-major layout. */
    result.m[0]  = s.x;
    result.m[1]  = s.y;
    result.m[2]  = s.z;

    result.m[4]  = u.x;
    result.m[5]  = u.y;
    result.m[6]  = u.z;

    result.m[8]  = -f.x;
    result.m[9]  = -f.y;
    result.m[10] = -f.z;

    result.m[12] = -vec3_dot(s, eye);
    result.m[13] = -vec3_dot(u, eye);
    result.m[14] =  vec3_dot(f, eye);
    result.m[15] = 1.0f;

    return result;
}

static Mat4 mat4_perspective(float fovyRadians, float aspect,
                             float zNear, float zFar)
{
    float tanHalfFovy = tanf(fovyRadians * 0.5f);
    Mat4 result = mat4_zero();

    result.m[0]  = 1.0f / (aspect * tanHalfFovy);
    result.m[5]  = 1.0f / tanHalfFovy;
    result.m[10] = -(zFar + zNear) / (zFar - zNear);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);

    return result;
}

/* ================================================================
   Original global variables
   ================================================================ */

double lastPrintTime = 0.0;
int framesCount = 0;
double c = 299792458.0;
double G = 6.67430e-11;
bool Gravity = false;

/* Forward declarations. */
typedef struct Camera Camera;
typedef struct BlackHole BlackHole;
typedef struct ObjectData ObjectData;
typedef struct Engine Engine;

/* ================================================================
   Camera
   ================================================================ */

struct Camera {
    Vec3 target;
    float radius;
    float minRadius;
    float maxRadius;

    float azimuth;
    float elevation;

    float orbitSpeed;
    float panSpeed;
    double zoomSpeed;

    bool dragging;
    bool panning;
    bool moving;
    double lastX;
    double lastY;
};

static void Camera_Init(Camera *cam)
{
    cam->target = vec3_make(0.0f, 0.0f, 0.0f);
    cam->radius = 6.34194e10f;
    cam->minRadius = 1e10f;
    cam->maxRadius = 1e12f;

    cam->azimuth = 0.0f;
    cam->elevation = (float)(M_PI / 2.0);

    cam->orbitSpeed = 0.01f;
    cam->panSpeed = 0.01f;
    cam->zoomSpeed = 25e9f;

    cam->dragging = false;
    cam->panning = false;
    cam->moving = false;
    cam->lastX = 0.0;
    cam->lastY = 0.0;
}

static Vec3 Camera_Position(const Camera *cam)
{
    float clampedElevation = clampf_value(
        cam->elevation,
        0.01f,
        (float)M_PI - 0.01f
    );

    return vec3_make(
        cam->radius * sinf(clampedElevation) * cosf(cam->azimuth),
        cam->radius * cosf(clampedElevation),
        cam->radius * sinf(clampedElevation) * sinf(cam->azimuth)
    );
}

static void Camera_Update(Camera *cam)
{
    /* Always keep target at black hole center. */
    cam->target = vec3_make(0.0f, 0.0f, 0.0f);

    if (cam->dragging || cam->panning) {
        cam->moving = true;
    } else {
        cam->moving = false;
    }
}

static void Camera_ProcessMouseMove(Camera *cam, double x, double y)
{
    float dx = (float)(x - cam->lastX);
    float dy = (float)(y - cam->lastY);

    if (cam->dragging && cam->panning) {
        /* Pan: Shift + Left or Middle Mouse
           Disabled to keep camera centered on black hole. */
    }
    else if (cam->dragging && !cam->panning) {
        /* Orbit: Left mouse only */
        cam->azimuth += dx * cam->orbitSpeed;
        cam->elevation -= dy * cam->orbitSpeed;
        cam->elevation = clampf_value(
            cam->elevation,
            0.01f,
            (float)M_PI - 0.01f
        );
    }

    cam->lastX = x;
    cam->lastY = y;
    Camera_Update(cam);
}

static void Camera_ProcessMouseButton(Camera *cam, int button, int action,
                                      int mods, GLFWwindow *win)
{
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            cam->dragging = true;
            /* Disable panning so camera always orbits center. */
            cam->panning = false;
            glfwGetCursorPos(win, &cam->lastX, &cam->lastY);
        }
        else if (action == GLFW_RELEASE) {
            cam->dragging = false;
            cam->panning = false;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            Gravity = true;
        }
        else if (action == GLFW_RELEASE) {
            Gravity = false;
        }
    }
}

static void Camera_ProcessScroll(Camera *cam, double xoffset, double yoffset)
{
    (void)xoffset;
    cam->radius -= yoffset * cam->zoomSpeed;
    cam->radius = clampf_value(
        cam->radius,
        cam->minRadius,
        cam->maxRadius
    );
    Camera_Update(cam);
}

static void Camera_ProcessKey(Camera *cam, int key, int scancode,
                              int action, int mods)
{
    (void)cam;
    (void)scancode;
    (void)mods;

    if (action == GLFW_PRESS && key == GLFW_KEY_G) {
        Gravity = !Gravity;
        printf("[INFO] Gravity turned %s\n", Gravity ? "ON" : "OFF");
    }
}

/* ================================================================
   Black hole
   ================================================================ */

struct BlackHole {
    Vec3 position;
    double mass;
    double radius;
    double r_s;
};

static BlackHole BlackHole_Create(Vec3 pos, double mass)
{
    BlackHole bh;
    bh.position = pos;
    bh.mass = mass;
    bh.radius = 0.0;
    bh.r_s = 2.0 * G * bh.mass / (c * c);
    return bh;
}

static bool BlackHole_Intercept(const BlackHole *bh,
                                float px, float py, float pz)
{
    double dx = (double)px - (double)bh->position.x;
    double dy = (double)py - (double)bh->position.y;
    double dz = (double)pz - (double)bh->position.z;
    double dist2 = dx * dx + dy * dy + dz * dz;
    return dist2 < bh->r_s * bh->r_s;
}

BlackHole SagA = { {0.0f, 0.0f, 0.0f}, 8.54e36, 0.0, 0.0 };

/* ================================================================
   Object data / object array
   ================================================================ */

struct ObjectData {
    Vec4 posRadius;
    Vec4 color;
    float mass;
    Vec3 velocity;
};

#define MAX_OBJECTS 16

static ObjectData objects[MAX_OBJECTS];
static size_t objectCount = 0;

static void InitObjects(void)
{
    objectCount = 3;

    objects[0].posRadius = vec4_make(4e11f, 0.0f, 0.0f, 4e10f);
    objects[0].color = vec4_make(1, 1, 0, 1);
    objects[0].mass = 1.98892e30f;
    objects[0].velocity = vec3_make(0.0f, 0.0f, 0.0f);

    objects[1].posRadius = vec4_make(0.0f, 0.0f, 4e11f, 4e10f);
    objects[1].color = vec4_make(1, 0, 0, 1);
    objects[1].mass = 1.98892e30f;
    objects[1].velocity = vec3_make(0.0f, 0.0f, 0.0f);

    objects[2].posRadius = vec4_make(0.0f, 0.0f, 0.0f, (float)SagA.r_s);
    objects[2].color = vec4_make(0, 0, 0, 1);
    objects[2].mass = (float)SagA.mass;
    objects[2].velocity = vec3_make(0.0f, 0.0f, 0.0f);

    /* Original optional fourth object remains commented out. */
    /*
    objects[3].posRadius = vec4_make(6e10f, 0.0f, 0.0f, 5e10f);
    objects[3].color = vec4_make(0, 1, 0, 1);
    */
}

/* ================================================================
   Engine
   ================================================================ */

struct Engine {
    GLuint gridShaderProgram;

    GLFWwindow *window;
    GLuint quadVAO;
    GLuint texture;
    GLuint shaderProgram;
    GLuint computeProgram;

    GLuint cameraUBO;
    GLuint diskUBO;
    GLuint objectsUBO;

    GLuint gridVAO;
    GLuint gridVBO;
    GLuint gridEBO;
    int gridIndexCount;

    int WIDTH;
    int HEIGHT;
    int COMPUTE_WIDTH;
    int COMPUTE_HEIGHT;
    float width;
    float height;
};

static Engine engine = {0};

/* ================================================================
   Shader helpers
   ================================================================ */

static GLuint CompileShader(GLenum type, const char *source,
                            const char *label)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);

        char *log = (char *)malloc((size_t)logLen + 1);
        if (log) {
            glGetShaderInfoLog(shader, logLen, NULL, log);
            log[logLen] = '\0';
            fprintf(stderr, "Shader compile error (%s):\n%s\n",
                    label ? label : "shader", log);
            free(log);
        }

        glDeleteShader(shader);
        exit(EXIT_FAILURE);
    }

    return shader;
}

static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader,
                          const char *label)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

        char *log = (char *)malloc((size_t)logLen + 1);
        if (log) {
            glGetProgramInfoLog(program, logLen, NULL, log);
            log[logLen] = '\0';
            fprintf(stderr, "Shader link error (%s):\n%s\n",
                    label ? label : "program", log);
            free(log);
        }

        glDeleteProgram(program);
        exit(EXIT_FAILURE);
    }

    return program;
}

static GLuint CreateShaderProgram(void)
{
    static const char *vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\n";

    static const char *fragmentShaderSource =
        "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D screenTexture;\n"
        "void main() {\n"
        "    FragColor = texture(screenTexture, TexCoord);\n"
        "}\n";

    GLuint vertexShader = CompileShader(
        GL_VERTEX_SHADER, vertexShaderSource, "inline vertex shader"
    );
    GLuint fragmentShader = CompileShader(
        GL_FRAGMENT_SHADER, fragmentShaderSource, "inline fragment shader"
    );

    GLuint shaderProgram = LinkProgram(
        vertexShader, fragmentShader, "inline shader program"
    );

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

static char *ReadTextFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long fileSize = ftell(file);
    if (fileSize < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer = (char *)malloc((size_t)fileSize + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytesRead = fread(buffer, 1, (size_t)fileSize, file);
    fclose(file);

    buffer[bytesRead] = '\0';
    return buffer;
}

static GLuint LoadShaderFile(const char *path, GLenum type)
{
    char *source = ReadTextFile(path);
    if (!source) {
        fprintf(stderr, "Failed to open shader: %s\n", path);
        exit(EXIT_FAILURE);
    }

    GLuint shader = CompileShader(type, source, path);
    free(source);
    return shader;
}

static GLuint CreateGridShaderProgram(void)
{
    /* grid.vert - embedded directly into the C executable */
    static const char *gridVertexShaderSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "uniform mat4 viewProj;\n"
        "void main() {\n"
        "    gl_Position = viewProj * vec4(aPos, 1.0);\n"
        "}\n";

    /* grid.frag - embedded directly into the C executable */
    static const char *gridFragmentShaderSource =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = vec4(0.5, 0.5, 0.5, 0.7);\n"
        "}\n";

    GLuint vertShader = CompileShader(
        GL_VERTEX_SHADER, gridVertexShaderSource, "embedded grid.vert"
    );
    GLuint fragShader = CompileShader(
        GL_FRAGMENT_SHADER, gridFragmentShaderSource, "embedded grid.frag"
    );

    GLuint program = LinkProgram(
        vertShader, fragShader, "embedded grid shader program"
    );

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

static GLuint CreateShaderProgramFromFiles(const char *vertPath,
                                           const char *fragPath)
{
    GLuint vertShader = LoadShaderFile(vertPath, GL_VERTEX_SHADER);
    GLuint fragShader = LoadShaderFile(fragPath, GL_FRAGMENT_SHADER);

    GLuint program = LinkProgram(vertShader, fragShader, "file shader program");

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

static GLuint CreateComputeProgram(const char *path)
{
    GLuint cs = LoadShaderFile(path, GL_COMPUTE_SHADER);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);

        char *log = (char *)malloc((size_t)logLen + 1);
        if (log) {
            glGetProgramInfoLog(prog, logLen, NULL, log);
            log[logLen] = '\0';
            fprintf(stderr, "Compute shader link error:\n%s\n", log);
            free(log);
        }

        glDeleteProgram(prog);
        glDeleteShader(cs);
        exit(EXIT_FAILURE);
    }

    glDeleteShader(cs);
    return prog;
}

/* ================================================================
   Quad
   ================================================================ */

typedef struct {
    GLuint vao;
    GLuint texture;
} QuadResult;

static QuadResult QuadVAO(void)
{
    float quadVertices[] = {
        /* positions    texCoords */
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,

        -1.0f,  1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(quadVertices),
        quadVertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE,
        4 * (GLsizei)sizeof(float),
        (void *)0
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE,
        4 * (GLsizei)sizeof(float),
        (void *)(2 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        engine.COMPUTE_WIDTH,
        engine.COMPUTE_HEIGHT,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        NULL
    );

    QuadResult result = { VAO, texture };
    glBindVertexArray(0);
    return result;
}

/* ================================================================
   Grid generation / drawing
   ================================================================ */

static void Engine_GenerateGrid(Engine *eng)
{
    const int gridSize = 25;
    const float spacing = 1e10f;

    const size_t vertexCount = (size_t)(gridSize + 1) * (size_t)(gridSize + 1);
    const size_t indexCount = (size_t)gridSize * (size_t)gridSize * 4u;

    Vec3 *vertices = (Vec3 *)malloc(vertexCount * sizeof(Vec3));
    GLuint *indices = (GLuint *)malloc(indexCount * sizeof(GLuint));

    if (!vertices || !indices) {
        fprintf(stderr, "Failed to allocate grid data.\n");
        free(vertices);
        free(indices);
        exit(EXIT_FAILURE);
    }

    size_t vertexIndex = 0;

    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            float worldX = (x - gridSize / 2) * spacing;
            float worldZ = (z - gridSize / 2) * spacing;

            float y = 0.0f;

            /* Warp grid using Schwarzschild geometry. */
            for (size_t oi = 0; oi < objectCount; ++oi) {
                ObjectData *obj = &objects[oi];
                Vec3 objPos = vec3_make(
                    obj->posRadius.x,
                    obj->posRadius.y,
                    obj->posRadius.z
                );
                double mass = obj->mass;
                double radius = obj->posRadius.w;
                (void)radius;

                double r_s = 2.0 * G * mass / (c * c);
                double dx = (double)worldX - (double)objPos.x;
                double dz = (double)worldZ - (double)objPos.z;
                double dist = sqrt(dx * dx + dz * dz);

                /* Prevent sqrt of negative or divide-by-zero inside center. */
                if (dist > r_s) {
                    double deltaY = 2.0 * sqrt(r_s * (dist - r_s));
                    y += (float)deltaY - 3e10f;
                }
                else {
                    /* For points inside/at r_s: make it dip down sharply. */
                    y += 2.0f * (float)sqrt(r_s * r_s) - 3e10f;
                }
            }

            vertices[vertexIndex++] = vec3_make(worldX, y, worldZ);
        }
    }

    size_t indexIndex = 0;
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            int i = z * (gridSize + 1) + x;

            indices[indexIndex++] = (GLuint)i;
            indices[indexIndex++] = (GLuint)(i + 1);

            indices[indexIndex++] = (GLuint)i;
            indices[indexIndex++] = (GLuint)(i + gridSize + 1);
        }
    }

    if (eng->gridVAO == 0) glGenVertexArrays(1, &eng->gridVAO);
    if (eng->gridVBO == 0) glGenBuffers(1, &eng->gridVBO);
    if (eng->gridEBO == 0) glGenBuffers(1, &eng->gridEBO);

    glBindVertexArray(eng->gridVAO);

    glBindBuffer(GL_ARRAY_BUFFER, eng->gridVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        (GLsizeiptr)(vertexCount * sizeof(Vec3)),
        vertices,
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eng->gridEBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        (GLsizeiptr)(indexCount * sizeof(GLuint)),
        indices,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        (GLsizei)sizeof(Vec3),
        (void *)0
    );

    eng->gridIndexCount = (int)indexCount;

    glBindVertexArray(0);

    free(vertices);
    free(indices);
}

static void Engine_DrawGrid(Engine *eng, const Mat4 *viewProj)
{
    glUseProgram(eng->gridShaderProgram);
    glUniformMatrix4fv(
        glGetUniformLocation(eng->gridShaderProgram, "viewProj"),
        1,
        GL_FALSE,
        viewProj->m
    );

    glBindVertexArray(eng->gridVAO);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawElements(
        GL_LINES,
        eng->gridIndexCount,
        GL_UNSIGNED_INT,
        0
    );

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

static void Engine_DrawFullScreenQuad(Engine *eng)
{
    glUseProgram(eng->shaderProgram);
    glBindVertexArray(eng->quadVAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texture);
    glUniform1i(
        glGetUniformLocation(eng->shaderProgram, "screenTexture"),
        0
    );

    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
    glEnable(GL_DEPTH_TEST);
}

static void Engine_RenderScene(Engine *eng)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->shaderProgram);
    glBindVertexArray(eng->quadVAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eng->texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(eng->window);
    glfwPollEvents();
}

/* ================================================================
   UBO upload data structures
   ================================================================ */

typedef struct {
    Vec3 pos;     float _pad0;
    Vec3 right;   float _pad1;
    Vec3 up;      float _pad2;
    Vec3 forward; float _pad3;
    float tanHalfFov;
    float aspect;
    int moving;
    int _pad4;
} CameraUBOData;

typedef struct {
    int numObjects;
    float _pad0, _pad1, _pad2;
    Vec4 posRadius[MAX_OBJECTS];
    Vec4 color[MAX_OBJECTS];
    float mass[MAX_OBJECTS];
} ObjectsUBOData;

static void Engine_UploadCameraUBO(Engine *eng, const Camera *cam)
{
    CameraUBOData data;
    memset(&data, 0, sizeof(data));

    Vec3 camPos = Camera_Position(cam);
    Vec3 fwd = vec3_normalize(vec3_sub(cam->target, camPos));
    Vec3 up = vec3_make(0.0f, 1.0f, 0.0f);
    Vec3 right = vec3_normalize(vec3_cross(fwd, up));
    up = vec3_cross(right, fwd);

    data.pos = camPos;
    data.right = right;
    data.up = up;
    data.forward = fwd;
    data.tanHalfFov = tanf(radiansf(60.0f * 0.5f));
    data.aspect = (float)eng->WIDTH / (float)eng->HEIGHT;
    data.moving = (cam->dragging || cam->panning) ? 1 : 0;

    glBindBuffer(GL_UNIFORM_BUFFER, eng->cameraUBO);
    glBufferSubData(
        GL_UNIFORM_BUFFER,
        0,
        (GLsizeiptr)sizeof(data),
        &data
    );
}

static void Engine_UploadObjectsUBO(Engine *eng)
{
    ObjectsUBOData data;
    memset(&data, 0, sizeof(data));

    size_t count = objectCount < MAX_OBJECTS ? objectCount : MAX_OBJECTS;
    data.numObjects = (int)count;

    for (size_t i = 0; i < count; ++i) {
        data.posRadius[i] = objects[i].posRadius;
        data.color[i] = objects[i].color;
        data.mass[i] = objects[i].mass;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, eng->objectsUBO);
    glBufferSubData(
        GL_UNIFORM_BUFFER,
        0,
        (GLsizeiptr)sizeof(data),
        &data
    );
}

static void Engine_UploadDiskUBO(Engine *eng)
{
    float r1 = (float)(SagA.r_s * 2.2);
    float r2 = (float)(SagA.r_s * 5.2);
    float num = 2.0f;
    float thickness = 1e9f;
    float diskData[4] = { r1, r2, num, thickness };

    glBindBuffer(GL_UNIFORM_BUFFER, eng->diskUBO);
    glBufferSubData(
        GL_UNIFORM_BUFFER,
        0,
        (GLsizeiptr)sizeof(diskData),
        diskData
    );
}

/* ================================================================
   Compute dispatch
   ================================================================ */

static void Engine_DispatchCompute(Engine *eng, const Camera *cam)
{
    int cw = cam->moving ? eng->COMPUTE_WIDTH : 200;
    int ch = cam->moving ? eng->COMPUTE_HEIGHT : 150;

    glBindTexture(GL_TEXTURE_2D, eng->texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        cw,
        ch,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        NULL
    );

    glUseProgram(eng->computeProgram);
    Engine_UploadCameraUBO(eng, cam);
    Engine_UploadDiskUBO(eng);
    Engine_UploadObjectsUBO(eng);

    glBindImageTexture(
        0,
        eng->texture,
        0,
        GL_FALSE,
        0,
        GL_WRITE_ONLY,
        GL_RGBA8
    );

    GLuint groupsX = (GLuint)ceil((double)cw / 16.0);
    GLuint groupsY = (GLuint)ceil((double)ch / 16.0);
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

/* ================================================================
   Engine initialization
   ================================================================ */

static void Engine_Init(Engine *eng)
{
    memset(eng, 0, sizeof(*eng));

    eng->WIDTH = 800;
    eng->HEIGHT = 600;
    eng->COMPUTE_WIDTH = 200;
    eng->COMPUTE_HEIGHT = 150;
    eng->width = 100000000000.0f;
    eng->height = 75000000000.0f;

    if (!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    eng->window = glfwCreateWindow(
        eng->WIDTH,
        eng->HEIGHT,
        "Black Hole",
        NULL,
        NULL
    );

    if (!eng->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(eng->window);

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW: %s\n",
                (const char *)glewGetErrorString(glewErr));
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    printf("OpenGL %s\n", glGetString(GL_VERSION));

    eng->shaderProgram = CreateShaderProgram();
    eng->gridShaderProgram = CreateGridShaderProgram();
    eng->computeProgram = CreateComputeProgram("geodesic.comp");

    glGenBuffers(1, &eng->cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, eng->cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, eng->cameraUBO);

    glGenBuffers(1, &eng->diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, eng->diskUBO);
    glBufferData(
        GL_UNIFORM_BUFFER,
        (GLsizeiptr)(sizeof(float) * 4),
        NULL,
        GL_DYNAMIC_DRAW
    );
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, eng->diskUBO);

    glGenBuffers(1, &eng->objectsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, eng->objectsUBO);

    GLsizeiptr objUBOSize =
        (GLsizeiptr)sizeof(int) +
        (GLsizeiptr)(3 * sizeof(float)) +
        (GLsizeiptr)(MAX_OBJECTS * (sizeof(Vec4) + sizeof(Vec4))) +
        (GLsizeiptr)(MAX_OBJECTS * sizeof(float));

    glBufferData(
        GL_UNIFORM_BUFFER,
        objUBOSize,
        NULL,
        GL_DYNAMIC_DRAW
    );
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, eng->objectsUBO);

    QuadResult result = QuadVAO();
    eng->quadVAO = result.vao;
    eng->texture = result.texture;
}

/* ================================================================
   GLFW callback wrappers replacing the C++ lambdas
   ================================================================ */

static void MouseButtonCallback(GLFWwindow *win, int button,
                                int action, int mods)
{
    Camera *cam = (Camera *)glfwGetWindowUserPointer(win);
    Camera_ProcessMouseButton(cam, button, action, mods, win);
}

static void CursorPosCallback(GLFWwindow *win, double x, double y)
{
    Camera *cam = (Camera *)glfwGetWindowUserPointer(win);
    Camera_ProcessMouseMove(cam, x, y);
}

static void ScrollCallback(GLFWwindow *win, double xoffset, double yoffset)
{
    Camera *cam = (Camera *)glfwGetWindowUserPointer(win);
    Camera_ProcessScroll(cam, xoffset, yoffset);
}

static void KeyCallback(GLFWwindow *win, int key, int scancode,
                        int action, int mods)
{
    Camera *cam = (Camera *)glfwGetWindowUserPointer(win);
    Camera_ProcessKey(cam, key, scancode, action, mods);
}

static void SetupCameraCallbacks(GLFWwindow *window, Camera *camera)
{
    glfwSetWindowUserPointer(window, camera);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);
}

/* ================================================================
   MAIN
   ================================================================ */

int main(void)
{
    Camera camera;
    Camera_Init(&camera);

    SagA = BlackHole_Create(vec3_make(0.0f, 0.0f, 0.0f), 8.54e36);
    InitObjects();
    Engine_Init(&engine);

    SetupCameraCallbacks(engine.window, &camera);

    unsigned char *pixels = (unsigned char *)malloc(
        (size_t)engine.WIDTH * (size_t)engine.HEIGHT * 3u
    );

    double t0 = glfwGetTime();
    lastPrintTime = t0;

    double lastTime = glfwGetTime();
    int renderW = 800;
    int renderH = 600;
    int numSteps = 80000;
    (void)t0;
    (void)renderW;
    (void)renderH;
    (void)numSteps;

    while (!glfwWindowShouldClose(engine.window)) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;
        (void)dt;

        /* --------------------------------------------------------
           Gravity
           -------------------------------------------------------- */
        for (size_t i = 0; i < objectCount; ++i) {
            ObjectData *obj = &objects[i];

            for (size_t j = 0; j < objectCount; ++j) {
                ObjectData *obj2 = &objects[j];

                if (i == j) continue; /* skip self-interaction */

                float dx = obj2->posRadius.x - obj->posRadius.x;
                float dy = obj2->posRadius.y - obj->posRadius.y;
                float dz = obj2->posRadius.z - obj->posRadius.z;
                float distance = sqrtf(dx * dx + dy * dy + dz * dz);

                if (distance > 0.0f) {
                    /* Original direction vector was vector<double>, but
                       the components came from float arithmetic. */
                    float directionX = dx / distance;
                    float directionY = dy / distance;
                    float directionZ = dz / distance;

                    double Gforce =
                        (G * (double)obj->mass * (double)obj2->mass) /
                        ((double)distance * (double)distance);

                    double acc1 = Gforce / (double)obj->mass;
                    double accX = (double)directionX * acc1;
                    double accY = (double)directionY * acc1;
                    double accZ = (double)directionZ * acc1;

                    if (Gravity) {
                        obj->velocity.x += (float)accX;
                        obj->velocity.y += (float)accY;
                        obj->velocity.z += (float)accZ;

                        obj->posRadius.x += obj->velocity.x;
                        obj->posRadius.y += obj->velocity.y;
                        obj->posRadius.z += obj->velocity.z;

                        printf("velocity: %g, %g, %g\n",
                               obj->velocity.x,
                               obj->velocity.y,
                               obj->velocity.z);
                    }
                }
            }
        }

        /* --------------------------------------------------------
           GRID
           -------------------------------------------------------- */
        Engine_GenerateGrid(&engine);

        Vec3 camPos = Camera_Position(&camera);
        Mat4 view = mat4_look_at(
            camPos,
            camera.target,
            vec3_make(0.0f, 1.0f, 0.0f)
        );

        Mat4 proj = mat4_perspective(
            radiansf(60.0f),
            (float)engine.COMPUTE_WIDTH / (float)engine.COMPUTE_HEIGHT,
            1e9f,
            1e14f
        );

        /* viewProj = proj * view in the original code. */
        /* Both matrices are not multiplied here because the grid shader
           only receives the final matrix. Implement the multiplication. */
        Mat4 viewProj = mat4_zero();
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float value = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    value += proj.m[k * 4 + row] * view.m[col * 4 + k];
                }
                viewProj.m[col * 4 + row] = value;
            }
        }

        Engine_DrawGrid(&engine, &viewProj);

        /* --------------------------------------------------------
           RUN RAYTRACER
           -------------------------------------------------------- */
        glViewport(0, 0, engine.WIDTH, engine.HEIGHT);
        Engine_DispatchCompute(&engine, &camera);
        Engine_DrawFullScreenQuad(&engine);

        /* --------------------------------------------------------
           PRESENT TO SCREEN
           -------------------------------------------------------- */
        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    free(pixels);
    glfwDestroyWindow(engine.window);
    glfwTerminate();

    return 0;
}
