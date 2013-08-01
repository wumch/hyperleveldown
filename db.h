
#pragma once

#include "./assist.h"
#include <string>
#include <v8.h>
// Usually, header files of `node` and `hyperleveldb` are located in
// their own directory under /usr/include.
#include <node.h>
#include <db.h>
#include <status.h>
#include <options.h>
#include <cache.h>
#include <uv.h>
#include "./jiterator.h"

namespace leveldb {

class HyperLevelDB:
	public node::ObjectWrap
{
private:
	const std::string directory;

	leveldb::Options open_options;

	leveldb::DB* db;

	leveldb::Cache* cache;

public:
	static void init(v8::Handle<v8::Object> exports);

	HyperLevelDB(const std::string& directory_);

public:
	// ctor for js.
	static v8::Handle<v8::Value> js_new(const v8::Arguments& args);

	// open database.
	static v8::Handle<v8::Value> js_open(const v8::Arguments& args);
	// destroy db.
	static v8::Handle<v8::Value> js_close(const v8::Arguments& args);

	static v8::Handle<v8::Value> js_put(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_get(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_del(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_batch(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_approximate_size(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_get_property(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_iterator(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_next(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_end(const v8::Arguments& args);

	static v8::Handle<v8::Value> js_destroy(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_repair(const v8::Arguments& args);

private:
	static void on_open(uv_work_t* uv_work, int uv_status);
	static void on_close(uv_work_t* uv_work, int uv_status);

	static void on_put(uv_work_t* uv_work, int uv_status);
	static void on_get(uv_work_t* uv_work, int uv_status);
	static void on_del(uv_work_t* uv_work, int uv_status);
	static void on_batch(uv_work_t* uv_work, int uv_status);
	static void on_approximate_size(uv_work_t* uv_work, int uv_status);
	static void on_get_property(uv_work_t* uv_work, int uv_status);
	static void on_next(uv_work_t* uv_work, int uv_status);
	static void on_end(uv_work_t* uv_work, int uv_status);

	static void on_destroy(uv_work_t* uv_work, int uv_status);
	static void on_repair(uv_work_t* uv_work, int uv_status);

private:
	inline leveldb::Status put(const leveldb::WriteOptions& options, const v8::Handle<v8::Value>& key_, const v8::Handle<v8::Value>& value_);
	inline leveldb::Status get(const leveldb::ReadOptions& options, const v8::Handle<v8::Value>& key_, std::string& res);
	inline leveldb::Status del(const leveldb::WriteOptions& options, const v8::Handle<v8::Value>& key_);

protected:
	// Provide this since that not only make a default open-options diffrent from `leveldb`'s may be useful,
	// but also can provide more options that `leveldb`.
	CS_FORCE_INLINE void init_default_open_options(leveldb::Options& options);
	CS_FORCE_INLINE void fill_open_options(v8::Handle<v8::Object>& opts_from, leveldb::Options& opts_to);

	CS_FORCE_INLINE void fill_write_options(const v8::Handle<v8::Object>& opts_from, leveldb::WriteOptions& opts_to) const;

	CS_FORCE_INLINE void fill_iter_settings(const v8::Handle<v8::Object>& opts_from, leveldb::ReadOptions& read_options, IterOptions& iter_options);
	CS_FORCE_INLINE bool fill_read_options(const v8::Handle<v8::Object>& opts_from, leveldb::ReadOptions& opts_to, bool fill_cache_default = true) const;
	CS_FORCE_INLINE void fill_iter_options(const v8::Handle<v8::Object>& opts_from, IterOptions& iter_options);

private:
	static const v8::Persistent<v8::String> batch_operation_type;
	static const v8::Persistent<v8::String> batch_operation_put;
	static const v8::Persistent<v8::String> batch_operation_del;
	static const v8::Persistent<v8::String> batch_operation_key;
	static const v8::Persistent<v8::String> batch_operation_value;

	static const v8::Persistent<v8::String> write_option_sync;

	static const v8::Persistent<v8::String> read_option_verify_checksums;
	static const v8::Persistent<v8::String> read_option_fill_cache;
	static const v8::Persistent<v8::String> read_option_as_buffer;

	static const v8::Persistent<v8::String> iter_option_start;
	static const v8::Persistent<v8::String> iter_option_end;
	static const v8::Persistent<v8::String> iter_option_reverse;
	static const v8::Persistent<v8::String> iter_option_keys;
	static const v8::Persistent<v8::String> iter_option_values;
	static const v8::Persistent<v8::String> iter_option_limit;
	static const v8::Persistent<v8::String> iter_option_fill_cache;
	static const v8::Persistent<v8::String> iter_option_key_as_buffer;
	static const v8::Persistent<v8::String> iter_option_value_as_buffer;

};

}
