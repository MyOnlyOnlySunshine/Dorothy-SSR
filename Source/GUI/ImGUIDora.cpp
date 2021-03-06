/* Copyright (c) 2017 Jin Li, http://www.luvfight.me

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "Const/Header.h"
#include "GUI/ImGUIDora.h"
#include "Basic/Application.h"
#include "Cache/ShaderCache.h"
#include "Effect/Effect.h"
#include "Basic/Content.h"
#include "Basic/Director.h"
#include "Basic/Scheduler.h"
#include "Basic/View.h"
#include "Cache/TextureCache.h"
#include "Other/utf8.h"
#include "imgui.h"

NS_DOROTHY_BEGIN

class LogPanel
{
public:
	LogPanel():
	_scrollToBottom(false),
	_autoScroll(true)
	{
		LogHandler += std::make_pair(this, &LogPanel::addLog);
	}

	~LogPanel()
	{
		LogHandler -= std::make_pair(this, &LogPanel::addLog);
	}

	void clear()
	{
		_buf.clear();
		_lineOffsets.clear();
	}

	void addLog(const string& text)
	{
		int old_size = _buf.size();
		_buf.append("%s", text.c_str());
		for (int new_size = _buf.size(); old_size < new_size; old_size++)
		{
			if (_buf[old_size] == '\n')
			{
				_lineOffsets.push_back(old_size);
			}
		}
		_scrollToBottom = true;
    }

    void Draw(const char* title, bool* p_open = nullptr)
    {
		ImGui::SetNextWindowSize(ImVec2(400,300), ImGuiSetCond_FirstUseEver);
		ImGui::Begin(title, p_open);
		if (ImGui::Button("Clear")) clear();
		ImGui::SameLine();
		bool copy = ImGui::Button("Copy");
		ImGui::SameLine();
		if (ImGui::Checkbox("Scroll", &_autoScroll))
		{
			if (_autoScroll) _scrollToBottom = true;
		}
		ImGui::SameLine();
		_filter.Draw("Filter", -55.0f);
		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (copy) ImGui::LogToClipboard();
		if (_filter.IsActive())
		{
			const char* buf_begin = _buf.begin();
			const char* line = buf_begin;
			for (int line_no = 0; line != nullptr; line_no++)
			{
				const char* line_end = (line_no < _lineOffsets.Size) ? buf_begin + _lineOffsets[line_no] : nullptr;
				if (_filter.PassFilter(line, line_end))
				{
					ImGui::TextWrappedUnformatted(line, line_end);
				}
				line = line_end && line_end[1] ? line_end + 1 : nullptr;
			}
		}
		else
		{
			ImGui::TextWrappedUnformatted(_buf.begin(), _buf.end());
		}
		if (_scrollToBottom && _autoScroll)
		{
			ImGui::SetScrollHere(1.0f);
		}
		_scrollToBottom = false;
		ImGui::EndChild();
		ImGui::End();
	}
	
private:
	ImGuiTextBuffer _buf;
	ImGuiTextFilter _filter;
	ImVector<int> _lineOffsets;
	bool _scrollToBottom;
	bool _autoScroll;
};

ImGUIDora::ImGUIDora():
_cursor(0),
_editingDel(false),
_textLength(0),
_textEditing{},
_isLoadingFont(false),
_textInputing(false),
_mousePressed{ false, false, false },
_mouseWheel(0.0f),
_log(New<LogPanel>())
{
	_vertexDecl
		.begin()
			.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();

	SharedApplication.eventHandler += std::make_pair(this, &ImGUIDora::handleEvent);
}

ImGUIDora::~ImGUIDora()
{
	SharedApplication.eventHandler -= std::make_pair(this, &ImGUIDora::handleEvent);
	ImGui::Shutdown();
}

const char* ImGUIDora::getClipboardText(void*)
{
	return SDL_GetClipboardText();
}

void ImGUIDora::setClipboardText(void*, const char* text)
{
	SDL_SetClipboardText(text);
}

void ImGUIDora::setImePositionHint(int x, int y)
{
	int w;
	SDL_GetWindowSize(SharedApplication.getSDLWindow(), &w, nullptr);
	float scale = s_cast<float>(w) / SharedApplication.getWidth();
	int offset =
#if BX_PLATFORM_IOS
		45;
#elif BX_PLATFORM_OSX
		10;
#else
		0;
#endif
	SDL_Rect rc = { s_cast<int>(x * scale), s_cast<int>(y * scale), 0, offset };
	SharedApplication.invokeInRender([rc]()
	{
		SDL_SetTextInputRect(c_cast<SDL_Rect*>(&rc));
	});
}

void ImGUIDora::loadFontTTF(String ttfFontFile, int fontSize, String glyphRanges)
{
	if (_isLoadingFont) return;
	_isLoadingFont = true;

	Sint64 size;
	Uint8* fileData = SharedContent.loadFileUnsafe(ttfFontFile, size);

	if (!fileData)
	{
		Log("load ttf file for imgui failed!");
		return;
	}
	
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->ClearFonts();
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	fontConfig.PixelSnapH = true;
	fontConfig.OversampleH = 1;
	fontConfig.OversampleV = 1;
	io.Fonts->AddFontFromMemoryTTF(fileData, s_cast<int>(size), s_cast<float>(fontSize), &fontConfig, io.Fonts->GetGlyphRangesDefault());
	Uint8* texData;
	int width;
	int height;
	io.Fonts->GetTexDataAsAlpha8(&texData, &width, &height);
	updateTexture(texData, width, height);
	io.Fonts->ClearTexData();
	io.Fonts->ClearInputData();

	const ImWchar* targetGlyphRanges = nullptr;
	switch (Switch::hash(glyphRanges))
	{
		case "Chinese"_hash:
			targetGlyphRanges = io.Fonts->GetGlyphRangesChinese();
			break;
		case "Korean"_hash:
			targetGlyphRanges = io.Fonts->GetGlyphRangesKorean();
			break;
		case "Japanese"_hash:
			targetGlyphRanges = io.Fonts->GetGlyphRangesJapanese();
			break;
		case "Cyrillic"_hash:
			targetGlyphRanges = io.Fonts->GetGlyphRangesCyrillic();
			break;
		case "Thai"_hash:
			targetGlyphRanges = io.Fonts->GetGlyphRangesThai();
			break;
	}

	if (targetGlyphRanges)
	{
		io.Fonts->AddFontFromMemoryTTF(fileData, s_cast<int>(size), s_cast<float>(fontSize), &fontConfig, targetGlyphRanges);
		SharedAsyncThread.Process.run([]()
		{
			ImGuiIO& io = ImGui::GetIO();
			int texWidth, texHeight;
			ImVec2 texUvWhitePixel;
			unsigned char* texPixelsAlpha8;
			io.Fonts->Build(texWidth, texHeight, texUvWhitePixel, texPixelsAlpha8);
			return Values::create(texWidth, texHeight, texUvWhitePixel, texPixelsAlpha8);
		}, [this, fileData, size](Values* result)
		{
			ImGuiIO& io = ImGui::GetIO();
			result->get(io.Fonts->TexWidth, io.Fonts->TexHeight, io.Fonts->TexUvWhitePixel, io.Fonts->TexPixelsAlpha8);
			io.Fonts->Fonts.erase(io.Fonts->Fonts.begin());
			updateTexture(io.Fonts->TexPixelsAlpha8, io.Fonts->TexWidth, io.Fonts->TexHeight);
			io.Fonts->ClearTexData();
			io.Fonts->ClearInputData();
			MakeOwnArray(fileData, s_cast<size_t>(size));
			_isLoadingFont = false;
		});
	}
	else
	{
		MakeOwnArray(fileData, s_cast<size_t>(size));
		_isLoadingFont = false;
	}
}

void ImGUIDora::showStats()
{
	/* print debug text */
	ImGui::Begin("Dorothy Stats", nullptr, Vec2{195,305}, 0.8f, ImGuiWindowFlags_AlwaysAutoResize);
	const bgfx::Stats* stats = bgfx::getStats();
	const char* rendererNames[] = {
		"Noop", //!< No rendering.
		"Direct3D9", //!< Direct3D 9.0
		"Direct3D11", //!< Direct3D 11.0
		"Direct3D12", //!< Direct3D 12.0
		"Gnm", //!< GNM
		"Metal", //!< Metal
		"OpenGLES", //!< OpenGL ES 2.0+
		"OpenGL", //!< OpenGL 2.1+
		"Vulkan", //!< Vulkan
	};
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Renderer:");
	ImGui::SameLine();
	ImGui::TextUnformatted(rendererNames[bgfx::getCaps()->rendererType]);
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Multithreaded:");
	ImGui::SameLine();
	ImGui::TextUnformatted((bgfx::getCaps()->supported & BGFX_CAPS_RENDERER_MULTITHREADED) ? "true" : "false");
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Backbuffer:");
	ImGui::SameLine();
	ImGui::Text("%d x %d", stats->width, stats->height);
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Draw call:");
	ImGui::SameLine();
	ImGui::Text("%d", stats->numDraw);
	static int frames = 0;
	static double cpuTime = 0, gpuTime = 0, deltaTime = 0;
	cpuTime += SharedApplication.getCPUTime();
	gpuTime += std::abs(double(stats->gpuTimeEnd) - double(stats->gpuTimeBegin)) / double(stats->gpuTimerFreq);
	deltaTime += SharedApplication.getDeltaTime();
	frames++;
	static double lastCpuTime = 0, lastGpuTime = 0, lastDeltaTime = 1000.0 / SharedApplication.getMaxFPS();
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "CPU time:");
	ImGui::SameLine();
	ImGui::Text("%.1f ms", lastCpuTime);
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "GPU time:");
	ImGui::SameLine();
	ImGui::Text("%.1f ms", lastGpuTime);
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Delta time:");
	ImGui::SameLine();
	ImGui::Text("%.1f ms", lastDeltaTime);
	if (frames == SharedApplication.getMaxFPS())
	{
		lastCpuTime = 1000.0 * cpuTime / frames;
		lastGpuTime = 1000.0 * gpuTime / frames;
		lastDeltaTime = 1000.0 * deltaTime / frames;
		frames = 0;
		cpuTime = gpuTime = deltaTime = 0.0;
	}
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "C++ Object:");
	ImGui::SameLine();
	ImGui::Text("%d", Object::getObjectCount());
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Lua Object:");
	ImGui::SameLine();
	ImGui::Text("%d", Object::getLuaRefCount());
	ImGui::TextColored(Color(0xff00ffff).toVec4(), "Callback:");
	ImGui::SameLine();
	ImGui::Text("%d", Object::getLuaCallbackCount());
	ImGui::End();
}

void ImGUIDora::showLog()
{
	_log->Draw("Dorothy Log");
}

bool ImGUIDora::init()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.Alpha = 1.0f;
	style.WindowPadding = ImVec2(10, 10);
	style.WindowMinSize = ImVec2(100, 32);
	style.WindowRounding = 0.0f;
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.ChildWindowRounding = 0.0f;
	style.FramePadding = ImVec2(5, 5);
	style.FrameRounding = 0.0f;
	style.ItemSpacing = ImVec2(10, 10);
	style.ItemInnerSpacing = ImVec2(5, 5);
	style.TouchExtraPadding = ImVec2(5, 5);
	style.IndentSpacing = 10.0f;
	style.ColumnsMinSpacing = 5.0f;
	style.ScrollbarSize = 25.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabMinSize = 20.0f;
	style.GrabRounding = 0.0f;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.DisplayWindowPadding = ImVec2(50, 50);
	style.DisplaySafeAreaPadding = ImVec2(5, 5);
	style.AntiAliasedLines = true;
	style.AntiAliasedShapes = true;
	style.CurveTessellationTol = 1.0f;

	style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.80f);
	style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.05f, 0.10f, 0.90f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.70f, 0.70f, 0.65f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.80f, 0.80f, 0.30f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.00f, 0.80f, 0.80f, 0.40f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.00f, 0.65f, 0.65f, 0.45f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.80f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.30f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.20f, 0.20f, 0.80f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.55f, 0.55f, 0.80f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.30f, 0.30f, 0.60f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 0.40f, 0.40f, 0.30f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.40f, 0.40f, 0.40f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.50f, 0.50f, 0.40f);
	style.Colors[ImGuiCol_ComboBg] = ImVec4(0.00f, 0.20f, 0.20f, 0.99f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.90f, 0.90f, 0.50f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.30f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.40f, 0.40f, 0.60f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.40f, 0.40f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 0.40f, 0.40f, 0.45f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.55f, 0.55f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.53f, 0.53f, 0.80f);
	style.Colors[ImGuiCol_Column] = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.00f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.30f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.60f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.90f);
	style.Colors[ImGuiCol_CloseButton] = ImVec4(0.00f, 0.50f, 0.50f, 0.50f);
	style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.00f, 0.70f, 0.70f, 0.60f);
	style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 0.60f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.35f);
	style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.00f, 0.20f, 0.20f, 0.35f);

	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = SDLK_a;
	io.KeyMap[ImGuiKey_C] = SDLK_c;
	io.KeyMap[ImGuiKey_V] = SDLK_v;
	io.KeyMap[ImGuiKey_X] = SDLK_x;
	io.KeyMap[ImGuiKey_Y] = SDLK_y;
	io.KeyMap[ImGuiKey_Z] = SDLK_z;

	io.SetClipboardTextFn = ImGUIDora::setClipboardText;
	io.GetClipboardTextFn = ImGUIDora::getClipboardText;
	io.ImeSetInputScreenPosFn = ImGUIDora::setImePositionHint;
	io.ClipboardUserData = nullptr;

	_iniFilePath = SharedContent.getWritablePath() + "imgui.ini";
	io.IniFilename = _iniFilePath.c_str();

	_effect = SpriteEffect::create(
		"built-in/vs_ocornut_imgui.bin"_slice,
		"built-in/fs_ocornut_imgui.bin"_slice);

	Uint8* texData;
	int width;
	int height;
	io.Fonts->GetTexDataAsAlpha8(&texData, &width, &height);
	updateTexture(texData, width, height);
	io.Fonts->ClearTexData();
	io.Fonts->ClearInputData();

	SharedDirector.getSystemScheduler()->schedule([this](double deltaTime)
	{
		if (!_inputs.empty())
		{
			const auto& event = _inputs.front();
			ImGuiIO& io = ImGui::GetIO();
			switch (event.type)
			{
				case SDL_TEXTINPUT:
				{
					io.AddInputCharactersUTF8(event.text.text);
					break;
				}
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					int key = event.key.keysym.sym & ~SDLK_SCANCODE_MASK;
					io.KeysDown[key] = (event.type == SDL_KEYDOWN);
					if (_textLength == 0)
					{
						io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
						io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
						io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
						io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
					}
					break;
				}
			}
			_inputs.pop_front();
		}
		return false;
	});

	return true;
}

void ImGUIDora::begin()
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = s_cast<float>(SharedApplication.getWidth());
	io.DisplaySize.y = s_cast<float>(SharedApplication.getHeight());
	io.DeltaTime = s_cast<float>(SharedApplication.getDeltaTime());

	if (_textInputing != io.WantTextInput)
	{
		_textInputing = io.WantTextInput;
		if (_textInputing)
		{
			memset(_textEditing, 0, SDL_TEXTINPUTEVENT_TEXT_SIZE);
			_cursor = 0;
			_textLength = 0;
			_editingDel = false;
		}
		SharedApplication.invokeInRender([this]()
		{
			if (_textInputing) SDL_StartTextInput();
			else SDL_StopTextInput();
		});
	}

	int mx, my;
	Uint32 mouseMask = SDL_GetMouseState(&mx, &my);
	int w, h;
	SDL_Window* window = SharedApplication.getSDLWindow();
	SDL_GetWindowSize(window, &w, &h);
	mx = s_cast<int>(io.DisplaySize.x * (s_cast<float>(mx) / w));
	my = s_cast<int>(io.DisplaySize.y * (s_cast<float>(my) / h));
	bool hasMousePos = (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_FOCUS) != 0;
	io.MousePos = hasMousePos ? ImVec2((float)mx, (float)my) : ImVec2(-1, -1);
	io.MouseDown[0] = _mousePressed[0] || (mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
	io.MouseDown[1] = _mousePressed[1] || (mouseMask & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
	io.MouseDown[2] = _mousePressed[2] || (mouseMask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
	_mousePressed[0] = _mousePressed[1] = _mousePressed[2] = false;

	io.MouseWheel = _mouseWheel;
	_mouseWheel = 0.0f;

	// Hide OS mouse cursor if ImGui is drawing it
	SDL_ShowCursor(io.MouseDrawCursor ? 0 : 1);

	// Start the frame
	ImGui::NewFrame();
}

void ImGUIDora::end()
{
	ImGui::Render();
}

inline bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexDecl& _decl, uint32_t _numIndices)
{
	return _numVertices == bgfx::getAvailTransientVertexBuffer(_numVertices, _decl)
		&& _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices);
}

void ImGUIDora::render()
{
	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData->CmdListsCount == 0)
	{
		return;
	}

	SharedView.pushName("ImGui"_slice, [&]()
	{
		Uint8 viewId = SharedView.getId();
		const ImGuiIO& io = ImGui::GetIO();
		const float width = io.DisplaySize.x;
		const float height = io.DisplaySize.y;
		{
			float ortho[16];
			bx::mtxOrtho(ortho, 0.0f, width, height, 0.0f, -1.0f, 1.0f);
			bgfx::setViewTransform(viewId, nullptr, ortho);
		}

		ImGUIDora* guiDora = SharedImGUI.getTarget();
		bgfx::TextureHandle textureHandle = guiDora->_fontTexture->getHandle();
		bgfx::UniformHandle sampler = guiDora->_effect->getSampler();
		bgfx::ProgramHandle program = guiDora->_effect->apply();

		// Render command lists
		for (int32_t ii = 0, num = drawData->CmdListsCount; ii < num; ++ii)
		{
			bgfx::TransientVertexBuffer tvb;
			bgfx::TransientIndexBuffer tib;

			const ImDrawList* drawList = drawData->CmdLists[ii];
			uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
			uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();

			if (!checkAvailTransientBuffers(numVertices, guiDora->_vertexDecl, numIndices))
			{
				Log("not enough space in transient buffer just quit drawing the rest.");
				break;
			}

			bgfx::allocTransientVertexBuffer(&tvb, numVertices, guiDora->_vertexDecl);
			bgfx::allocTransientIndexBuffer(&tib, numIndices);

			ImDrawVert* verts = (ImDrawVert*)tvb.data;
			memcpy(verts, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));

			ImDrawIdx* indices = (ImDrawIdx*)tib.data;
			memcpy(indices, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

			uint32_t offset = 0;
			for (const ImDrawCmd* cmd = drawList->CmdBuffer.begin(), *cmdEnd = drawList->CmdBuffer.end(); cmd != cmdEnd; ++cmd)
			{
				if (cmd->UserCallback)
				{
					cmd->UserCallback(drawList, cmd);
				}
				else if (0 != cmd->ElemCount)
				{
					if (nullptr != cmd->TextureId)
					{
						union
						{
							ImTextureID ptr;
							struct { bgfx::TextureHandle handle; } s;
						} texture = { cmd->TextureId };
						textureHandle = texture.s.handle;
					}

					uint64_t state = 0
						| BGFX_STATE_RGB_WRITE
						| BGFX_STATE_ALPHA_WRITE
						| BGFX_STATE_MSAA
						| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);

					const uint16_t xx = uint16_t(bx::fmax(cmd->ClipRect.x, 0.0f));
					const uint16_t yy = uint16_t(bx::fmax(cmd->ClipRect.y, 0.0f));
					bgfx::setScissor(xx, yy,
						uint16_t(bx::fmin(cmd->ClipRect.z, 65535.0f) - xx),
						uint16_t(bx::fmin(cmd->ClipRect.w, 65535.0f) - yy));
					bgfx::setState(state);
					bgfx::setTexture(0, sampler, textureHandle);
					bgfx::setVertexBuffer(&tvb, 0, numVertices);
					bgfx::setIndexBuffer(&tib, offset, cmd->ElemCount);
					bgfx::submit(viewId, program);
				}

				offset += cmd->ElemCount;
			}
		}
	});
}

void ImGUIDora::sendKey(int key, int count)
{
	for (int i = 0; i < count; i++)
	{
		SDL_Event e;
		e.type = SDL_KEYDOWN;
		e.key.keysym.sym = key;
		_inputs.push_back(e);
		e.type = SDL_KEYUP;
		_inputs.push_back(e);
	}
}

void ImGUIDora::updateTexture(Uint8* data, int width, int height)
{
	const Uint32 textureFlags = BGFX_TEXTURE_MIN_POINT | BGFX_TEXTURE_MAG_POINT;

	bgfx::TextureHandle textureHandle = bgfx::createTexture2D(
		s_cast<uint16_t>(width), s_cast<uint16_t>(height),
		false, 1, bgfx::TextureFormat::A8, textureFlags,
		bgfx::copy(data, width*height * 1));

	bgfx::TextureInfo info;
	bgfx::calcTextureSize(info,
		s_cast<uint16_t>(width), s_cast<uint16_t>(height),
		0, false, false, 1, bgfx::TextureFormat::A8);

	_fontTexture = Texture2D::create(textureHandle, info, textureFlags);
}

void ImGUIDora::handleEvent(const SDL_Event& event)
{
	switch (event.type)
	{
		case SDL_MOUSEWHEEL:
		{
			if (event.wheel.y > 0)
			{
				_mouseWheel = 1;
			}
			if (event.wheel.y < 0)
			{
				_mouseWheel = -1;
			}
			break;
		}
		case SDL_MOUSEBUTTONDOWN:
		{
			if (event.button.button == SDL_BUTTON_LEFT) _mousePressed[0] = true;
			if (event.button.button == SDL_BUTTON_RIGHT) _mousePressed[1] = true;
			if (event.button.button == SDL_BUTTON_MIDDLE) _mousePressed[2] = true;
			break;
		}
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			int key = event.key.keysym.sym & ~SDLK_SCANCODE_MASK;
			if (event.type == SDL_KEYDOWN && key == SDLK_BACKSPACE)
			{
				if (!_editingDel)
				{
					_inputs.push_back(event);
				}
			}
			else if (_textLength == 0)
			{
				_inputs.push_back(event);
			}
			break;
		}
		case SDL_TEXTINPUT:
		{
			const char* newText = event.text.text;
			if (strcmp(newText, _textEditing) != 0)
			{
				if (_textLength > 0)
				{
					if (_cursor < _textLength)
					{
						sendKey(SDLK_RIGHT, _textLength - _cursor);
					}
					sendKey(SDLK_BACKSPACE, _textLength);
				}
				_inputs.push_back(event);
			}
			memset(_textEditing, 0, SDL_TEXTINPUTEVENT_TEXT_SIZE);
			_cursor = 0;
			_textLength = 0;
			_editingDel = false;
			break;
		}
		case SDL_TEXTEDITING:
		{
			Sint32 cursor = event.edit.start;
			const char* newText = event.edit.text;
			if (strcmp(newText, _textEditing) != 0)
			{
				size_t lastLength = strlen(_textEditing);
				size_t newLength = strlen(newText);
				const char* oldChar = _textEditing;
				const char* newChar = newText;
				if (_cursor < _textLength)
				{
					sendKey(SDLK_RIGHT, _textLength - _cursor);
				}
				while (*oldChar == *newChar && *oldChar != '\0' && *newChar != '\0')
				{
					oldChar++; newChar++;
				}
				size_t toDel = _textEditing + lastLength - oldChar;
				size_t toAdd = newText + newLength - newChar;
				if (toDel > 0)
				{
					int charCount = utf8_count_characters(oldChar);
					_textLength -= charCount;
					sendKey(SDLK_BACKSPACE, charCount);
				}
				_editingDel = (toDel > 0);
				if (toAdd > 0)
				{
					SDL_Event e;
					e.type = SDL_TEXTINPUT;
					_textLength += utf8_count_characters(newChar);
					memcpy(e.text.text, newChar, toAdd + 1);
					_inputs.push_back(e);
				}
				memcpy(_textEditing, newText, SDL_TEXTINPUTEVENT_TEXT_SIZE);
				_cursor = cursor;
				sendKey(SDLK_LEFT, _textLength - _cursor);
			}
			else if (_cursor != cursor)
			{
				if (_cursor < cursor)
				{
					sendKey(SDLK_RIGHT, cursor - _cursor);
				}
				else
				{
					sendKey(SDLK_LEFT, _cursor - cursor);
				}
				_cursor = cursor;
			}
			break;
		}
	}
}

bool ImGUIDora::handle(const SDL_Event& event)
{
	return ImGui::IsAnyItemActive();
}

NS_DOROTHY_END
