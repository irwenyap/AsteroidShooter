#include "GraphicsEngine.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ShaderUtils.hpp"
#include "../GameObject.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// DEFAULT SHADER
const char* vertexShaderSource = R"(
#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 460 core
out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D texture1;
uniform vec4 objectColor;
uniform bool useTexture;

void main() {
    if (useTexture)
        FragColor = texture(texture1, TexCoord);
    else
        FragColor = objectColor;
}
)";

// QUAD
std::vector<float> vertices = {
     0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
     0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
    -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
};

std::vector<unsigned int> indices = {
    0, 1, 3,
    1, 2, 3
};

GraphicsEngine& GraphicsEngine::GetInstance()
{
    static GraphicsEngine ge;
    return ge;
}

void GraphicsEngine::Init() {
    shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);

    view = glm::mat4(1.0f);
    float aspect = 1920.f / 1080.f;
    float zoom = 25.0f;
    projection = glm::ortho(-aspect * zoom, aspect * zoom, -zoom, zoom);

    meshes[Mesh::MESH_TYPE::QUAD] = Mesh(vertices, indices);

    textures[Texture::TEXTURE_TYPE::TEX_ASTEROID] = LoadTexture("../Assets/asteroid.png");
    textures[Texture::TEXTURE_TYPE::TEX_PLAYER] = LoadTexture("../Assets/spaceship.png");
}

void GraphicsEngine::Render(std::vector<std::unique_ptr<GameObject>>& gameObjects) {
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");

    GLint texUniformLoc = glGetUniformLocation(shaderProgram, "texture1");
    GLint useTexLoc = glGetUniformLocation(shaderProgram, "useTexture");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    glUniform1i(texUniformLoc, 0); // use texture unit 0
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    for (const auto& go : gameObjects) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, go->position);
        model = glm::rotate(model, go->rotation, glm::vec3(0, 0, 1));
        model = glm::scale(model, go->scale);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4fv(colorLoc, 1, glm::value_ptr(go->color));

        if (go->textured) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[go->textureType]);
            glUniform1i(useTexLoc, true);
        } else {
            glUniform1i(useTexLoc, false);
        }

        const Mesh& mesh = meshes[go->meshType];
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
    glUseProgram(0);
}

void GraphicsEngine::UpdateProjection(int width, int height) {
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float zoom = 25.0f;
    projection = glm::ortho(-aspect * zoom, aspect * zoom, -zoom, zoom);
}

GLuint GraphicsEngine::LoadTexture(const std::string& filePath)
{
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // flip image
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << filePath << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    if (nrChannels == 4) format = GL_RGBA;
    else if (nrChannels == 3) format = GL_RGB;
    else if (nrChannels == 1) format = GL_RED;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}
