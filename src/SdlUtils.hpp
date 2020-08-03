#pragma once

#include <stdexcept>
#include <memory>
#include <type_traits>

#include <SDL.h>

// Stores current SDL error on creation
class SdlError: public std::runtime_error {
	public:
		SdlError(): std::runtime_error(SDL_GetError()) {};
		SdlError(const SdlError& other): std::runtime_error(other) {};
		// Not sure about assignment operator here...
};

// Originally taken from: https://eb2.co/blog/2014/04/c--14-and-sdl2-managing-resources/
template<typename Creator, typename Destructor, typename... Arguments>
auto sdl_resource(Creator c, Destructor d, Arguments&&... args) {
	auto r = c(std::forward<Arguments>(args)...);
	if (!r) { throw SdlError(); }
	return std::unique_ptr<std::decay_t<decltype(*r)>, decltype(d)>(r, d);
}
