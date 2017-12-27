# RainDrop
Simple Test Project usage Box2D and SDL2

<img src="http://www.biagiofesta.it/images/RainDropExample.png" height="420">


## Installation

## Prerequisites
* C++14 compiler support
* [Box2D](http://box2d.org/)
* [SDL2](https://www.libsdl.org/download-2.0.php)
* [SDL2_gfx](http://www.ferzkopp.net/wordpress/2016/01/02/sdl_gfx-sdl2_gfx/)
* [SDL2_ttf](https://www.libsdl.org/projects/SDL_ttf/)

### GitHub Commands Line
~~~
git clone https://github.com/BiagioFesta/RainDrop.git && \
cd RainDrop && \
mkdir build && \
cd build && \
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_MULTITHREAD=ON && \
make && \
cp ../NotoMono-Regular.ttf . && \
./RainDrop
~~~
