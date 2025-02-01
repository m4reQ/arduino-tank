#include "gui.h"
#include <raylib.h>
#include <raymath.h>

static Texture2D s_ArrowTexture;
static Texture2D s_ArrowOverlayTexture;
static Texture2D s_HullTexture;
static Texture2D s_HeadSensorTexture;
static Texture2D s_SensorIndicatorTexture;
static Texture2D s_TrackTexture;
static uint32_t s_WindowWidth;
static uint32_t s_WindowHeight;

static void LoadAssets(void)
{
    s_ArrowTexture = LoadTexture("./assets/images/arrow.png");
    s_ArrowOverlayTexture = LoadTexture("./assets/images/arrow_overlay.png");
    s_HeadSensorTexture = LoadTexture("./assets/images/head_sensor.png");
    s_HullTexture = LoadTexture("./assets/images/hull.png");
    s_SensorIndicatorTexture = LoadTexture("./assets/images/sensor_indicator.png");
    s_TrackTexture = LoadTexture("./assets/images/track.png");
}

static void DrawFrametimeText(const ApplicationState *state)
{
    char text[32];
    snprintf(text, sizeof(text), "Frametime: %.2f ms", state->frametime * 1000.0f);
    DrawText(text, 10, 10, 34, GREEN);
}

static void DrawLatencyText(float latency)
{
    char text[32];
    snprintf(text, sizeof(text), "Latency: %.3f ms", latency * 1000.0f);

    int textWidth = MeasureText(text, 34);
    DrawText(text, s_WindowWidth - textWidth - 10, s_WindowHeight - 34 - 10, 34, GREEN);
}

static void DrawHeadSensorText(const ApplicationState *state)
{
    char text[32];
    const int positionX = s_WindowWidth * 0.6f;
    const int posisionY = s_WindowHeight * 0.06f;

    snprintf(text, sizeof(text), "dist: %.2f mm", state->headSensorDist);
    DrawText(text, positionX, posisionY, 18, (Color){0, 255, 0, 255});

    snprintf(text, sizeof(text), "heading: %d deg", (int)state->headSensorHeading);
    DrawText(text, positionX, posisionY + 18, 18, GREEN);
}

static void DrawArrow(Vector2 position, float rotation, float scale, bool isPressed)
{
    DrawTextureEx(
        s_ArrowTexture,
        position,
        rotation,
        scale,
        WHITE);

    if (isPressed)
        DrawTextureEx(
            s_ArrowOverlayTexture,
            position,
            rotation,
            scale,
            WHITE);
}

static void DrawArrows(const ApplicationState *state)
{
    const float scale = 0.2f;
    const int textureWidth = scale * s_ArrowTexture.width;
    const int textureHeight = scale * s_ArrowTexture.height;

    DrawArrow(
        (Vector2){10, s_WindowWidth - 10},
        270.0f,
        scale,
        state->movingLeft);
    DrawArrow(
        (Vector2){textureWidth * 2 + 10 * 2, s_WindowHeight - 10},
        180.0f,
        scale,
        state->movingBackward);
    DrawArrow(
        (Vector2){textureWidth * 3 + 10 * 3, s_WindowHeight - textureHeight - 10},
        90.0f,
        scale,
        state->movingRight);
    DrawArrow(
        (Vector2){textureWidth + 10 * 2, s_WindowHeight - textureHeight * 2 - 10 * 2},
        0.0f,
        scale,
        state->movingForward);
}

static void DrawTank(const ApplicationState *state)
{
    const float scale = 1.0f;
    const int hullWidth = s_HullTexture.width * scale;
    const int hullHeight = s_HullTexture.height * scale;
    const int trackWidth = s_TrackTexture.width * scale;
    const int trackHeight = s_TrackTexture.height * scale;
    const int indicatorWidth = s_SensorIndicatorTexture.width * scale;
    const int indicatorHeight = s_SensorIndicatorTexture.height * scale;
    const int indicatorOffsetX = indicatorWidth / 3;
    const int headWidth = s_HeadSensorTexture.width * scale;
    const int headHeight = s_HeadSensorTexture.height * scale;

    DrawTextureEx(
        s_TrackTexture,
        (Vector2){(s_WindowWidth - hullWidth) / 2 - trackWidth / 2, (s_WindowHeight - hullHeight) / 2},
        0.0f,
        scale,
        WHITE);
    DrawTextureEx(
        s_TrackTexture,
        (Vector2){(s_WindowWidth - hullWidth) / 2 - trackWidth / 2 + hullWidth, (s_WindowHeight - hullHeight) / 2},
        0.0f,
        scale,
        WHITE);

    DrawTextureEx(
        s_HullTexture,
        (Vector2){(s_WindowWidth - hullWidth) / 2, (s_WindowHeight - hullHeight) / 2},
        0.0f,
        scale,
        WHITE);

    // sensors
    if (state->sensorLeftOn)
        DrawTextureEx(
            s_SensorIndicatorTexture,
            (Vector2){
                (s_WindowWidth - hullWidth) / 2 + indicatorOffsetX + 1,
                (s_WindowHeight - hullHeight) / 2 - indicatorHeight / 2},
            0.0f,
            scale,
            WHITE);
    if (state->sensorRightOn)
        DrawTextureEx(
            s_SensorIndicatorTexture,
            (Vector2){
                (s_WindowWidth + hullWidth) / 2 - indicatorWidth - indicatorOffsetX,
                (s_WindowHeight - hullHeight) / 2 - indicatorHeight / 2},
            0.0f,
            scale,
            WHITE);
    if (state->sensorRearOn)
        DrawTextureEx(
            s_SensorIndicatorTexture,
            (Vector2){
                (s_WindowWidth + indicatorWidth) / 2,
                (s_WindowHeight - hullHeight) / 2 + hullHeight + indicatorHeight / 2},
            180.0f,
            scale,
            WHITE);

    // head sensor
    Vector2 headSensorPos = {
        (s_WindowWidth - headWidth) / 2,
        (s_WindowHeight - hullHeight + headHeight) / 2};
    const Vector2 headSensorPivot = {
        s_WindowWidth / 2,
        (s_WindowHeight - hullHeight) / 2 + headHeight};

    headSensorPos = Vector2Subtract(headSensorPos, headSensorPivot);
    headSensorPos = Vector2Rotate(headSensorPos, -state->headSensorHeading * DEG2RAD);
    headSensorPos = Vector2Add(headSensorPos, headSensorPivot);

    DrawTextureEx(
        s_HeadSensorTexture,
        headSensorPos,
        -state->headSensorHeading,
        scale,
        WHITE);
}

void GUI_Draw(const ApplicationState *state)
{
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTank(state);
    DrawHeadSensorText(state);
    DrawFrametimeText(state);
    DrawLatencyText(0.0f);
    DrawArrows(state);
    EndDrawing();
}

void GUI_Initialize(uint32_t width, uint32_t height)
{
    s_WindowWidth = width;
    s_WindowHeight = height;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(width, height, "Tank controller");
    LoadAssets();
}

void GUI_Shutdown(void)
{
    UnloadTexture(s_ArrowTexture);
    UnloadTexture(s_ArrowOverlayTexture);
    UnloadTexture(s_HeadSensorTexture);
    UnloadTexture(s_HullTexture);
    UnloadTexture(s_SensorIndicatorTexture);
    UnloadTexture(s_TrackTexture);

    CloseWindow();
}
