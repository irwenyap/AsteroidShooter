#include "Mesh.hpp"
#include <glad/glad.h>

Mesh::Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0)
{
}

Mesh::Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {

    // vao
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // vbo
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // ebo
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);       // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // texcoord
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    indexCount = static_cast<GLsizei>(indices.size());
}
