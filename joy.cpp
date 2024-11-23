#include "graphics.hpp"

#include "GLFW/glfw3.h"

#include <cmath>
#include <ios>
#include <iostream>
#include <ostream>

static bool quit = false;
static bool attention = false;

static void error_callback(int code, const char* description)
{
  std::cerr << "error: " << code << ", " << description << '\n';
}

static void joystick_callback(int jid, int event)
{
  std::cout << "joystick event\n";
  if (event == GLFW_CONNECTED) {
    // The joystick was connected

    std::cout << "joystick connected\n";
  } else if (event == GLFW_DISCONNECTED) {
    // The joystick was disconnected

    std::cout << "joystick disconnected\n";
  }

  std::cout << std::endl;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_Q && action == GLFW_PRESS)
    quit = true;
  else if (key == GLFW_KEY_A && action == GLFW_PRESS)
    attention = true;
}

struct Pos
{
  float x;
  float y;
};

std::ostream& operator<< (std::ostream& stream, const Pos& pos)
{
  stream << "(" << pos.x << ", " << pos.y << ")";
  return stream;
}

int main()
{
  glfwSetErrorCallback(error_callback);
  GraphicsContext context(ClientAPI::OPENGL);

  GLFWwindow* window = glfwCreateWindow(640, 480, "My Title", nullptr, nullptr);
  if (!window) {
    // Window or context creation failed

    return 1;
  }

  glfwSetJoystickCallback(joystick_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);

  std::cout << std::boolalpha;
  for (int joystick_id = 0; joystick_id < GLFW_JOYSTICK_LAST; joystick_id++) {
    std::cout << "joy: " << (glfwJoystickPresent(joystick_id) == GLFW_TRUE) << '\n';
  }

  std::cout << "is gamepad: " << glfwJoystickIsGamepad(GLFW_JOYSTICK_1) << '\n' <<
    "gamepad name: " << glfwGetGamepadName(GLFW_JOYSTICK_1) << '\n';

  GLFWgamepadstate state;
  float axis_left_trigger_last_pos = -1.f;
  float axis_right_trigger_last_pos = -1.f;
  Pos axis_left_last_pos = { 0, 0 };
  Pos axis_right_last_pos = { 0, 0 };
  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    // Keep running
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
      if (state.buttons[GLFW_GAMEPAD_BUTTON_A]) {
        std::cout << "gamepad button A\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_B]) {
        std::cout << "gamepad button B\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_X]) {
        std::cout << "gamepad button X\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_Y]) {
        std::cout << "gamepad button Y\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]) {
        std::cout << "gamepad left bumper\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]) {
        std::cout << "gamepad right bumper\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_GUIDE]) {
        std::cout << "gamepad guide\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_START]) {
        std::cout << "gamepad start\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_BACK]) {
        std::cout << "gamepad back\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB]) {
        std::cout << "gamepad left thumb\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB]) {
        std::cout << "gamepad right thumb\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]) {
        std::cout << "gamepad dpad up\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]) {
        std::cout << "gamepad dpad right\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]) {
        std::cout << "gamepad dpad down\n";
      } else if (state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]) {
        std::cout << "gamepad dpad left\n";
      }

      constexpr float SENSITIVITY = .1f;

      const float axis_left_trigger_current_pos = state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
      const float axis_right_trigger_current_pos = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];

      if (std::fabs(axis_left_trigger_current_pos - axis_left_trigger_last_pos) >= SENSITIVITY ||
          std::fabs(axis_right_trigger_current_pos - axis_right_trigger_last_pos) >= SENSITIVITY) {
        std::cout << "left/right trigger: " << state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] << ", "
                  << state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] << '\n';
        axis_left_trigger_last_pos = axis_left_trigger_current_pos;
        axis_right_trigger_last_pos = axis_right_trigger_current_pos;
      }


      const Pos axis_left_current_pos = { state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] };
      const Pos axis_right_current_pos = { state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] };

      if (std::fabs(axis_left_current_pos.x - axis_left_last_pos.x) >= SENSITIVITY ||
          std::fabs(axis_left_current_pos.y - axis_left_last_pos.y) >= SENSITIVITY) {
        std::cout << "gamepad left: " << axis_left_current_pos << '\n';
      }
      axis_left_last_pos = axis_left_current_pos;

      if (std::fabs(axis_right_current_pos.x - axis_right_last_pos.x) >= SENSITIVITY ||
          std::fabs(axis_right_current_pos.y - axis_right_last_pos.y) >= SENSITIVITY) {
        std::cout << "gamepad right: " << axis_right_current_pos << '\n';
      }
      axis_right_last_pos = axis_right_current_pos;
    }
    glfwSwapBuffers(window);
    glfwPollEvents();

    if (quit)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (attention) {
      glfwRequestWindowAttention(window);
      attention = false;
    }
  }

  glfwDestroyWindow(window);

  return 0;
}
