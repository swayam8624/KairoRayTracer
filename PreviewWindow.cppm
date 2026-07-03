module;

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

export module Kairo.Foundation.RayTracer.PreviewWindow;

import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Film;
import Kairo.Foundation.RayTracer.ImageIO;

export namespace kairo::foundation::raytracer
{
    struct PreviewOptions final
    {
        std::string Title = "KairoRayTracer Preview";
        std::filesystem::path SavePath = "outputs/beauty.ppm";
    };

    [[nodiscard]]
    inline std::vector<std::uint8_t> ToSRGBBytes(
        const Film& film)
    {
        std::vector<std::uint8_t> bytes;
        bytes.resize(static_cast<std::size_t>(film.Width()) * film.Height() * 3u);

        for (std::uint32_t y = 0; y < film.Height(); ++y)
        {
            for (std::uint32_t x = 0; x < film.Width(); ++x)
            {
                const Color3f color =
                    Saturate(film.GetPixel(x, y));

                const std::size_t index =
                    (static_cast<std::size_t>(y) * film.Width() + x) * 3u;

                bytes[index + 0] = LinearChannelToByte(color.r);
                bytes[index + 1] = LinearChannelToByte(color.g);
                bytes[index + 2] = LinearChannelToByte(color.b);
            }
        }

        return bytes;
    }

    inline void RunPreviewWindow(
        Film film,
        PreviewOptions options,
        const std::function<Film()>& rerender)
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

        GLFWwindow* window =
            glfwCreateWindow(
                static_cast<int>(film.Width()),
                static_cast<int>(film.Height()),
                options.Title.c_str(),
                nullptr,
                nullptr);

        if (!window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW preview window.");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        std::vector<std::uint8_t> bytes =
            ToSRGBBytes(film);

        bool saveWasPressed = false;
        bool rerenderWasPressed = false;

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            const bool savePressed =
                glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;

            if (savePressed && !saveWasPressed)
            {
                SavePPM(film, options.SavePath);
                std::cout << "Saved " << options.SavePath << "\n";
            }

            saveWasPressed = savePressed;

            const bool rerenderPressed =
                glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

            if (rerenderPressed && !rerenderWasPressed)
            {
                film = rerender();
                bytes = ToSRGBBytes(film);
                SavePPM(film, options.SavePath);
                std::cout << "Re-rendered and saved " << options.SavePath << "\n";
            }

            rerenderWasPressed = rerenderPressed;

            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);

            glViewport(0, 0, width, height);
            glClearColor(0.02f, 0.025f, 0.035f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            const float scaleX =
                static_cast<float>(width) / static_cast<float>(film.Width());

            const float scaleY =
                static_cast<float>(height) / static_cast<float>(film.Height());

            glRasterPos2i(0, static_cast<int>(film.Height()));
            glPixelZoom(scaleX, -scaleY);
            glDrawPixels(
                static_cast<GLsizei>(film.Width()),
                static_cast<GLsizei>(film.Height()),
                GL_RGB,
                GL_UNSIGNED_BYTE,
                bytes.data());

            glfwSwapBuffers(window);
        }

        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
