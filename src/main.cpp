// Copyright 2017 <Biagio Festa>
#include <Box2D/Box2D.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <iostream>
#include <utility>
#include <vector>

namespace {

constexpr const int kWidthRenderer = 800;
constexpr const int kHeightRenderer = 600;
constexpr const float32 kTimeStep = 1.f / 60.f;
constexpr const int32 aVelocityIterations = 10;
constexpr const int32 aPositionIterations = 8;
constexpr const float kRatioPixelPerMetric = 40.f;
constexpr const float kRatioMetrixPerPixel = 1.f / kRatioPixelPerMetric;
constexpr const uint32 kFlagsDebugDraw = b2Draw::e_shapeBit;

b2World gB2World{b2Vec2{0.f, 0.f}};
std::vector<b2Body*> gBodies;
SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
SDL_Event gEvent;
b2Draw* gB2Draw = nullptr;

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
  SDL_CreateWindowAndRenderer(
      kWidthRenderer, kHeightRenderer,
      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC, &gWindow,
      &gRenderer);

  SDL_SetWindowTitle(gWindow, "RainDrop");
}

void stepB2World() {
  gB2World.Step(kTimeStep, aVelocityIterations, aPositionIterations);
}

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
                  const b2Color& color) override {}

  void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis,
                       const b2Color& color) override {}

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
  b2BodyDef aBodyDef;
  aBodyDef.type = b2_dynamicBody;

  b2Body* aBody = createBody(aBodyDef);
  addShapeToBody(SDL_Rect{0, 0, 50, 50}, aBody);

  b2Vec2 aBody1StartPosition = getMetricPositionFromPixel(100, 100);
  aBody->SetTransform(aBody1StartPosition, 0.f);

  setRestitutionToBody(0.3f, aBody);
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

  for (; itBody; itBody = itBody->GetNext()) {
  }
}

void mainLoop() {
  constexpr int kSecSimulation = 1;
  constexpr int kNumStep = kSecSimulation / kTimeStep;

  initGraphic();
  initBodies();

  bool aRunning = true;

  while (aRunning) {
    if (SDL_PollEvent(&gEvent)) {
      if (gEvent.type == SDL_QUIT) {
        aRunning = false;
      }
      if (gEvent.type == SDL_KEYUP) {
        if (gEvent.key.type == SDL_KEYUP &&
            gEvent.key.keysym.scancode == SDL_SCANCODE_RETURN) {
          createSquareDrop();
        }
      }
    }

    stepB2World();
    cleanUselessBodies();

    std::cout << "BodyInstances: " << gB2World.GetBodyCount() << "\r";

    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);

    gB2World.DrawDebugData();

    SDL_RenderPresent(gRenderer);

    SDL_Delay(1);
  }
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  ::mainLoop();
  return 0;
}
