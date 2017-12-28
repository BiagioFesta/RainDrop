// Copyright 2017 <Biagio Festa>
#include <Box2D/Box2D.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

/// Configuration ///
constexpr const int kWidthRenderer = 800;
constexpr const int kHeightRenderer = 600;
constexpr const float32 kTimeStep = 1.f / 40.f;
constexpr const int32 aVelocityIterations = 10;
constexpr const int32 aPositionIterations = 8;
constexpr const float kRatioPixelPerMetric = 40.f;
constexpr const float kRatioMetrixPerPixel = 1.f / kRatioPixelPerMetric;
constexpr const uint32 kFlagsDebugDraw = b2Draw::e_shapeBit;
constexpr const Uint32 kFlagsRenderer =
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
constexpr const int kFrameRateRender = 60;
constexpr const int kMsElapsedForUpdateRender = 1000.f / kFrameRateRender;
constexpr const int kFrequencyUpdatePhysic = 400;
constexpr const float kMsElapsedForUpdatePhysic =
    1000.f / kFrequencyUpdatePhysic;

/// Global Variables (yeah, Bad! But it's just a single CompUnit) ///
b2World gB2World{b2Vec2{0.f, 0.f}};
std::vector<b2Body*> gBodies;
SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
SDL_Event gEvent;
b2Draw* gB2Draw = nullptr;
TTF_Font* gFont = nullptr;
SDL_Texture* gTextureTextFrameRateRenderer = nullptr;
SDL_Texture* gTextureTextFrameRatePhysic = nullptr;
SDL_Texture* gTextureTextFrameRateMainLoop = nullptr;
SDL_Texture* gTextureTextObjectCounter = nullptr;
bool gThreadRunning = true;
std::size_t gFrameRatePhysic = 0;
std::size_t gFrameRateRender = 0;
std::size_t gFrameRateMainLoop = 0;
std::mutex gMutexUpdatePhysic;

int getPixelFromMetric(const float32 iMetric) noexcept {
  return static_cast<int>(iMetric * kRatioPixelPerMetric);
}

float getMetricFromPixel(const int iPixel) noexcept {
  return iPixel * kRatioMetrixPerPixel;
}

b2Vec2 getMetricPositionFromPixel(int iXPixel, int iYPixel) {
  return b2Vec2{getMetricFromPixel(iXPixel),
                getMetricFromPixel(kHeightRenderer - iYPixel)};
}

SDL_Rect getPixelPositionFromMetric(const b2Vec2& iMetrics) {
  return SDL_Rect{getPixelFromMetric(iMetrics.x),
                  kHeightRenderer - getPixelFromMetric(iMetrics.y), 0, 0};
}

void initSDL() {
  if (SDL_Init(SDL_INIT_EVERYTHING)) {
    std::cerr << "Error: " << SDL_GetError() << "\n";
    exit(-1);
  }

  if (TTF_Init()) {
    std::cerr << "Error: " << TTF_GetError() << "\n";
    exit(-1);
  }
}

void quitSDL() {
  TTF_Quit();
  SDL_Quit();
}

void loadFont() {
  gFont = TTF_OpenFont("NotoMono-Regular.ttf", 12);
  if (!gFont) {
    std::cerr << "Error: " << TTF_GetError() << "\n";
    exit(-1);
  }
}

void unloadFont() {
  if (gFont) {
    TTF_CloseFont(gFont);
    gFont = nullptr;
  }
}

void loadTextureAsText(const char* aText, SDL_Texture** oTexture) {
  if (oTexture) {
    SDL_DestroyTexture(*oTexture);
  }
  SDL_Surface* aSurface =
      TTF_RenderText_Blended(gFont, aText, SDL_Color{255, 255, 255, 255});
  if (!aSurface) {
    std::cerr << "Error: " << TTF_GetError() << "\n";
    exit(-1);
  }

  *oTexture = SDL_CreateTextureFromSurface(gRenderer, aSurface);
  if (!oTexture) {
    std::cerr << "Error: " << SDL_GetError() << "\n";
    exit(-1);
  }
}

void drawPolygon(const std::vector<Sint16>& iXVertices,
                 const std::vector<Sint16>& iYVertices,
                 const int iNumberVertices, const SDL_Color& iColor) {
  polygonRGBA(gRenderer, iXVertices.data(), iYVertices.data(), iNumberVertices,
              iColor.r, iColor.g, iColor.b, iColor.a);
}

void drawSolidPolygon(const std::vector<Sint16>& iXVertices,
                      const std::vector<Sint16>& iYVertices,
                      const int iNumberVertices, const SDL_Color& iColor) {
  filledPolygonRGBA(gRenderer, iXVertices.data(), iYVertices.data(),
                    iNumberVertices, iColor.r, iColor.g, iColor.b, iColor.a);
}

void initGraphic() {
  SDL_CreateWindowAndRenderer(kWidthRenderer, kHeightRenderer, kFlagsRenderer,
                              &gWindow, &gRenderer);

  SDL_SetWindowTitle(gWindow, "RainDrop");
  SDL_SetHint("SDL_HINT_RENDER_VSYNC", "1");
  if (SDL_SetHint("SDL_HINT_RENDER_SCALE_QUALITY", "2") == SDL_FALSE) {
    std::cerr << "Scale quality not supported\n";
  }

  loadFont();
}

void closeGraphic() {
  unloadFont();
  if (gRenderer) {
    SDL_DestroyRenderer(gRenderer);
    gRenderer = nullptr;
  }

  if (gWindow) {
    SDL_DestroyWindow(gWindow);
    gWindow = nullptr;
  }
}

void stepB2World() {
  using Clock_t = std::chrono::high_resolution_clock;
  using TimePoint_t = typename Clock_t::time_point;

  static std::size_t sCounterFrame = 0;
  static TimePoint_t sLastCallTime;
  static TimePoint_t sTimeResetCounter = Clock_t::now();

  const auto aTimeNow = Clock_t::now();
  const auto aTimeElapsedMs =
      std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(
          aTimeNow - sLastCallTime)
          .count();
  static_assert(std::is_same<decltype(aTimeElapsedMs), const float>::value,
                "Not float");

  if (std::chrono::duration_cast<std::chrono::milliseconds>(aTimeNow -
                                                            sTimeResetCounter)
          .count() >= 1000) {
    gFrameRatePhysic = sCounterFrame;
    sCounterFrame = 0;
    sTimeResetCounter = aTimeNow;
  }

  if (aTimeElapsedMs >= kMsElapsedForUpdatePhysic) {
    gB2World.Step(kTimeStep, aVelocityIterations, aPositionIterations);

    sLastCallTime = aTimeNow;
    ++sCounterFrame;
  }
}

void cleanUselessBodies() {
  constexpr int kMaxOutsidePixel = 1000;

  auto itBody = gB2World.GetBodyList();
  while (itBody != nullptr) {
    SDL_Rect aPixelCoordinate =
        getPixelPositionFromMetric(itBody->GetPosition());
    if (aPixelCoordinate.x < -kMaxOutsidePixel ||
        aPixelCoordinate.x > kWidthRenderer + kMaxOutsidePixel ||
        aPixelCoordinate.y < -kMaxOutsidePixel ||
        aPixelCoordinate.y > kHeightRenderer + kMaxOutsidePixel) {
      b2Body* aPtrBodyToErase = itBody;
      itBody = itBody->GetNext();
      aPtrBodyToErase->GetWorld()->DestroyBody(aPtrBodyToErase);
    } else {
      itBody = itBody->GetNext();
    }
  }
}

void threadPhysicEngine() {
  while (gThreadRunning) {
    std::unique_lock<std::mutex> aLocker(gMutexUpdatePhysic);
    cleanUselessBodies();
    stepB2World();
    aLocker.unlock();

    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
  }
}

b2Body* createBody(const b2BodyDef& iBodyDef) {
  b2Body* aNewBody = gB2World.CreateBody(&iBodyDef);
  gBodies.push_back(aNewBody);
  return aNewBody;
}

void addShapeToBody(const SDL_Rect& aPixelSize, b2Body* ioBody) {
  b2PolygonShape aRectShape;
  aRectShape.SetAsBox(getMetricFromPixel(aPixelSize.w) / 2.f,
                      getMetricFromPixel(aPixelSize.h) / 2.f,
                      b2Vec2{getMetricFromPixel(aPixelSize.x),
                             getMetricFromPixel(aPixelSize.y)},
                      0.f);

  ioBody->CreateFixture(&aRectShape, 1.f);
}

void addShapeCircleToBody(int iRadiusPixel, b2Body* ioBody) {
  b2CircleShape aCircleShape;
  aCircleShape.m_radius = getMetricFromPixel(iRadiusPixel);

  ioBody->CreateFixture(&aCircleShape, 1.f);
}

class PhysicDrawner : public b2Draw {
 public:
  void DrawPolygon(const b2Vec2* iVertices, int32 iVertexCount,
                   const b2Color& iColor) override {
    const auto aVerticesPixel = buildGraphicVertices(iVertices, iVertexCount);
    const auto aColor = convertColor(iColor);

    drawPolygon(aVerticesPixel.first, aVerticesPixel.second, iVertexCount,
                aColor);
  }

  void DrawSolidPolygon(const b2Vec2* iVertices, int32 iVertexCount,
                        const b2Color& iColor) override {
    const auto aVerticesPixel = buildGraphicVertices(iVertices, iVertexCount);
    const auto aColor = convertColor(iColor);

    drawSolidPolygon(aVerticesPixel.first, aVerticesPixel.second, iVertexCount,
                     aColor);
  }

  void DrawCircle(const b2Vec2& center, float32 radius,
                  const b2Color& color) override {
    const SDL_Rect aPosition = getPixelPositionFromMetric(center);
    const auto aColor = convertColor(color);
    const Sint16 aRadiusPixel = getPixelFromMetric(radius);

    circleRGBA(gRenderer, aPosition.x, aPosition.y, aRadiusPixel, aColor.r,
               aColor.g, aColor.b, aColor.a);
  }

  void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis,
                       const b2Color& color) override {
    const SDL_Rect aPosition = getPixelPositionFromMetric(center);
    const auto aColor = convertColor(color);
    const Sint16 aRadiusPixel = getPixelFromMetric(radius);

    filledCircleRGBA(gRenderer, aPosition.x, aPosition.y, aRadiusPixel,
                     aColor.r, aColor.g, aColor.b, aColor.a);
  }

  void DrawSegment(const b2Vec2& p1, const b2Vec2& p2,
                   const b2Color& color) override {}

  void DrawTransform(const b2Transform& xf) override {}

 private:
  std::pair<std::vector<Sint16>, std::vector<Sint16>> buildGraphicVertices(
      const b2Vec2* iMetricVertices, const int32 iVertexCount) {
    SDL_Rect aRect;
    std::vector<Sint16> aVerticesPixelX;
    std::vector<Sint16> aVerticesPixelY;
    aVerticesPixelX.reserve(iVertexCount);
    aVerticesPixelY.reserve(iVertexCount);

    for (int32 i = 0; i < iVertexCount; ++i) {
      const b2Vec2& aVertex = iMetricVertices[i];
      aRect = getPixelPositionFromMetric(aVertex);

      aVerticesPixelX.emplace_back(aRect.x);
      aVerticesPixelY.emplace_back(aRect.y);
    }

    return make_pair(std::move(aVerticesPixelX), std::move(aVerticesPixelY));
  }

  SDL_Color convertColor(const b2Color& iColor) {
    return SDL_Color{
        static_cast<Uint8>(iColor.r * 255), static_cast<Uint8>(iColor.g * 255),
        static_cast<Uint8>(iColor.g * 255), static_cast<Uint8>(iColor.a * 255)};
  }
};

void setRestitutionToBody(const float iValue, b2Body* ioBody) {
  for (auto itFix = ioBody->GetFixtureList(); itFix; itFix = itFix->GetNext()) {
    itFix->SetRestitution(iValue);
  }
}

void createSquareDrop() {
#ifdef ENABLE_MULTITHREAD
  std::unique_lock<std::mutex> aLocker(gMutexUpdatePhysic);
#endif

  b2BodyDef aBodyDef;
  aBodyDef.type = b2_dynamicBody;

  b2Body* aBody = createBody(aBodyDef);
  addShapeToBody(SDL_Rect{0, 0, 50, 50}, aBody);

  b2Vec2 aBody1StartPosition = getMetricPositionFromPixel(100, 100);
  aBody->SetTransform(aBody1StartPosition, 0.f);

  setRestitutionToBody(0.3f, aBody);
}

void createCircleDrop() {
#ifdef ENABLE_MULTITHREAD
  std::unique_lock<std::mutex> aLocker(gMutexUpdatePhysic);
#endif

  b2BodyDef aBodyDef;
  aBodyDef.type = b2_dynamicBody;

  b2Body* aBody = createBody(aBodyDef);
  addShapeCircleToBody(25, aBody);

  b2Vec2 aStartPosition = getMetricPositionFromPixel(100, 100);
  aBody->SetTransform(aStartPosition, 0.f);

  setRestitutionToBody(0.7f, aBody);
}

void initBodies() {
  gB2World.SetGravity(b2Vec2{0.f, -0.8f});
  gB2Draw = new PhysicDrawner();
  gB2Draw->SetFlags(kFlagsDebugDraw);
  gB2World.SetDebugDraw(gB2Draw);

  createSquareDrop();

  b2BodyDef aBodyDef;
  aBodyDef.type = b2_staticBody;

  b2Body* aBody2 = createBody(aBodyDef);
  addShapeToBody(SDL_Rect{0, 0, 800, 100}, aBody2);
  addShapeToBody(SDL_Rect{200, 100, 100, 100}, aBody2);
  b2Vec2 aBody2StartPosition = getMetricPositionFromPixel(400, 600);
  aBody2->SetTransform(aBody2StartPosition, -0.25f);
  setRestitutionToBody(0.3f, aBody2);
}

void drawTexture(SDL_Texture* iTexture, int x, int y) {
  if (iTexture) {
    int aWidthTexture, aHeightTexture;
    SDL_QueryTexture(iTexture, nullptr, nullptr, &aWidthTexture,
                     &aHeightTexture);

    SDL_Rect aDestRect{x, y, aWidthTexture, aHeightTexture};
    SDL_RenderCopy(gRenderer, iTexture, nullptr, &aDestRect);
  }
}

void drawAllGlobalTexture() {
  drawTexture(gTextureTextFrameRateRenderer, 500, 10);
  drawTexture(gTextureTextFrameRatePhysic, 500, 30);
  drawTexture(gTextureTextFrameRateMainLoop, 500, 50);
  drawTexture(gTextureTextObjectCounter, 500, 70);
}

void loadAllGlobalTeture() {
  loadTextureAsText((std::string("FrameRate Renderer [Hz]: ") +
                     (std::to_string(gFrameRateRender)))
                        .c_str(),
                    &gTextureTextFrameRateRenderer);
  loadTextureAsText((std::string("FrameRate Physic [Hz]: ") +
                     (std::to_string(gFrameRatePhysic)))
                        .c_str(),
                    &gTextureTextFrameRatePhysic);
  loadTextureAsText((std::string("FrameRate MainLoop [Hz]: ") +
                     (std::to_string(gFrameRateMainLoop)))
                        .c_str(),
                    &gTextureTextFrameRateMainLoop);
  loadTextureAsText((std::string("No. Physic Objects: ") +
                     std::to_string(gB2World.GetBodyCount()))
                        .c_str(),
                    &gTextureTextObjectCounter);
}

void renderStep() {
  using Clock_t = std::chrono::high_resolution_clock;
  using TimePoint_t = typename Clock_t::time_point;

  static std::size_t sCounterFrame = 0;
  static TimePoint_t sLastCallTime;
  static TimePoint_t sTimeResetCounter = Clock_t::now();

  const auto aTimeNow = Clock_t::now();
  const auto aTimeElapsedMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(aTimeNow -
                                                            sLastCallTime)
          .count();

  if (std::chrono::duration_cast<std::chrono::milliseconds>(aTimeNow -
                                                            sTimeResetCounter)
          .count() >= 1000) {
    gFrameRateRender = sCounterFrame;
    sCounterFrame = 0;
    sTimeResetCounter = aTimeNow;
  }

  if (aTimeElapsedMs >= kMsElapsedForUpdateRender) {
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);

    loadAllGlobalTeture();
    gB2World.DrawDebugData();
    drawAllGlobalTexture();

    SDL_RenderPresent(gRenderer);

    sLastCallTime = aTimeNow;
    ++sCounterFrame;
  }
}

void printHeaderInfo(std::ostream* oStream) {
  *oStream << "Renderer resolution [Pixel]: " << kWidthRenderer << "x"
           << kHeightRenderer << "\n"
           << "TimeStep Physic Engine [s]: " << kTimeStep << "\n"
           << "FrameRate Renderer [Hz]: " << kFrameRateRender << "\n"
           << "FrameRate Physic Engine [Hz]: " << kFrequencyUpdatePhysic << "\n"
           << "Time Delay Renderer [ms]: " << kMsElapsedForUpdateRender << "\n"
           << "Time Delay Phisic [ms]: " << kMsElapsedForUpdatePhysic << "\n"
           << "Multithread: "
           <<
#ifdef ENABLE_MULTITHREAD
      "Enabled"
#else
      "Disabled"
#endif
           << "\n";

  const int nDrivers = SDL_GetNumVideoDrivers();
  *oStream << "Graphical Video Drivers:\n";
  for (int i = 0; i < nDrivers; ++i) {
    *oStream << "  - " << SDL_GetVideoDriver(i) << "\n";
  }

  SDL_RendererInfo aRendererInfo;
  SDL_GetRendererInfo(gRenderer, &aRendererInfo);
  *oStream << "Renderer Name: " << aRendererInfo.name << "\n";
}

void mainLoop() {
  using Clock_t = std::chrono::high_resolution_clock;
  using TimePoint_t = typename Clock_t::time_point;

  constexpr int kSecSimulation = 1;
  constexpr int kNumStep = kSecSimulation / kTimeStep;

  initGraphic();

  printHeaderInfo(&std::cout);

  initBodies();

  bool aRunning = true;

  std::size_t aFrameCounter = 0;
  TimePoint_t aLastFrameCounterReset = Clock_t::now();

#ifdef ENABLE_MULTITHREAD
  std::thread aThreadPhysic{threadPhysicEngine};
#endif

  while (aRunning) {
    const auto aTimeNow = Clock_t::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            aTimeNow - aLastFrameCounterReset)
            .count() >= 1000) {
      gFrameRateMainLoop = aFrameCounter;
      aFrameCounter = 0;
      aLastFrameCounterReset = aTimeNow;
    }

    if (SDL_PollEvent(&gEvent)) {
      if (gEvent.type == SDL_QUIT) {
        aRunning = false;
      }
      if (gEvent.type == SDL_KEYUP) {
        if (gEvent.key.type == SDL_KEYUP) {
          if (gEvent.key.keysym.scancode == SDL_SCANCODE_S) {
            createSquareDrop();
          } else if (gEvent.key.keysym.scancode == SDL_SCANCODE_C) {
            createCircleDrop();
          }
        }
      }
    }

#ifndef ENABLE_MULTITHREAD
    cleanUselessBodies();
    stepB2World();
#endif
    renderStep();

    std::this_thread::sleep_for(std::chrono::nanoseconds(100));

    ++aFrameCounter;
  }

  gThreadRunning = false;

#ifdef ENABLE_MULTITHREAD
  aThreadPhysic.join();
  // aThreadRender.join();
#endif

  closeGraphic();
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  initSDL();
  ::mainLoop();
  quitSDL();
  return 0;
}
