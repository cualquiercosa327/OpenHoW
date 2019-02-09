/* OpenHoW
 * Copyright (C) 2017-2019 Mark Sowden <markelswo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL2/SDL.h>

#include <PL/platform_filesystem.h>
#include <PL/platform_graphics_camera.h>

#include "engine.h"
#include "input.h"
#include "imgui_layer.h"

#include "../3rdparty/imgui/examples/imgui_impl_sdl.h"
#include "../3rdparty/imgui/examples/imgui_impl_opengl2.h"

#include "client/display.h"

static SDL_Window *window = nullptr;
static SDL_GLContext gl_context = nullptr;

static PLCamera *imgui_camera = nullptr;

unsigned int System_GetTicks(void) {
    return SDL_GetTicks();
}

void System_DisplayMessageBox(unsigned int level, const char *msg, ...) {
    switch(level) {
        case PROMPT_LEVEL_ERROR: {
            level = SDL_MESSAGEBOX_ERROR;
        } break;
        case PROMPT_LEVEL_WARNING: {
            level = SDL_MESSAGEBOX_WARNING;
        } break;
        case PROMPT_LEVEL_DEFAULT: {
            level = SDL_MESSAGEBOX_INFORMATION;
        } break;

        default: return;
    }

    char buf[2048];
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    SDL_ShowSimpleMessageBox(level, ENGINE_TITLE, msg, window);
}

const char *System_GetClipboardText(void*) {
    static char *clipboard = nullptr;
    if(clipboard != nullptr) {
        SDL_free(clipboard);
    }

    clipboard = SDL_GetClipboardText();
    return clipboard;
}

void System_SetClipboardText(void*, const char *text) {
    SDL_SetClipboardText(text);
}

void System_DisplayWindow(bool fullscreen, int width, int height) {
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, 8);

#if 1
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

#if 1
    if(SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_GL_SetSwapInterval(1);
    }
#endif

    unsigned int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS;
    if(fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    } else {
        flags |= SDL_WINDOW_SHOWN;
    }

    window = SDL_CreateWindow(
            ENGINE_TITLE,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width, height,
            flags
    );
    if(window == nullptr) {
        System_DisplayMessageBox(PROMPT_LEVEL_ERROR, "Failed to create window!\n%s", SDL_GetError());
        Engine_Shutdown();
    }

    if((gl_context = SDL_GL_CreateContext(window)) == nullptr) {
        System_DisplayMessageBox(PROMPT_LEVEL_ERROR, "Failed to create context!\n%s", SDL_GetError());
        Engine_Shutdown();
    }

    /* setup imgui integration */

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();

    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    //io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
    io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
    io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
    io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
    io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
    io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
    io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
    io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
    io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
    io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

    io.SetClipboardTextFn = System_SetClipboardText;
    io.GetClipboardTextFn = System_GetClipboardText;
    io.ClipboardUserData = nullptr;

#ifdef _WIN32
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    io.ImeWindowHandle = wmInfo.info.win.window;
#endif

    ImGui_ImplOpenGL2_Init();

    ImGui::StyleColorsDark();
}

void System_SetWindowTitle(const char *title) {
    SDL_SetWindowTitle(window, title);
}

bool System_SetWindowSize(int *width, int *height, bool fs) {
    SDL_SetWindowSize(window, *width, *height);
    SDL_SetWindowFullscreen(window, (fs ? SDL_WINDOW_FULLSCREEN : 0));

    // ensure that the window updated successfully
    SDL_DisplayMode mode;
    if(SDL_GetWindowDisplayMode(window, &mode) == 0) {
        if(mode.w != *width || mode.h != *height) {
            if(mode.w < 0) mode.w = 0;
            if(mode.h < 0) mode.h = 0;
            *width = mode.w;
            *height = mode.h;
            return false;
        }
    }

    /* ensure that imgui is kept up to date */

    ImGuiIO &io = ImGui::GetIO();

    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GL_GetDrawableSize(window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    return true;
}

void System_SwapDisplay(void) {
    SDL_GL_SwapWindow(window);
}

void System_Shutdown(void) {
    Engine_Shutdown();

    ImGui_ImplOpenGL2_DestroyDeviceObjects();
    ImGui::DestroyContext();

    SDL_StopTextInput();

    if(gl_context != nullptr) {
        SDL_GL_DeleteContext(gl_context);
    }

    if(window != nullptr) {
        SDL_DestroyWindow(window);
    }

    SDL_ShowCursor(SDL_TRUE);

    SDL_EnableScreenSaver();
    SDL_Quit();

    exit(EXIT_SUCCESS);
}

///////////////////////////////////////////////////

static int TranslateSDLKey(int key) {
    if(key < 128) {
        return key;
    }

    switch(key) {
        case SDLK_F1: return PORK_KEY_F1;
        case SDLK_F2: return PORK_KEY_F2;
        case SDLK_F3: return PORK_KEY_F3;
        case SDLK_F4: return PORK_KEY_F4;
        case SDLK_F5: return PORK_KEY_F5;
        case SDLK_F6: return PORK_KEY_F6;
        case SDLK_F7: return PORK_KEY_F7;
        case SDLK_F8: return PORK_KEY_F8;
        case SDLK_F9: return PORK_KEY_F9;
        case SDLK_F10: return PORK_KEY_F10;
        case SDLK_F11: return PORK_KEY_F11;
        case SDLK_F12: return PORK_KEY_F12;

        case SDLK_ESCAPE: return PORK_KEY_ESCAPE;

        case SDLK_PAUSE: return PORK_KEY_PAUSE;
        case SDLK_INSERT: return PORK_KEY_INSERT;
        case SDLK_HOME: return PORK_KEY_HOME;

        case SDLK_UP: return PORK_KEY_UP;
        case SDLK_DOWN: return PORK_KEY_DOWN;
        case SDLK_LEFT: return PORK_KEY_LEFT;
        case SDLK_RIGHT: return PORK_KEY_RIGHT;

        case SDLK_SPACE: return PORK_KEY_SPACE;

        case SDLK_LSHIFT: return PORK_KEY_LSHIFT;
        case SDLK_RSHIFT: return PORK_KEY_RSHIFT;

        case SDLK_PAGEUP: return PORK_KEY_PAGEUP;
        case SDLK_PAGEDOWN: return PORK_KEY_PAGEDOWN;

        default: return -1;
    }
}

static int TranslateSDLMouseButton(int button) {
    switch(button) {
        case SDL_BUTTON_LEFT: return PORK_MOUSE_BUTTON_LEFT;
        case SDL_BUTTON_RIGHT: return PORK_MOUSE_BUTTON_RIGHT;
        case SDL_BUTTON_MIDDLE: return PORK_MOUSE_BUTTON_MIDDLE;
        default: return -1;
    }
}

static int TranslateSDLButton(int button) {
    switch(button) {
        case SDL_CONTROLLER_BUTTON_A: return PORK_BUTTON_CROSS;
        case SDL_CONTROLLER_BUTTON_B: return PORK_BUTTON_CIRCLE;
        case SDL_CONTROLLER_BUTTON_X: return PORK_BUTTON_SQUARE;
        case SDL_CONTROLLER_BUTTON_Y: return PORK_BUTTON_TRIANGLE;

        case SDL_CONTROLLER_BUTTON_BACK: return PORK_BUTTON_SELECT;
        case SDL_CONTROLLER_BUTTON_START: return PORK_BUTTON_START;

        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return PORK_BUTTON_L1;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return PORK_BUTTON_L3;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return PORK_BUTTON_R1;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return PORK_BUTTON_R3;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return PORK_BUTTON_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return PORK_BUTTON_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return PORK_BUTTON_RIGHT;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return PORK_BUTTON_UP;

        default: return -1;
    }
}

static void PollEvents() {
    ImGuiIO &io = ImGui::GetIO();

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            default:break;

            case SDL_KEYUP:
            case SDL_KEYDOWN: {
                if(io.WantCaptureKeyboard) {
                    int key = event.key.keysym.scancode;
                    IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
                    io.KeysDown[key] = (event.type == SDL_KEYDOWN);
                    io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
                    io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
                    io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
                    io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
                    break;
                }

                int key = TranslateSDLKey(event.key.keysym.sym);
                if(key != -1) {
                    Input_SetKeyState(key, (event.type == SDL_KEYDOWN));
                }
            } break;

            case SDL_TEXTINPUT: {
                if(io.WantCaptureKeyboard) {
                    io.AddInputCharactersUTF8(event.text.text);
                    break;
                }
            } break;

            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN: {
                if(io.WantCaptureMouse) {
                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT:
                            io.MouseDown[0] = event.button.state;
                            break;
                        case SDL_BUTTON_RIGHT:
                            io.MouseDown[1] = event.button.state;
                            break;
                        case SDL_BUTTON_MIDDLE:
                            io.MouseDown[2] = event.button.state;
                            break;

                        default:break;
                    }

                    break;
                }

                int button = TranslateSDLMouseButton(event.button.button);
                Input_SetMouseState(event.motion.x, event.motion.y, button, event.button.state);
            } break;

            case SDL_MOUSEWHEEL: {
                if(event.wheel.x > 0) {
                    io.MouseWheelH += 1;
                } else if(event.wheel.x < 0) {
                    io.MouseWheelH -= 1;
                }

                if(event.wheel.y > 0) {
                    io.MouseWheel += 1;
                } else if(event.wheel.y < 0) {
                    io.MouseWheel -= 1;
                }
            } break;

            case SDL_MOUSEMOTION: {
                io.MousePos.x = event.motion.x;
                io.MousePos.y = event.motion.y;

                Input_SetMouseState(event.motion.x, event.motion.y, -1, false);
            } break;

            case SDL_CONTROLLERBUTTONUP: {
                int button = TranslateSDLButton(event.cbutton.button);
                if(button != -1) {
                    Input_SetButtonState((unsigned int) event.cbutton.which, button, false);
                }
            } break;

            case SDL_CONTROLLERBUTTONDOWN: {
                int button = TranslateSDLButton(event.cbutton.button);
                if(button != -1) {
                    Input_SetButtonState((unsigned int) event.cbutton.which, button, true);
                }
            } break;

            case SDL_CONTROLLERAXISMOTION: {
                if(event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT && event.caxis.value > 1000) {
                    Input_SetButtonState((unsigned int) event.cbutton.which, PORK_BUTTON_L2, true);
                } else if(event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT && event.caxis.value <= 1000) {
                    Input_SetButtonState((unsigned int) event.cbutton.which, PORK_BUTTON_L2, false);
                }

                if(event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT && event.caxis.value > 1000) {
                    Input_SetButtonState((unsigned int) event.cbutton.which, PORK_BUTTON_R2, true);
                } else if(event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT && event.caxis.value <= 1000){
                    Input_SetButtonState((unsigned int) event.cbutton.which, PORK_BUTTON_R2, false);
                }
            } break;

            case SDL_QUIT: {
                Engine_Shutdown();
            } break;

            case SDL_WINDOWEVENT: {
                if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    Display_UpdateViewport(0, 0, (unsigned int) event.window.data1, (unsigned int) event.window.data2);

                    imgui_camera->viewport.w = event.window.data1;
                    imgui_camera->viewport.h = event.window.data2;
                }
            } break;
        }
    }
}

static void *u_malloc(size_t size) {
    return u_alloc(1, size, true);
}

static void *u_calloc(size_t num, size_t size) {
    return u_alloc(num, size, true);
}

int main(int argc, char **argv) {
    pl_malloc = u_malloc;
    pl_calloc = u_calloc;

    plInitialize(argc, argv);

    char app_dir[PL_SYSTEM_MAX_PATH];
    plGetApplicationDataDirectory(ENGINE_APP_NAME, app_dir, PL_SYSTEM_MAX_PATH);

    if(!plCreatePath(app_dir)) {
        System_DisplayMessageBox(PROMPT_LEVEL_WARNING,
                                "Unable to create %s: %s\n"
                                "Settings will not be saved.", app_dir, plGetError());
    }

    std::string log_path = std::string(app_dir) + "/" + ENGINE_LOG;
    plSetupLogOutput(log_path.c_str());

    plSetupLogLevel(LOG_LEVEL_DEFAULT, "info", PLColour(0, 255, 0, 255), true);
    plSetupLogLevel(LOG_LEVEL_WARNING, "warning", PLColour(255, 255, 0, 255), true);
    plSetupLogLevel(LOG_LEVEL_ERROR, "error", PLColour(255, 0, 0, 255), true);
    plSetupLogLevel(LOG_LEVEL_DEBUG, "debug", PLColour(0, 255, 255, 255),
#ifdef _DEBUG
        true
#else
        false
#endif
    );

    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        System_DisplayMessageBox(PROMPT_LEVEL_ERROR, "Failed to initialize SDL2!\n%s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_DisableScreenSaver();

    Engine_Initialize();

    // deal with any console vars provided (todo: pl should deal with this?)
    for(int i = 1; i < argc; ++i) {
        if(pl_strncasecmp("+", argv[i], 1) == 0) {
            plParseConsoleString(argv[i] + 1);
            ++i;
            // todo: deal with other var arguments ... :(
        }
    }

    /* setup the camera we'll use for drawing the imgui overlay */

    if((imgui_camera = plCreateCamera()) == nullptr) {
        Error("failed to create ui camera, aborting!\n%s\n", plGetError());
    }

    imgui_camera->mode         = PL_CAMERA_MODE_ORTHOGRAPHIC;
    imgui_camera->fov          = 90;
    imgui_camera->near         = 0;
    imgui_camera->far          = 1000;
    imgui_camera->viewport.w   = cv_display_width->i_value;
    imgui_camera->viewport.h   = cv_display_height->i_value;

    //SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_CaptureMouse(SDL_TRUE);
    SDL_ShowCursor(SDL_TRUE);

    /* using this to catch modified keys
     * without having to do the conversion
     * ourselves                            */
    SDL_StartTextInput();

#define TICKS_PER_SECOND    25
#define SKIP_TICKS          (1000 / TICKS_PER_SECOND)
#define MAX_FRAMESKIP       5

    unsigned int next_tick = System_GetTicks();
    unsigned int loops;
    double delta_time;

    while(Engine_IsRunning()) {
        PollEvents();

        loops = 0;
        while(System_GetTicks() > next_tick && loops < MAX_FRAMESKIP) {
            Engine_Simulate();
            next_tick += SKIP_TICKS;
            loops++;
        }

        ImGui_ImplOpenGL2_NewFrame();
        ImGui::NewFrame();

        delta_time = (double)(System_GetTicks() + SKIP_TICKS - next_tick) / (double)(SKIP_TICKS);
        Display_SetupDraw(delta_time);

        Display_DrawScene();
        Display_DrawInterface();

        /* now render imgui */

        ImGui::Render();

        plSetupCamera(imgui_camera);
        plSetShaderProgram(nullptr);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        /* and finally, swap */

        Display_Flush();
    }

    System_Shutdown();

    return EXIT_SUCCESS;
}
