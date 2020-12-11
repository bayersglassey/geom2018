# SPIDER GAME


## What is "Spider Game"?

A game in development, a kind of puzzly arcadey look at the triangular grid.
You play as a spider... robot?..
There are screenshots below.

![A spider... robot](/www/img/player.png)

It [has a website](http://depths.bayersglassey.com) which may or may not be up at the moment.
If it's up, you should be able to play an old version of it online, thanks to the wonders of
[Emscripten](https://emscripten.org/).
The Emscripten version is from an old build, and seems to run into some nasty lag, so if you
like it whatsoever, consider cloning this repo and attempting to build it locally!


## How to compile & run the game

``make && ./bin/demo`` should do the trick (if you have gcc).
You'll need the [SDL2](https://www.libsdl.org/) development/runtime libraries for your OS.

Once the game is running, use the arrow keys to move, Escape to quit.

If you die or get stuck, press '1' to respawn at the last save point you touched.
(Save points are the glowing rotating green things.)

Press Spacebar to spit little balls, like a real spider.
They mostly just help you judge your jumps, but you may find... other uses for them also.


## What is "geom2018"?

A graphics library for tesselating the plane with squares & triangles.
The game demo has sprouted from it, and come to overshadow the library...


## Spider Game screenshots

### I know you want to play this
![](/img/title.png)

### Why hello.
![](/img/start_0.gif)

### Don't get crushed
![](/img/big_0.gif)

### Strange creatures in the depths
![](/img/jungle_0.gif)

### Run run run
![](/img/jungle_1.gif)

### RUUUNNNN
![](/img/jungle_2.gif)

### Your useless bouncing spit
![](/img/map1_0.gif)

### A mysterious place
![](/img/screen7.png)

### An untimely end
![](/img/screen8.png)


## "geom2018" screenshots

### A little pattern:
![](/img/screen4.png)

### A big animated pattern:
![](/img/screen3.png)

### A bug results in a neat pattern:
![](/img/screen1.png)

### Game demo:
![](/img/screen2.png)

### A puzzle:
![](/img/demo1.png)

### Game demo passed through some kind of geometry-expanding transformation:
![](/img/screen5.png)

### A demo map, zoomed out:
![](/img/demo2.png)


