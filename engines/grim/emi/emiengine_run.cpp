/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
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
 *
 */

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "graphics/cursorman.h"
#include "graphics/renderer.h"
#include "graphics/surface.h"

#include "video/qt_decoder.h"

#include "engines/grim/emi/resources/cbundle.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/core/cgrimeconsole.h"
#include "engines/grim/emi/graphics/coglcontext.h"
#include "engines/grim/emi/core/cpathingfileservices.h"
#include "engines/grim/emi/graphics/ctinyglcontext.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/keygenerator.h"
#include "engines/grim/emi/images/labitmap.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/text/polyglot_v2.h"
#include "engines/grim/emi/files/zfile.h"

namespace Grim {

static const int kScreenWidth = 640;
static const int kScreenHeight = 480;
static const int kLoadScreenPs2Lines = 448;

Common::Error EmiEngine::run() {
	if (!checkGameData("Escape from Monkey Island"))
		return Common::kUserCanceled;

	mountEmiPatchArchives();

	_pathingFileServices = new CPathingFileServices(this);
	new LConsoleServices(this);
	_zFileServices = new ZFileServices(this);

	_keyGenerator = new KeyGenerator(this);

	bool initialized = initialize();

	if (initialized) {
		if (ConfMan.hasKey("save_slot"))
			loadGameState(ConfMan.getInt("save_slot"));

		while (!shouldQuit()) {
			uint32 startTime = g_system->getMillis();

			if (!runFrame())
				break;

			pumpEvents();

			uint32 endTime = g_system->getMillis();

			if (endTime >= startTime && endTime - startTime < _speedLimitMs)
				g_system->delayMillis(_speedLimitMs - (endTime - startTime));
		}
	}

	shutdown();

	delete _zFileServices;
	_zFileServices = nullptr;

	deleteAllConsoles();
	_grimeConsole = nullptr;
	deleteAllFileServices();
	_pathingFileServices = nullptr;

	delete _keyGenerator;
	_keyGenerator = nullptr;

	if (!initialized)
		return Common::kUnknownError;

	return Common::kNoError;
}

CDrawContext *EmiEngine::createDrawContext() {
	uint32 availableTypes = Graphics::Renderer::getAvailableTypes();
	Graphics::RendererType desiredType = Graphics::Renderer::parseTypeCode(ConfMan.get("renderer"));

#if defined(USE_TINYGL)
	if (desiredType == Graphics::kRendererTypeTinyGL && (availableTypes & Graphics::kRendererTypeTinyGL))
		return new CTinyGLContext(this);
#endif

#if defined(USE_OPENGL_GAME)
	if (availableTypes & Graphics::kRendererTypeOpenGL)
		return new COGLContext(this);
#endif

#if defined(USE_TINYGL)
	if (availableTypes & Graphics::kRendererTypeTinyGL)
		return new CTinyGLContext(this);
#endif

	return nullptr;
}

bool EmiEngine::openDrawContext(CDrawContext *drawContext, DisplayMode *mode) {
	mode->bpp = 32;

	if (drawContext->open(mode))
		return true;

	mode->bpp = 16;

	return drawContext->open(mode);
}

bool EmiEngine::initialize() {
	_polyglot->configure("Install/messages.tab");

	// The demo does not look for a disc, its data are in the game directory
	if (!_isDemo && !findDisc(1) && !findDisc(2)) {
		showMessage("/GOT_CD/Please make sure that the game is properly installed and that the CD is in your drive.",
				"/NO_FILES/Could not find files!");

		return false;
	}

	CDrawContext *drawContext = createDrawContext();

	if (!drawContext) {
		showMessage("/OTHER_APP/Please make sure you are not running any application that takes exclusive control of your hardware.",
				"/HARDWARE/Could not initialize your hardware!");

		return false;
	}

	setDrawContext(drawContext);
	drawContext->enumerateDevices();

	DisplayMode mode;

	mode.width = kScreenWidth;
	mode.height = kScreenHeight;
	mode.bpp = 32;
	mode.reserved12[0] = 2;
	mode.reserved12[1] = 16;
	mode.flags = 0;

	if (ConfMan.getBool("fullscreen"))
		mode.flags |= 1;

	bool opened = openDrawContext(drawContext, &mode);

#if defined(USE_TINYGL)
	if (!opened && dynamic_cast<CTinyGLContext *>(drawContext) == nullptr && (Graphics::Renderer::getAvailableTypes() & Graphics::kRendererTypeTinyGL)) {
		warning("EmiEngine::initialize(): the OpenGL context could not be opened, using the software renderer");

		delete drawContext;
		drawContext = new CTinyGLContext(this);
		setDrawContext(drawContext);
		drawContext->enumerateDevices();
		opened = openDrawContext(drawContext, &mode);
	}
#endif

	if (!opened) {
		showMessage("/OTHER_APP/Please make sure you are not running any application that takes exclusive control of your hardware.",
				"/HARDWARE/Could not initialize your hardware!");

		return false;
	}

	if (getGamePlatform() == Common::kPlatformPS2)
		showLoadScreenPs2(drawContext);

	drawContext->setTextureFlag(false);
	CursorMan.showMouse(false);
	_grimeConsole = new CGrimeConsole(this);
	_grimeConsole->setDebugLog(true);

	// PS2 opens the bundles before it starts the modules, they load the loading loop and the fonts
	bool bundlesFirst = getGamePlatform() == Common::kPlatformPS2;

	if (bundlesFirst && !CBundleServices::init(this, getGameLanguage() != Common::EN_ANY)) {
		showMessage("/NO_FILES/Could not find files!", "/ERROR/Error!");

		return false;
	}

	if (!startupModules(drawContext))
		return false;

	if (_showFps)
		_lua->_statsOverlayFlags |= Lua_V2::kStatsOverlayFlag;

	if (!bundlesFirst && !CBundleServices::init(this, getGameLanguage() != Common::EN_ANY)) {
		showMessage("/NO_FILES/Could not find files!", "/ERROR/Error!");

		return false;
	}

	// PS2 loads the loading icon in startupModules, which it runs after opening the bundles
	if (getGamePlatform() == Common::kPlatformPS2)
		Movie::loadLoadingIconPs2(this);

	ZFile *defaultFile = _zFileServices->open("default.tga", "rb");

	if (defaultFile) {
		LABitmap *defaultBitmap = new LABitmap(this);
		bool loaded = defaultBitmap->loadFile("default.tga");

		_zFileServices->closeZFile(defaultFile);

		if (loaded)
			drawContext->_defaultTexture = drawContext->createTexture(defaultBitmap);

		delete defaultBitmap;
	}

	// The demo leaves the cached texture slots empty, they give the default texture
	if (!_isDemo) {
		LABitmap whiteBitmap(this);

		whiteBitmap.create(2, 2, LColorFormat(9), nullptr);

		uint32 *whitePixels = (uint32 *)whiteBitmap._pixels;

		if (whitePixels) {
			for (int i = 0; i < 4; i++)
				whitePixels[i] = 0xFFFFFFFF;
		}

		for (int i = 0; i < CDrawContext::kNumCachedTextures; i++)
			drawContext->setCachedTexture(i, &whiteBitmap);
	}

	pixelTest();

	// The demo splash stays on screen until the first game frame
	if (_isDemo)
		showSplash(drawContext);

	setShutdownCallback(shutdownCallback);
	_polyglot->shutdown();
	_polyglot->configure("script.tab");

	if (getGamePlatform() == Common::kPlatformMacintosh)
		playAspyrLogo(drawContext);

	return openGameview(nullptr);
}

void EmiEngine::playAspyrLogo(CDrawContext *drawContext) {
	Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(Common::Path("AMWI.m4b"));

	if (!stream)
		return;

	Video::QuickTimeDecoder decoder;

	if (!decoder.loadStream(stream)) {
		warning("EmiEngine::playAspyrLogo(): cannot open AMWI.m4b");

		return;
	}

	Common::Event event;

	while (g_system->getEventManager()->pollEvent(event)) {
	}

	decoder.start();

	bool skipped = false;

	while (!shouldQuit() && !skipped && !decoder.endOfVideo()) {
		if (decoder.needsUpdate()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();

			if (frame && drawContext->beginScene()) {
				drawContext->drawMovieFrame(*frame);
				drawContext->endScene();
				drawContext->present(true);
			}
		}

		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_KEYDOWN:
			case Common::EVENT_LBUTTONDOWN:
			case Common::EVENT_RBUTTONDOWN:
			case Common::EVENT_MBUTTONDOWN:
			case Common::EVENT_JOYBUTTON_DOWN:
			case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
				skipped = true;

				break;

			default:
				break;
			}
		}

		g_system->delayMillis(10);
	}

	decoder.stop();
}

void EmiEngine::showLoadScreenPs2(CDrawContext *drawContext) {
	LABitmap *bitmap = new LABitmap(this);

	if (bitmap->loadFile("LOAD.TGA")) {
		int width = bitmap->getWidth();
		int height = drawContext->_displayMode.height;
		int lines = MIN<int>(bitmap->getHeight(), kLoadScreenPs2Lines);
		int bytesPerPixel = bitmap->_colorFormat._bytesPerPixel;
		const byte *pixels = (const byte *)bitmap->getPixels();
		Graphics::PixelFormat format = drawContext->getMovieFormat();
		Graphics::Surface screen;

		screen.create(width, height, format);

		// PS2 stretches the top 448 lines of the picture over the whole screen
		for (int y = 0; y < height; y++) {
			const byte *source = pixels + (y * lines / height) * bitmap->getStride();

			for (int x = 0; x < width; x++) {
				const byte *pixel = source + x * bytesPerPixel;
				int red = bitmap->_colorFormat.getRed(pixel);
				int green = bitmap->_colorFormat.getGreen(pixel);
				int blue = bitmap->_colorFormat.getBlue(pixel);

				screen.setPixel(x, y, format.RGBToColor(red, green, blue));
			}
		}

		if (drawContext->beginScene()) {
			drawContext->drawMovieFrame(screen);
			drawContext->endScene();
		}

		drawContext->present(true);
		screen.free();
	}

	delete bitmap;
}

void EmiEngine::shutdown() {
	closeGameview();
	shutdownModules();

	if (_grimeConsole)
		_grimeConsole->setDebugLog(false);

	destroyDemoReplay();
	destroyInputRecorder();

	delete _drawContext;
	_drawContext = nullptr;

	delete _screenBitmap;
	_screenBitmap = nullptr;
	_island.clear();

	_polyglot->shutdown();

	for (int i = 0; i < kNumStatsEvents; i++)
		_statsEvents[i] = nullptr;

	_firstFrame = true;
	_lastStamp = 0.0;
	_controlSkipFrames = 0;
	_replayRequest = -1;
	_replayRequestName.clear();
	_replayMusicFrames = 0;
	_demoIndex = -1;
	_demoCount = 0;
}

} // End of namespace Grim
