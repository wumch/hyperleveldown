
#pragma once

#include "./assist.h"
#include <cstdlib>
#include <sstream>
#include <v8.h>
#include <options.h>
#include <db.h>
#include "./db.h"

namespace leveldb {

#if defined(__FRANK_HYPERLEVELDB_FILL_OPTIONS) or 								\
	defined(__FRANK_HYPERLEVELDB_FILL_OPTIONS_INTEGER) or 						\
	defined(__FRANK_HYPERLEVELDB_FILL_OPTIONS_BOOLEAN)
#	error "marco __FRANK_HYPERLEVELDB_FILL_OPTIONS already exists!"
#else
#	define __FRANK_FILL_OPTIONS_INTEGER(name, key_name, opts, options) 			\
	{																			\
		v8::Local<v8::String> key = v8::String::New(key_name);					\
		if (opts->Has(key))														\
		{																		\
			options.name = opts->Get(key)->ToInt32()->Value();					\
		}																		\
	}

#	define __FRANK_FILL_OPTIONS_BOOLEAN(name, key_name, opts, options) 			\
	{																			\
		v8::Local<v8::String> key = v8::String::New(key_name);					\
		if (opts->Has(key))														\
		{																		\
			options.name = opts->Get(key)->IsTrue();							\
		}																		\
	}

void HyperLevelDB::fill_open_options(v8::Handle<v8::Object>& opts_from, leveldb::Options& opts_to)
{
	{
		v8::Local<v8::String> key = v8::String::New("cacheSize");
		if (opts_from->Has(key))
		{
			int64_t block_cache_size = opts_from->Get(key)->ToInteger()->Value();
			if (block_cache_size > 0)
			{
				cache = static_cast<leveldb::Cache*>(std::malloc(block_cache_size));
				if (CS_BUNLIKELY(!cache))
				{
					std::stringstream stream;
					stream << "failed to alloc " << block_cache_size << " bytes block-cache";
					raise_err(stream.str());
				}
				opts_to.block_cache = cache;
			}
		}
	}

	{
		v8::Local<v8::String> key = v8::String::New("compression");
		if (opts_from->Has(key))
		{
			opts_to.compression = opts_from->Get(key)->IsTrue() ?
					leveldb::kSnappyCompression : leveldb::kNoCompression;
		}
	}

	__FRANK_FILL_OPTIONS_BOOLEAN(create_if_missing, "createIfMissing", opts_from, opts_to)
	__FRANK_FILL_OPTIONS_BOOLEAN(error_if_exists, "errorIfExists", opts_from, opts_to)

	__FRANK_FILL_OPTIONS_INTEGER(write_buffer_size, "writeBufferSize", opts_from, opts_to)
	__FRANK_FILL_OPTIONS_INTEGER(block_size, "blockSize", opts_from, opts_to)
	__FRANK_FILL_OPTIONS_INTEGER(max_open_files, "maxOpenFiles", opts_from, opts_to)
	__FRANK_FILL_OPTIONS_INTEGER(block_restart_interval, "blockRestartInterval", opts_from, opts_to)
}
#	undef __FRANK_HYPERLEVELDB_FILL_OPTIONS
#endif

void HyperLevelDB::init_default_open_options(leveldb::Options& options)
{
	options.create_if_missing = true;
	options.error_if_exists = false;
	options.compression = leveldb::kSnappyCompression;
	options.block_cache = NULL;

	options.write_buffer_size = 4 << 20;
	options.block_size = 4 << 10;
	options.max_open_files = 1000;
	options.block_restart_interval = 16;
}

HyperLevelDB::HyperLevelDB(const std::string& directory_)
	: directory(directory_), db(NULL), cache(NULL)
{}

void HyperLevelDB::fill_write_options(const v8::Handle<v8::Object>& opts_from, leveldb::WriteOptions& opts_to) const
{
	if (opts_from->Has(write_option_sync))
	{
		opts_to.sync = opts_from->Get(write_option_sync)->IsTrue();
	}
}

bool HyperLevelDB::fill_read_options(const v8::Handle<v8::Object>& opts_from, leveldb::ReadOptions& opts_to, bool fill_cache_default) const
{
	if (opts_from->Has(read_option_verify_checksums))
	{
		opts_to.verify_checksums = opts_from->Get(read_option_verify_checksums)->IsTrue();
	}
	opts_to.fill_cache = opts_from->Has(read_option_fill_cache) ? opts_from->Get(read_option_fill_cache)->IsTrue() : fill_cache_default;
	return !(opts_from->Has(read_option_as_buffer) && opts_from->Get(read_option_as_buffer)->IsFalse());
}

#ifdef __FRANK_FILL_ITER_OPTION_BOOLEAN
#	error "macro __FRANK_FILL_ITER_OPTION_BOOLEAN already exists!"
#else
#	define __FRANK_FILL_ITER_OPTION_BOOLEAN(opts_from, opts_to, option)					\
	{																					\
		if (opts_from->Has(iter_option_ ## option))										\
		{																				\
			iter_options.option = opts_from->Get(iter_option_ ## option)->IsTrue();		\
		}																				\
	}

void HyperLevelDB::fill_iter_options(const v8::Handle<v8::Object>& opts_from, IterOptions& iter_options)
{
	{
		if (opts_from->Has(iter_option_start))
		{
			v8::String::AsciiValue data(opts_from->Get(iter_option_start)->ToString());
			iter_options.start = std::string(*data, data.length());
		}
	}
	{
		if (opts_from->Has(iter_option_end))
		{
			v8::String::AsciiValue data(opts_from->Get(iter_option_end)->ToString());
			iter_options.end = std::string(*data, data.length());
		}
	}
	{
		if (opts_from->Has(iter_option_limit))
		{
			iter_options.limit = opts_from->Get(iter_option_limit)->ToInteger()->Value();
		}
	}
	__FRANK_FILL_ITER_OPTION_BOOLEAN(opts_from, iter_options, reverse);
	__FRANK_FILL_ITER_OPTION_BOOLEAN(opts_from, iter_options, keys);
	__FRANK_FILL_ITER_OPTION_BOOLEAN(opts_from, iter_options, values);
	__FRANK_FILL_ITER_OPTION_BOOLEAN(opts_from, iter_options, key_as_buffer);
	__FRANK_FILL_ITER_OPTION_BOOLEAN(opts_from, iter_options, value_as_buffer);
}
#	undef __FRANK_FILL_ITER_OPTION_BOOLEAN
#endif

void HyperLevelDB::fill_iter_settings(const v8::Handle<v8::Object>& opts_from, leveldb::ReadOptions& read_options, IterOptions& iter_options)
{
	fill_read_options(opts_from, read_options);
	fill_iter_options(opts_from, iter_options);
}

}
