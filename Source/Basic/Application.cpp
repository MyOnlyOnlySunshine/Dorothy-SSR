/* Copyright (c) 2016 Jin Li, http://www.luvfight.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "Const/Header.h"
#include "Basic/Application.h"
#include "bx/timer.h"

NS_DOROTHY_BEGIN

int Application::winWidth = 800;
int Application::winHeight = 600;
bool Application::running = true;

// This function runs in main thread, and do render work
int Application::run()
{
	SDL_Init(SDL_INIT_EVENTS);

	Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
#if BX_PLATFORM_IOS || BX_PLATFORM_ANDROID
	windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif

	SDL_Window* window = SDL_CreateWindow("Study BGFX & SDL",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		winWidth, winHeight, windowFlags);

	setSdlWindow(window);

	bgfx::renderFrame();

	bx::Thread thread;
	thread.init(Application::mainLogic);

	SDL_Event event;
	while (running)
	{
		// do render staff and swap buffers
		bgfx::renderFrame();
		// handle SDL event in this main thread only
		while(SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
    			running = false;
    			break;
			default:
				break;
			}
		}
	}

	// wait for render process to stop
	while (bgfx::RenderFrame::NoContext != bgfx::renderFrame());
	thread.shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();

	return thread.getExitCode();
}

void Application::setSdlWindow(SDL_Window* window)
{
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	SDL_GetWindowWMInfo(window, &wmi);

	bgfx::PlatformData pd;
#if BX_PLATFORM_OSX
	pd.ndt = nullptr;
	pd.nwh = wmi.info.cocoa.window;
#elif BX_PLATFORM_WINDOWS
	pd.ndt = nullptr;
	pd.nwh = wmi.info.win.window;
#elif BX_PLATFORM_ANDROID
	pd.ndt = nullptr;
	pd.nwh = wmi.info.android.window;
	SDL_GL_GetDrawableSize(window, &winWidth, &winHeight);
#endif
	pd.context = nullptr;
	pd.backBuffer = nullptr;
	pd.backBufferDS = nullptr;
	bgfx::setPlatformData(pd);
}

int Application::mainLogic(void* userData)
{
	// Initialization
	bgfx::init();
	bgfx::reset(winWidth, winHeight, BGFX_RESET_VSYNC);
	bgfx::setDebug(BGFX_DEBUG_TEXT);
	bgfx::setViewClear(0,
		BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH,
		0x303030ff, 1.0f, 0);
	bgfx::frame();

	SharedLueEngine.executeScriptFile("Script/main");

	// Update and invoke render apis
	while (running)
	{
		SharedPoolManager.push();
		bgfx::setViewRect(0, 0, 0, winWidth, winHeight);

		// This dummy draw call is here to make sure that view 0 is cleared
		// if no other draw calls are submitted to view 0.
		bgfx::touch(0);

		// Use debug font to print information about this example.
		bgfx::dbgTextClear();
		bgfx::dbgTextPrintf(0, 1, 0x4f, "bgfx/examples/00-helloworld");
		bgfx::dbgTextPrintf(0, 2, 0x6f, "Description: Initialization and debug text.");

		bgfx::dbgTextPrintf(0, 4, 0x0f, "Color can be changed with ANSI \x1b[9;me\x1b[10;ms\x1b[11;mc\x1b[12;ma\x1b[13;mp\x1b[14;me\x1b[0m code too.");

		const bgfx::Stats* stats = bgfx::getStats();
		bgfx::dbgTextPrintf(0, 6, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters."
				, stats->width
				, stats->height
				, stats->textWidth
				, stats->textHeight);

		// Advance to next frame. Rendering thread will be kicked to
		// process submitted rendering primitives.
		bgfx::frame();
		SharedPoolManager.pop();
	}

	// Shut down frameworks
	bgfx::shutdown();
	SDL_Event event;
	SDL_QuitEvent& qev = event.quit;
	qev.type = SDL_QUIT;
	SDL_PushEvent(&event);
	return 0;
}

NS_DOROTHY_END
/*
// Entry functions needed by SDL2
#if BX_PLATFORM_OSX || BX_PLATFORM_ANDROID
int main(int argc, char *argv[])
{
	Dorothy::App app;
	return app.run();
}
#elif BX_PLATFORM_WINDOWS
int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
#ifndef NDEBUG
	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
#endif

	Dorothy::App app;
	int result = app.run();

#ifndef NDEBUG
	FreeConsole();
#endif

	return result;
}
#endif // BX_PLATFORM_
*/