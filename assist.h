
#pragma once

#include <string>
#include <v8.h>
#include <status.h>
//#include "/data/fsuggest/staging/ccpp/meta.hpp"

#ifndef CS_FORCE_INLINE
#   ifdef __GNUC__
#       define CS_FORCE_INLINE __attribute__((always_inline))
#   else
#       define CS_FORCE_INLINE inline
#   endif
#endif

#if !defined(CS_LIKELY) && !defined(CS_UNLIKELY)
#	ifdef __GNUC__
#		define CS_LIKELY(...)		__builtin_expect(!!(__VA_ARGS__), 1)
#		define CS_UNLIKELY(...)		__builtin_expect(!!(__VA_ARGS__), 0)
#		define CS_BLIKELY(...)		__builtin_expect((__VA_ARGS__), true)
#		define CS_BUNLIKELY(...)	__builtin_expect((__VA_ARGS__), false)
#	else
#		define CS_LIKELY(expr)		(expr)
#		define CS_UNLIKELY(expr)	(expr)
#		define CS_BLIKELY(expr)		(expr)
#		define CS_BUNLIKELY(expr)	(expr)
#	endif
#endif

#ifndef CS_PREFETCH
#	ifdef __GNUC__
#		define CS_PREFETCH(addr,rw,clean) __builtin_prefetch(addr,rw,clean)
#	else
#		define CS_PREFETCH(addr,rw,clean)
#	endif
#endif

namespace leveldb {

template<typename AttrType>
CS_FORCE_INLINE static void attach(
		v8::Local<v8::ObjectTemplate> target,
		const char* name, const AttrType& attr)
{
	target->Set(v8::String::NewSymbol(name), attr);
}

template<typename FuncType>
CS_FORCE_INLINE static void attach_func(
	v8::Local<v8::ObjectTemplate> target,
	const char* name, FuncType func)
{
	return attach(target, name, v8::FunctionTemplate::New(func)->GetFunction());
}

CS_FORCE_INLINE static void raise_typeerr(const char* err)
{
	v8::ThrowException(v8::Exception::TypeError(v8::String::New(err)));
}

CS_FORCE_INLINE static void raise_typeerr(const std::string& err)
{
	v8::ThrowException(v8::Exception::TypeError(v8::String::New(err.data(), err.size())));
}

CS_FORCE_INLINE static void raise_err(const char* err)
{
	v8::ThrowException(v8::Exception::Error(v8::String::New(err)));
}

CS_FORCE_INLINE static void raise_err(const std::string& err)
{
	v8::ThrowException(v8::Exception::Error(v8::String::New(err.data(), err.size())));
}

CS_FORCE_INLINE static std::string jstr2str(const v8::Handle<v8::Value>& jstr)
{
	v8::String::AsciiValue data(jstr->ToString());
	return std::string(*data, data.length());
}

CS_FORCE_INLINE static std::string jsascii2str(const v8::String::AsciiValue& data)
{
	return std::string(*data, data.length());
}

}
