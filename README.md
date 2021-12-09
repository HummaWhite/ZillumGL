# ZillumGL

Renderer (path tracer) based on GLSL compute shader. [CPU version here](https://github.com/HummaWhite/Zillum)

#### Features

- Path tracer with Next Event Estimation (MIS weighted)
- GPU accelerated
- Multiple-threaded BVH from [*Implementing a practical rendering system using GLSL*](https://cs.uwaterloo.ca/~thachisu/tdf2015.pdf#:~:text=Multiple-threaded%20BVH%20%28MTBVH%29%20%E2%80%A2Prepare%20threaded%20BVHs%20for%20six,%E2%80%A2Need%20to%20add%20only%20%E2%80%9Chit%E2%80%9D%20and%20%E2%80%9Cmiss%E2%80%9D%20links)
- Environment map importance sampling
- Interactive, with adjustable material and camera
- A simple Sobol sampler
- XML scene loading

#### Working on

- A scene editor
- Real-time preview

#### Demo videos

- [Low discrepancy sampler test](https://youtu.be/pjfcD8fYfQg)
- [Rendering the Gallery with textures](https://youtu.be/TGbwSyqxKvY)
- [Rendering a Utah Teapot](https://youtu.be/HNXanaqzhgQ)

#### Demo pictures

- [Link to all demo pictures](https://hummawhite.github.io/gallery)

![](https://hummawhite.github.io/img/sponza.png)

![](https://hummawhite.github.io/img/car1.png)

![](https://hummawhite.github.io/img/rungholt.png)

![](https://hummawhite.github.io/img/mitsuba2.png)

