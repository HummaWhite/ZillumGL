# ZillumGL

Zillum Renderer based on GLSL compute shader. [[The CPU version]](https://github.com/HummaWhite/Zillum) [[Relevant blog posts]](https://hummawhite.github.io/posts)

#### Features

- Multiple integrators
  - Single-kernel naive-schedule path tracer (with MIS)
  - Single-kernel naive-schedule adjoint particle tracer (light tracer)
  - MIS-weighted *(s = 0, t = any) + (s = 1, t = any) + (s = any, t = 1)* integrator (triple path tracer)

- Multiple BSDF models including Disney Principled BRDF
- Multiple-threaded BVH from [*Implementing a practical rendering system using GLSL*](https://cs.uwaterloo.ca/~thachisu/tdf2015.pdf#:~:text=Multiple-threaded%20BVH%20%28MTBVH%29%20%E2%80%A2Prepare%20threaded%20BVHs%20for%20six,%E2%80%A2Need%20to%20add%20only%20%E2%80%9Chit%E2%80%9D%20and%20%E2%80%9Cmiss%E2%80%9D%20links)
- Environment map importance sampling (path tracer only)
- Interactive
- A simple Sobol sampler
- XML scene file

#### Currently or potentially working on

- Ray regeneration / streaming
- GPU BDPT

- GPU BVH construction
- A scene editor

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

