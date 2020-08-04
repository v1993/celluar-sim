#include <iostream>
#include <sstream>
#include <unordered_set>

#include "Global.hpp"
#include "Cell.hpp"
#include "SdlUtils.hpp"
#include <SDL_main.h>
#include <SDL2_framerate.h>
#include <SDL_assert.h>

#ifdef WITH_OPENMP
#include <omp.h>
#endif

GlobalSettingsType global;
GlobalFieldType field;

[[noreturn]] static void PrintUsageAndExit([[maybe_unused]] int argc, char* argv[]) {
	SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s WIDTH HEIGHT", argv[0]);
	exit(EXIT_FAILURE);
};

Uint32 my_callbackfunc([[maybe_unused]] Uint32 interval, [[maybe_unused]] void *param) {
	SDL_Event event;
	SDL_UserEvent userevent;

	/* In this example, our callback pushes a function
	into the queue, and causes our callback to be called again at the
	same interval: */

	userevent.type = SDL_USEREVENT;
	userevent.code = 0;
	// One might want to set `userevent.data1` or `userevent.data2`

	event.type = SDL_USEREVENT;
	event.user = userevent;

	SDL_PushEvent(&event);
	return 1000;
}

int main(int argc, char* argv[]) {
	// TODO: use option handling library
	if (argc != 3) PrintUsageAndExit(argc, argv);

	{
		std::istringstream stream1 {argv[1]}, stream2 {argv[2]};
		stream1 >> global.fieldW;
		stream2 >> global.fieldH;
		if (!stream1 or !stream2) PrintUsageAndExit(argc, argv);
	}

	try {
		// Init things
		field.lightMap = std::make_unique<uint8_t[]> (global.fieldH * global.fieldW);

		atexit(SDL_Quit);
		if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)) throw SdlError();
		auto window = sdl_resource(SDL_CreateWindow, SDL_DestroyWindow,
								   "Celluar simulator",
								   SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
								   800, 600, SDL_WINDOW_RESIZABLE
								  );

		// TODO: add proper FPS controls
		auto windowRenderer = sdl_resource(SDL_CreateRenderer, SDL_DestroyRenderer,
										   window.get(), -1,
										   //SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
										   SDL_RENDERER_TARGETTEXTURE
										  );

		// Texture we're rendering to. 1 field is exactly 1 pixel. Locking is used to send data.
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "nearest", SDL_HINT_OVERRIDE);
		auto renderTexture = sdl_resource(SDL_CreateTexture, SDL_DestroyTexture,
										  windowRenderer.get(), SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, global.fieldW, global.fieldH
										 );

		// "Midway" texture that's used to render with better quality
		// It gets recreated when `scaleFactor` is changed
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "linear", SDL_HINT_OVERRIDE);
		size_t scaleFactor;
		auto updateScaleFactor = [&scaleFactor, &windowRenderer]() {
			int w, h;
			if (SDL_GetRendererOutputSize(windowRenderer.get(), &w, &h)) throw SdlError();
			scaleFactor = std::min(
							  1 + ((w - 1) / global.fieldW),
							  1 + ((h - 1) / global.fieldH)
						  );
		};
		updateScaleFactor();
		auto scaleTexture = sdl_resource(SDL_CreateTexture, SDL_DestroyTexture,
										 windowRenderer.get(), SDL_GetWindowPixelFormat(window.get()), SDL_TEXTUREACCESS_TARGET,
										 global.fieldW * scaleFactor, global.fieldH * scaleFactor
										);

		SDL_RenderSetLogicalSize(windowRenderer.get(), global.fieldW * scaleFactor, global.fieldH * scaleFactor);

		// Setup random number generators, one per thread
#ifdef WITH_OPENMP
		size_t threads = omp_get_max_threads();
		auto rngs = std::make_unique<randomGenerator[]>(threads);
		#pragma omp parallel for
		for (size_t i = 0; i < threads; ++i) {
			std::random_device rng_dev;
			rngs[i].seed(rng_dev());
		};
#else
		randomGenerator rng;
		{
			std::random_device rng_dev;
			rng.seed(rng_dev());
		}
#endif

		// Setup FPS manager
		FPSmanager fps;
		SDL_initFramerate(&fps);
		SDL_setFramerate(&fps, 60);
		size_t frame_count = 0, fps_frame_count = 0;

		// Various variables that affect how simulation is working
		// TODO: make those changeble in runtime
		size_t mutationRate = 10;

		// Those vectors get reused a lot, so don't create them every loop
		std::vector<std::pair<Point, std::shared_ptr<Cell>>> moves;
		std::vector<std::pair<Point, CellActionRequest*>> energyts;
		std::vector<std::pair<Point, CellActionRequest*>> eats;

		std::vector<Point> divisions;
		std::vector<Point> todie;

		// We're all set, let's go!
		bool working = true;
		auto fpsTime = SDL_GetTicks();
		SDL_AddTimer(1000, my_callbackfunc, nullptr);
#ifdef WITH_OPENMP
		#pragma omp parallel
		try {
			// Style guide: tasks and taskloops must use default(none) to strictly control variable access
			// Note: must not be used in tasks. Each must get an individual copy instead.
			auto& rng = rngs[omp_get_thread_num()];
#endif
			while (working) {
				// Input events handling
#ifdef WITH_OPENMP
				#pragma omp master
#endif
				{
					SDL_Event e;
					while (SDL_PollEvent(&e)) {
						switch (e.type) {
						case SDL_QUIT:
							working = false;
							break;
						case SDL_KEYDOWN:
							if (!e.key.repeat)
								switch (e.key.keysym.scancode) {
								case SDL_SCANCODE_A: {
									std::cout << "Here, have some cells!" << std::endl;
									std::uniform_int_distribution<size_t> wDist(0, global.fieldW - 1);
									std::uniform_int_distribution<size_t> hDist(0, global.fieldH - 1);
									for (size_t i = 0; i < 10; ++i) {
										field.cells.insert_or_assign(Point(hDist(rng), wDist(rng)), std::make_shared<Cell>());
									}
									break;
								}
								case SDL_SCANCODE_KP_PLUS: {
									mutationRate += 5;
									std::cout << "Mutation rate: " << mutationRate << std::endl;
									break;
								}
								case SDL_SCANCODE_KP_MINUS: {
									if(mutationRate >= 5) mutationRate -= 5;
									std::cout << "Mutation rate: " << mutationRate << std::endl;
									break;
								}
								default:
									break;
								}
						case SDL_WINDOWEVENT:
							if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
								size_t oldScale = scaleFactor;
								updateScaleFactor();
								if (oldScale != scaleFactor) {
									scaleTexture = sdl_resource(SDL_CreateTexture, SDL_DestroyTexture,
																windowRenderer.get(), SDL_GetWindowPixelFormat(window.get()), SDL_TEXTUREACCESS_TARGET,
																global.fieldW * scaleFactor, global.fieldH * scaleFactor
															   );
									SDL_RenderSetLogicalSize(windowRenderer.get(), global.fieldW * scaleFactor, global.fieldH * scaleFactor);
								}
							}
							break;
						case SDL_USEREVENT:
							auto timeNow = SDL_GetTicks();
							std::cout << "FPS: " << double(fps_frame_count * 1000) / double(timeNow - fpsTime) << "\n";
							fpsTime = timeNow;
							fps_frame_count = 0;
						};
					};
				}
#ifdef WITH_OPENMP
				#pragma omp barrier
#endif
				if (!working) break;

				// Round 1 of calculations: poll cells for actions

#ifdef WITH_OPENMP
				#pragma omp sections
#endif
				{
#ifdef WITH_OPENMP
					#pragma omp section
#endif
					moves.clear();
#ifdef WITH_OPENMP
					#pragma omp section
#endif
					energyts.clear();
#ifdef WITH_OPENMP
					#pragma omp section
#endif
					eats.clear();
				}

#ifdef WITH_OPENMP
				#pragma omp single
#endif
				{
					for (auto& pair : field.cells) {
#ifdef WITH_OPENMP
						#pragma omp task firstprivate(pair) shared(moves, energyts, eats) default(none)
#endif
						{
							auto res = pair.second->advanceBegin(pair.first);
							if (res) {
								switch (res->type) {
								case CellActionRequestType::MOVE:
#ifdef WITH_OPENMP
									#pragma omp critical(moves)
#endif
									moves.emplace_back(pair.first, pair.second);
									break;
								case CellActionRequestType::ENERGY:
#ifdef WITH_OPENMP
									#pragma omp critical(energyts)
#endif
									energyts.emplace_back(pair.first, res);
									break;
								case CellActionRequestType::EAT:
#ifdef WITH_OPENMP
									#pragma omp critical(eats)
#endif
									eats.emplace_back(pair.first, res);
									break;

								case CellActionRequestType::NONE:
									abort(); // Something is wrong
									break;
								}
							}
						}
					}
#ifdef WITH_OPENMP
					#pragma omp taskwait
#endif
				}

#ifdef WITH_OPENMP
				#pragma omp single
#endif
				{
					// Round two: handle energy transfers, eating and movement
					// This includes changing state of cells, so we do this single-threaded (sadly)
					// TODO: shuffling vectors first might be a good idea

					// Energy transfers don't invalidate anything
					for (auto& req : energyts) {
						const auto second = req.second;
						req.first.checkBounds();
						req.first.apply(second->dir);
						field.cells.at(req.first)->addEnergy(second->num);
						second->res = 1;
					}

					// Eating requests might destroy source or target cells, so check for them first
					bool mapTouched = false;
					for (auto& req : eats) {
						req.first.checkBounds();
						// Are we still there?
						if (field.cells.count(req.first)) {
							auto& eater = field.cells.at(req.first);
							const auto second = mapTouched ? eater->getActionPtr() : req.second;;
							if (!req.first.apply(second->dir)) abort();

							// Is our eating target still there?
							if (field.cells.count(req.first)) {
								// It is. Good
								auto& prey = field.cells.at(req.first);
								bool canEat = false;
								
								auto getPotential = [](decltype(eater)& obj) {
									return obj->getEnergy() + obj->getPower();
								};
								
								auto eaterPotential = getPotential(eater);
								auto preyPotential = getPotential(prey);

								if (eaterPotential < preyPotential) {
									uint8_t diff = preyPotential - eaterPotential;

									std::uniform_int_distribution<uint8_t> dist(0, diff);
									// This affects how useful eating is in general
									canEat = dist(rng) < 25;
								} else {
									canEat = true;
								}
								second->res = canEat;

								if (canEat) {
									// We ate 'em!
									// Add from half to all of their energy to us and erase them
									//std::cout << "Om nom nom\n";
									std::uniform_int_distribution<uint8_t> dist(prey->getEnergy() / 2, prey->getEnergy());
									eater->addEnergy(dist(rng));
									field.cells.erase(req.first);
									mapTouched = true;
								}
							} else {
								second->res = 0;
							}
							// No outer else needed, the eater is gone by now
						}
					}

					// Now movement requests
					// Check both for existance of asker and possiblity of request
					// Also note that movement invalidates action pointers of moved cells, so be extra careful
					for (auto& reqPair : moves) {
						// Are we still there? Is that still really us?
						auto& cell = reqPair.second;
						if (field.cells.count(reqPair.first) and field.cells.at(reqPair.first) == cell) {
							auto& cell = field.cells.at(reqPair.first);
							const auto req = cell->getActionPtr();
							const auto origPos = reqPair.first;
							if (!reqPair.first.apply(req->dir)) abort();
							// Ensure that target space is empty
							if (!field.cells.count(reqPair.first)) {
								req->res = 1;
								reqPair.first.checkBounds();
								mapTouched = true;
								field.cells.insert({reqPair.first, std::move(cell)});
								field.cells.erase(origPos);
							} else {
								req->res = 0;
							}
						}
					}
				}
				// Implicit OpenMP barrier

				// Calculate lighting
				// Note: we might render blue component to texture as the same time
				// It could increase performance, but how much?..
				uint8_t maxLight;
				{
					size_t daytime = frame_count % 256;
					if (daytime < 128) maxLight = 255 - daytime;
					else maxLight = daytime;
				}
#ifdef WITH_OPENMP
#if defined(__GNUC__) && (__GNUC__ < 10)
				#pragma omp for nowait
#else
				#pragma omp for nowait order(concurrent)
#endif
#endif
				for (size_t x = 0; x < global.fieldW; ++x) {
					size_t lightLevel = maxLight;

					for (size_t y = 0; y < global.fieldH; ++y) {
						field.lightMap[x * global.fieldH + y] = lightLevel;
						// TODO: make shadow proportional to cell's power
						std::uniform_int_distribution distr(0, 1);
						size_t change = (field.cells.count({y, x}) ? 6 : 3) + distr(rng);
						lightLevel = (change < lightLevel) ? lightLevel - change : 0;
					};
				}

				{
					// Now finish calculations in cells
#ifdef WITH_OPENMP
					#pragma omp sections
#endif
					{
#ifdef WITH_OPENMP
						#pragma omp section
#endif
						divisions.clear();
#ifdef WITH_OPENMP
						#pragma omp section
#endif
						todie.clear();
					}

#ifdef WITH_OPENMP
					#pragma omp single
#endif
					{
						for (auto& pair : field.cells) {
#ifdef WITH_OPENMP
							#pragma omp task firstprivate(pair) shared(divisions, todie, rngs) default(none)
#endif
							{
								// It is required to explicitly request instance when using tasks
#ifdef WITH_OPENMP
								auto& rng = rngs[omp_get_thread_num()];
#endif
								auto res = pair.second->advanceEnd(pair.first, rng);
								switch (res) {
								case EndMoveAction::DIVIDE:
#ifdef WITH_OPENMP
									#pragma omp critical(divisions)
#endif
									divisions.push_back(pair.first);
									break;
								case EndMoveAction::DIE:
#ifdef WITH_OPENMP
									#pragma omp critical(todie)
#endif
									todie.push_back(pair.first);
									break;
								case EndMoveAction::NONE:
									break;
								};
							}
						}
#ifdef WITH_OPENMP
						#pragma omp taskwait
#endif
					}

#ifdef WITH_OPENMP
					#pragma omp single
#endif
					{
						// Now, in (sadly) single-threaded mode, handle ensuring deaths and divisions
						for (auto& pos : todie) {
							// It's an easy one
							field.cells.erase(pos);
						}
						std::uniform_int_distribution<size_t> mutDist(0, mutationRate);
						std::array<uint8_t, DirectionMax> possibleDirs;
						for (auto& pos : divisions) {
							// Divisions are tricky
							auto& parent = field.cells.at(pos);
							// Here we build a vector of possible division directions
							// Note: in theory, this loop can be ran in parallel. But how?.. And is it worth it?..
							size_t possibleCnt = 0;
							for (uint8_t dir = 0; dir < DirectionMax; ++dir) {
								// If it's a valid cell
								if (auto npos = pos.applyNew(Direction(dir))) {
									// and it's empty
									if (!field.cells.count(*npos)) {
										possibleDirs[possibleCnt++] = dir;
									}
								}
							}
							// If we can't divide, we silently die :(
							if (possibleCnt == 0) {
								field.cells.erase(pos);
								continue;
							}

							// Now, select random direction to divide into and do it!
							std::uniform_int_distribution<uint8_t> dist(0, possibleCnt - 1);
							Direction divDir {possibleDirs[dist(rng)]};
							auto newPos = *pos.applyNew(divDir);
							newPos.checkBounds();
							auto newCell = parent->fork();
							newCell->mutate(mutDist(rng), rng);
							field.cells.insert({newPos, std::move(newCell)});
						}
					}
					// Implicit barrier
				}

				// Rendering
#ifdef WITH_OPENMP
				#pragma omp master
#endif
				{
					uint8_t* pixels;
					int pitch;
					SDL_LockTexture(renderTexture.get(), nullptr, (void**)&pixels, &pitch);
					// Assert below may fail even in properly working case!
					//SDL_assert(global.fieldW * 3 == pitch);
#ifdef WITH_OPENMP
					#pragma omp taskloop simd collapse(2) shared(pixels, pitch, global, field) default(none)
#endif
					for (size_t y = 0; y < global.fieldH; ++y) {
						for (size_t x = 0; x < global.fieldW; ++x) {
							size_t pixidx = pitch * y + x * 3;
							
							if (field.cells.count({y, x})) {
								// RED - power
								// GREEN - energy
								auto& cell = field.cells.at({y, x});
								pixels[pixidx]		= cell->getPower();
								pixels[pixidx + 1]	= cell->getEnergy();
							} else {
								pixels[pixidx]		= 0;
								pixels[pixidx + 1]	= 0;
							}

							// BLUE - lighting level
							pixels[pixidx + 2]	= field.lightMap[x * global.fieldH + y];
						}
					}
#ifdef WITH_OPENMP
					#pragma omp taskwait
#endif
					SDL_UnlockTexture(renderTexture.get());

					// Upscale our texture using integer NN scaling
					SDL_SetRenderTarget(windowRenderer.get(), scaleTexture.get());
					SDL_RenderCopy(windowRenderer.get(), renderTexture.get(), nullptr, nullptr);

					// Now display it with linear downscaling
					SDL_SetRenderTarget(windowRenderer.get(), nullptr);
					SDL_RenderCopy(windowRenderer.get(), scaleTexture.get(), nullptr, nullptr);

					SDL_RenderPresent(windowRenderer.get());

					// Insert FPS-driven delay
					//SDL_framerateDelay(&fps);
					++frame_count;
					++fps_frame_count;
				}
			}
#ifdef WITH_OPENMP
		} catch (const std::exception& e) {
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "(in threaded loop) %s", e.what());
			exit(1);
		} catch (...) {
			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "unknown error in threaded loop");
			exit(2);
		}
#endif
	} catch (const std::exception& e) {
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", e.what());
		return 1;
	} catch (...) {
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "unknown error");
		return 2;
	}
}
