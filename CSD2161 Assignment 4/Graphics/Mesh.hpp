#pragma once

#include <vector>

typedef unsigned int GLuint;
typedef int GLsizei;

struct Mesh {
    enum MESH_TYPE {
        QUAD,
        NONE
    };

    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    GLsizei indexCount;

    Mesh();
    Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);
};
