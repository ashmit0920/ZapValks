#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include "stb_easy_font.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>

// screen
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1200;

// game states
enum GameState { WELCOME, INSTRUCTIONS, PLAYING, GAME_OVER };
GameState state = WELCOME;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// player
struct Entity {
    glm::vec2 Position;
    glm::vec2 Size;
    glm::vec3 Color;
    float     Health;
};
Entity Player;

// bullets
struct Bullet {
    glm::vec2 Position;
    glm::vec2 Velocity;
    glm::vec3 Color;
};
std::vector<Bullet> Bullets;

// enemies
struct Enemy {
    glm::vec2 Position;
    glm::vec2 Size;
    glm::vec3 Color;
    float     Speed;
};
std::vector<Enemy> Enemies;

// starfield for background
std::vector<glm::vec2> Stars;

// scores
unsigned int score = 0, highScore = 0;

// shader program & quad
unsigned int shaderProgram, VAO, VBO;

// simple vertex+fragment
const char* vertexSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 projection;
uniform mat4 model;

void main(){
    gl_Position = projection * model * vec4(aPos,0.0,1.0);
}
)";
const char* fragmentSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main(){
    FragColor = vec4(color,1.0);
}
)";

// utility: compile/link
unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    int ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(id, 512, nullptr, buf);
        std::cerr << "ShaderCompileError:\n" << buf << "\n";
    }
    return id;
}
unsigned int createProgram(const char* vs, const char* fs) {
    unsigned int p = glCreateProgram(),
        v = compileShader(GL_VERTEX_SHADER, vs),
        f = compileShader(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
        std::cerr << "LinkError:\n" << buf << "\n";
    }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// set up a unit quad (0,0)-(1,1)
void initRenderer() {
    shaderProgram = createProgram(vertexSrc, fragmentSrc);
    float quadVerts[] = {
        0,1,   1,0,   0,0,
        0,1,   1,1,   1,0
    };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // enable alpha blending for smooth visuals
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// draw any colored rectangle
void drawEntity(const Entity& e) {
    glUseProgram(shaderProgram);

    // projection orthographic, Y down-to-up
    glm::mat4 proj = glm::ortho(0.f, (float)SCR_WIDTH, 0.f, (float)SCR_HEIGHT, -1.f, 1.f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

    // model: translate & scale
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(e.Position, 0.f));
    model = glm::scale(model, glm::vec3(e.Size, 1.f));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

    // color
    glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(e.Color));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// input
bool keys[1024];
void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS)   keys[key] = true;
    if (action == GLFW_RELEASE) keys[key] = false;

    // state transitions
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
        if (state == WELCOME || state == INSTRUCTIONS) {
            state = PLAYING;
        }
        else if (state == GAME_OVER) {
            // reset
            score = 0;
            Player.Health = 100;
            Enemies.clear();
            Bullets.clear();
            state = WELCOME;
        }
    }
    if (key == GLFW_KEY_I && action == GLFW_PRESS && state == WELCOME) {
        state = INSTRUCTIONS;
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(w, true);
    }
}

void processInput() {
    float v = 600.f * deltaTime;
    // only vertical movement
    if (keys[GLFW_KEY_W] && Player.Position.y + Player.Size.y < SCR_HEIGHT)
        Player.Position.y += v;
    if (keys[GLFW_KEY_S] && Player.Position.y > 0)
        Player.Position.y -= v;
    // shoot
    if (keys[GLFW_KEY_SPACE]) {
        static float lastShot = 0;
        if (glfwGetTime() - lastShot >= 0.2f) {
            Bullet b;
            b.Position = Player.Position + glm::vec2(Player.Size.x, Player.Size.y / 2 - 5);
            b.Velocity = glm::vec2(600.f, 0.f);
            b.Color = glm::vec3(1.f, 0.8f, 0.2f);
            Bullets.push_back(b);
            lastShot = glfwGetTime();
        }
    }
}

// high score I/O
void loadHighScore() {
    std::ifstream fin("highscore.txt");
    if (fin) fin >> highScore;
}
void saveHighScore() {
    std::ofstream fout("highscore.txt");
    fout << highScore;
}

// spawn an enemy on the right, random Y
void spawnEnemy() {
    Enemy e;
    e.Size = glm::vec2(40, 40);
    e.Position = glm::vec2(SCR_WIDTH, rand() % (SCR_HEIGHT - (int)e.Size.y));
    e.Color = glm::vec3(1.f, 0.2f, 0.2f);
    // negative ? moves left
    e.Speed = -(150.f + rand() % 100);
    Enemies.push_back(e);
}

// text
void renderText(const char* text, float x, float y, float scale, glm::vec3 color) {
    // 1) ask stb_easy_font to fill a CPU buffer
    static char buffer[99999];
    int quads = stb_easy_font_print(0, 0, (char*)text, nullptr, buffer, sizeof(buffer));

    // 2) build an orthographic projection
    glm::mat4 proj = glm::ortho(0.f, (float)SCR_WIDTH, 0.f, (float)SCR_HEIGHT);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(color));

    // 3) upload the vertex data into a dynamic VBO
    GLuint textVBO, textVAO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, quads * 4 * 2 * sizeof(float), buffer, GL_DYNAMIC_DRAW);

    // 4) set up attribute (location = 0: vec2 aPos)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);

    // 5) draw the quads
    // translate & scale via a model matrix
    glm::mat4 model = glm::translate(glm::mat4(1.0f), { x, y, 0.0f })
        * glm::scale(glm::mat4(1.0f), { scale, scale, 1.0f });
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    
    for (int i = 0; i < quads; ++i) {
        glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
    }

    // 6) cleanup
    glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &textVAO);
}


int main() {
    srand((unsigned)time(NULL));
    // init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Horizontal Shooter", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    initRenderer();

    // init player on left
    Player.Position = glm::vec2(20, SCR_HEIGHT / 2 - 25);
    Player.Size = glm::vec2(50, 50);
    Player.Color = glm::vec3(0.2f, 0.6f, 1.f);
    Player.Health = 100.f;

    // build starfield
    for (int i = 0; i < 100; i++) {
        Stars.emplace_back(
            (float)(rand() % SCR_WIDTH),
            (float)(rand() % SCR_HEIGHT)
        );
    }

    loadHighScore();
    float spawnTimer = 0.f;

    // game loop
    while (!glfwWindowShouldClose(window)) {
        // time
        float now = glfwGetTime();
        deltaTime = now - lastFrame;
        lastFrame = now;

        glfwPollEvents();
        processInput();

        // spawn
        spawnTimer += deltaTime;
        if (state == PLAYING && spawnTimer >= 1.0f) {
            spawnEnemy();
            spawnTimer = 0;
        }

        // update bullets
        for (auto& b : Bullets)
            b.Position += b.Velocity * deltaTime;
        Bullets.erase(
            std::remove_if(Bullets.begin(), Bullets.end(),
                [](auto& b) { return b.Position.x > SCR_WIDTH + 10; }),
            Bullets.end()
        );

        // update enemies & collisions
        for (auto eIt = Enemies.begin(); eIt != Enemies.end(); ) {
            eIt->Position.x += eIt->Speed * deltaTime;
            bool removed = false;
            // off-screen left
            if (eIt->Position.x + eIt->Size.x < 0) {
                Player.Health -= 20.f;
                eIt = Enemies.erase(eIt);
                removed = true;
            }
            else {
                // bullet collisions
                for (auto bIt = Bullets.begin(); bIt != Bullets.end(); ) {
                    if (bIt->Position.x > eIt->Position.x &&
                        bIt->Position.x < eIt->Position.x + eIt->Size.x &&
                        bIt->Position.y > eIt->Position.y &&
                        bIt->Position.y < eIt->Position.y + eIt->Size.y)
                    {
                        score += 10;
                        bIt = Bullets.erase(bIt);
                        eIt = Enemies.erase(eIt);
                        removed = true;
                        break;
                    }
                    else ++bIt;
                }
            }
            if (!removed) ++eIt;
        }


        if (Player.Health <= 0 && state == PLAYING) {
            state = GAME_OVER;
            if (score > highScore) {
                highScore = score;
                saveHighScore();
            }
        }

        // ** RENDER **
        glClear(GL_COLOR_BUFFER_BIT);

        if (state == WELCOME) {
            glClearColor(0.05f, 0.05f, 0.2f, 1.f);

            renderText("Welcome to the Game!", 200.0f, 200.0f, 2.0f, glm::vec3(1.0f, 1.0f, 1.0f)); //0.2f, 0.8f, 0.2f
            renderText("Use W A S D to move", 240.0f, 300.0f, 1.2f, glm::vec3(0.7f, 0.7f, 0.7f));
            renderText("Press SPACE to shoot", 240.0f, 340.0f, 1.2f, glm::vec3(0.7f, 0.7f, 0.7f));
            renderText("Press ENTER to begin", 240.0f, 400.0f, 1.5f, glm::vec3(1.0f, 1.0f, 0.0f));
        }
        else if (state == INSTRUCTIONS) {
            glClearColor(0.1f, 0.1f, 0.2f, 1.f);
        }
        else if (state == PLAYING) {
            // starfield
            for (auto& s : Stars) {
                s.x -= 50.f * deltaTime;
                if (s.x < 0) s.x = SCR_WIDTH;
                // draw tiny white star
                drawEntity(Entity{ s, glm::vec2(2,2), glm::vec3(1.f),0 });
            }

            // player
            drawEntity(Player);
            // bullets
            for (auto& b : Bullets)
                drawEntity(Entity{ b.Position, glm::vec2(10,4), b.Color,0 });
            // enemies
            for (auto& e : Enemies)
                drawEntity(Entity{ e.Position, e.Size, e.Color,0 });

            // health bar
            float w = 200.f * glm::max(Player.Health, 0.f) / 100.f;
            drawEntity(Entity{ glm::vec2(10,10), glm::vec2(w,20), glm::vec3(0.1f,0.8f,0.1f),0 });

            // text to show score
            char scoreStr[32], healthStr[32];
            sprintf_s(scoreStr, "Score: %d", score);
            sprintf_s(healthStr, "Health: %.0f", Player.Health);
            renderText(scoreStr, 20.0f, 560.0f, 1.2f, glm::vec3(1.0f, 1.0f, 1.0f));
            renderText(healthStr, 20.0f, 530.0f, 1.0f, glm::vec3(0.6f, 1.0f, 0.6f));
        }
        else if (state == GAME_OVER) {
            glClearColor(0.2f, 0.05f, 0.05f, 1.f);
            renderText("Game Over", 280.0f, 200.0f, 2.5f, glm::vec3(1.0f, 0.2f, 0.2f));

            char finalScoreStr[64];
            sprintf_s(finalScoreStr, "Your Score: %d", score);
            renderText(finalScoreStr, 300.0f, 280.0f, 1.5f, glm::vec3(1.0f, 1.0f, 1.0f));

            char highScoreStr[64];
            sprintf_s(highScoreStr, "High Score: %d", highScore);
            renderText(highScoreStr, 300.0f, 320.0f, 1.5f, glm::vec3(1.0f, 1.0f, 0.6f));

            renderText("Press Enter to play again", 240.0f, 400.0f, 1.2f, glm::vec3(0.8f, 0.8f, 0.2f));
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}