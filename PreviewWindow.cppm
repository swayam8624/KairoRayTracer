module;

#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#if defined(_WIN32)
// The Windows OpenGL header uses WINGDIAPI and other declarations supplied by
// the Win32 SDK. GLFW_INCLUDE_NONE deliberately prevents GLFW from including
// windows.h, so the preview owns that platform-header dependency explicitly.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

// Windows' system OpenGL 1.1 header omits this core sampler token even though
// the value is stable and GLFW creates a context where the operation is valid.
// Newer platform headers may define it already, so keep this compatibility
// definition guarded.
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

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
    //=========================================================
    // Preview Window
    //
    // This is not an editor. It is a thin image viewer for a completed CPU film.
    // The important design boundary is that the ray tracer still produces an
    // image file; GLFW only helps inspect the result immediately.
    //=========================================================

    struct PreviewOptions final
    {
        std::string Title = "KairoRayTracer Preview";
        std::filesystem::path SavePath = "outputs/beauty.ppm";
    };

    enum class PreviewRenderRequest
    {
        Reload,
        OrbitLeft,
        OrbitRight,
        OrbitUp,
        OrbitDown,
        ZoomIn,
        ZoomOut,
        ResetOrbit
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
                    Saturate(ToneMapReinhard(film.GetPixel(x, y)));

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
        const std::function<Film(PreviewRenderRequest)>& rerender)
    {
        // GLFW owns the OS window and OpenGL context. The CPU renderer has
        // already finished before this function starts drawing.
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

        GLuint texture = 0;
        glGenTextures(1, &texture);

        auto uploadTexture =
            [&]()
            {
                // Uploading a texture is more reliable on macOS than old
                // glDrawPixels paths. The film stays CPU-owned; this texture is
                // only a display copy.
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGB,
                    static_cast<GLsizei>(film.Width()),
                    static_cast<GLsizei>(film.Height()),
                    0,
                    GL_RGB,
                    GL_UNSIGNED_BYTE,
                    bytes.data());
            };

        uploadTexture();

        bool saveWasPressed = false;
        bool rerenderWasPressed = false;
        bool leftWasPressed = false;
        bool rightWasPressed = false;
        bool upWasPressed = false;
        bool downWasPressed = false;
        bool zoomInWasPressed = false;
        bool zoomOutWasPressed = false;
        bool resetWasPressed = false;

        auto requestRender =
            [&](PreviewRenderRequest request)
            {
                film = rerender(request);
                bytes = ToSRGBBytes(film);
                uploadTexture();
                SaveImage(film, options.SavePath);
                std::cout << "Rendered and saved " << options.SavePath << "\n";
            };

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
                SaveImage(film, options.SavePath);
                std::cout << "Saved " << options.SavePath << "\n";
            }

            saveWasPressed = savePressed;

            const bool rerenderPressed =
                glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

            if (rerenderPressed && !rerenderWasPressed)
            {
                // R reloads the same scene through the callback supplied by the
                // preview executable. That keeps file watching/editor behavior
                // out of this minimal viewer.
                requestRender(PreviewRenderRequest::Reload);
            }

            rerenderWasPressed = rerenderPressed;

            const bool leftPressed =
                glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
            const bool rightPressed =
                glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
            const bool upPressed =
                glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
            const bool downPressed =
                glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
            const bool zoomInPressed =
                glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
            const bool zoomOutPressed =
                glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
            const bool resetPressed =
                glfwGetKey(window, GLFW_KEY_HOME) == GLFW_PRESS;

            if (leftPressed && !leftWasPressed)
            {
                requestRender(PreviewRenderRequest::OrbitLeft);
            }

            if (rightPressed && !rightWasPressed)
            {
                requestRender(PreviewRenderRequest::OrbitRight);
            }

            if (upPressed && !upWasPressed)
            {
                requestRender(PreviewRenderRequest::OrbitUp);
            }

            if (downPressed && !downWasPressed)
            {
                requestRender(PreviewRenderRequest::OrbitDown);
            }

            if (zoomInPressed && !zoomInWasPressed)
            {
                requestRender(PreviewRenderRequest::ZoomIn);
            }

            if (zoomOutPressed && !zoomOutWasPressed)
            {
                requestRender(PreviewRenderRequest::ZoomOut);
            }

            if (resetPressed && !resetWasPressed)
            {
                requestRender(PreviewRenderRequest::ResetOrbit);
            }

            leftWasPressed = leftPressed;
            rightWasPressed = rightPressed;
            upWasPressed = upPressed;
            downWasPressed = downPressed;
            zoomInWasPressed = zoomInPressed;
            zoomOutWasPressed = zoomOutPressed;
            resetWasPressed = resetPressed;

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

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
            glColor3f(1.0f, 1.0f, 1.0f);

            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(0.0f, 0.0f);

            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(static_cast<float>(width), 0.0f);

            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(static_cast<float>(width), static_cast<float>(height));

            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(0.0f, static_cast<float>(height));
            glEnd();

            glDisable(GL_TEXTURE_2D);

            glfwSwapBuffers(window);
        }

        glDeleteTextures(1, &texture);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
