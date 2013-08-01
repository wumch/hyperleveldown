
#include "./assist.h"
#include <cstdlib>
#include <string>
#include <sstream>
#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include <db.h>
#include "./db.h"
#include "./db_impl.h"
#include "./assist.h"
#include "./jobs.h"
#include "./jstatus.h"
#include "./jiterator.h"

namespace leveldb {

void HyperLevelDB::init(v8::Handle<v8::Object> exports)
{
	v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(js_new);
	tpl->SetClassName(v8::String::NewSymbol("HyperLevelDB"));
	tpl->InstanceTemplate()->SetInternalFieldCount(4);

	v8::Local<v8::ObjectTemplate> prototype = tpl->PrototypeTemplate();
	attach_func(prototype, "open", js_open);
	attach_func(prototype, "close", js_close);
	attach_func(prototype, "put", js_put);
	attach_func(prototype, "get", js_get);
	attach_func(prototype, "del", js_del);
	attach_func(prototype, "batch", js_batch);
	attach_func(prototype, "approximateSize", js_approximate_size);
	attach_func(prototype, "getProperty", js_get_property);
	attach_func(prototype, "iterator", js_iterator);

	attach_func(prototype, "destory", js_destroy);
	attach_func(prototype, "repair", js_repair);

	v8::Persistent<v8::Function> ctor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
	exports->Set(v8::String::NewSymbol("HyperLevelDB"), ctor);
}

v8::Handle<v8::Value> HyperLevelDB::js_new(const v8::Arguments& args)
{
	v8::HandleScope scope;

	if (CS_BUNLIKELY(args.Length() < 1 || !args[0]->IsString()))
	{
		raise_typeerr("the first argument must be a string indicate the directory of db.");
	}
	std::string dir = std::string(*v8::String::Utf8Value(args[0]->ToString()));

	HyperLevelDB* hyper_leveldb = new HyperLevelDB(dir);
	hyper_leveldb->Wrap(args.This());

	return args.This();
}

v8::Handle<v8::Value> HyperLevelDB::js_open(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	self->init_default_open_options(self->open_options);

	if (CS_BUNLIKELY(args.Length() < 1))
	{
		raise_typeerr("at least 1 arguments (callback) is required");
	}

	v8::Persistent<v8::Function> callback;
	if (args.Length() == 2)
	{
		if (args[0]->IsObject())
		{
			v8::Local<v8::Object> opts_from = args[0]->ToObject();
			self->fill_open_options(opts_from, self->open_options);
			callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[1]));
		}
		else if (args[0]->IsFunction())
		{
			callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[0]));
		}
		else if (CS_BUNLIKELY(!(args[0]->IsNull() || args[0]->IsUndefined())))
		{
			raise_typeerr("the first argument must be null, undefined, obejct, and can be also omited.");
		}
	}

	OpenJob* job = new OpenJob(self->db, self->open_options, self->directory, &self->db, callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_open);

	return args.This();
}

void HyperLevelDB::on_open(uv_work_t* uv_work, int uv_status)
{
	OpenJob* job = reinterpret_cast<OpenJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
	}
	else
	{
		const uint32_t argc = 1;
		std::string err(std::string("failed to open database at ") + job->directory + job->status.ToString());
		v8::Local<v8::Value> argv[argc] = { v8::Local<v8::Value>::New(v8::Exception::Error(v8::String::New(err.c_str(), err.size()))) };
		job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
	}
	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_close(const v8::Arguments& args)
{
	v8::HandleScope scope;

	if (CS_BUNLIKELY(args.Length() < 1 || !args[0]->IsFunction()))
	{
		raise_typeerr("the first argument (callback) must be a Function");
	}
	v8::Persistent<v8::Function> callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[0]));

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	CloseJob* job = new CloseJob(self->db, self->cache, callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_close);

	return scope.Close(v8::Undefined());
}

void HyperLevelDB::on_close(uv_work_t* uv_work, int uv_status)
{
	CloseJob* job = reinterpret_cast<CloseJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
	}
	else
	{
		const uint32_t argc = 1;
		std::string err(std::string("failed on closing database: ") + job->status.ToString());
		v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
		job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
	}
	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_put(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	v8::Local<v8::Value> key, value;
	v8::Persistent<v8::Function> callback;
	leveldb::WriteOptions options;

	switch (args.Length())
	{
	case 3:
		key = args[0];
		value = args[1];
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
		break;
	case 2:
		key = args[0];
		value = args[1];
		break;
	case 0:		// intentionally go ahead.
	case 1:
		raise_typeerr("at least 2 arguments (key, value) are required.");
		break;
	default:
		key = args[0];
		value = args[1];
		self->fill_write_options(args[2]->ToObject(), options);
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
		break;
	}

	v8::String::AsciiValue key_data(key->ToString()), value_data(value->ToString());
	PutJob* job = new PutJob(self->db, options, v8::String::AsciiValue(key->ToString()), v8::String::AsciiValue(value->ToString()), callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_put);

	return args.This();
}

void HyperLevelDB::on_put(uv_work_t* uv_work, int uv_status)
{
	PutJob* job = reinterpret_cast<PutJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}

	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_get(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	v8::Local<v8::Value> key;
	v8::Persistent<v8::Function> callback;
	leveldb::ReadOptions options;
	bool as_buffer = true;

	switch (args.Length())
	{
	case 2:
		key = args[0];
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[1]));
		break;
	case 1:
		key = args[0];
		break;
	case 0:
		raise_typeerr("at least 1 argument (key) are required.");
		break;
	default:
		key = args[0];
		as_buffer = self->fill_read_options(args[1]->ToObject(), options);
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
		break;
	}

	GetJob* job = new GetJob(self->db, options, as_buffer, v8::String::AsciiValue(key->ToString()), callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_get);

	return args.This();
}

void HyperLevelDB::on_get(uv_work_t* uv_work, int uv_status)
{
	GetJob* job = reinterpret_cast<GetJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const int argc = 2;
			v8::Local<v8::Value> argv[argc];
			argv[0] = v8::Local<v8::Value>::New(v8::Null());
			if (job->as_buffer)
			{
				argv[1] = v8::Local<v8::Value>::New(node::Buffer::New(job->result.data(), job->result.size())->handle_);
			}
			else
			{
				argv[1] = v8::Local<v8::Value>::New(v8::String::New(job->result.data(), job->result.size()));
			}
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}

	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_del(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	v8::Local<v8::Value> key;
	v8::Persistent<v8::Function> callback;
	leveldb::WriteOptions options;

	switch (args.Length())
	{
	case 2:
		key = args[0];
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[1]));
		break;
	case 1:
		key = args[0];
		break;
	case 0:
		raise_typeerr("at least 1 argument (key) are required.");
		break;
	default:
		key = args[0];
		self->fill_write_options(args[1]->ToObject(), options);
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
		break;
	}

	DelJob* job = new DelJob(self->db, options, v8::String::AsciiValue(key->ToString()), callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_del);

	return args.This();
}

void HyperLevelDB::on_del(uv_work_t* uv_work, int uv_status)
{
	GetJob* job = reinterpret_cast<GetJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}

	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_batch(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	v8::Persistent<v8::Function> callback;
	leveldb::WriteOptions options;

	switch (args.Length())
	{
	case 2:
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[1]));
		break;
	case 1:
		break;
	case 0:
		raise_typeerr("at least 1 argument (operations) are required.");
		break;
	default:
		self->fill_write_options(args[1]->ToObject(), options);
		callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
		break;
	}
	if (CS_BUNLIKELY(!args[0]->IsArray()))
	{
		raise_typeerr("the first argument `operations` must be an Array");
	}

	BatchJob* job = new BatchJob(self->db, options, callback);

	v8::Local<v8::Array> operations = v8::Local<v8::Array>::Cast(args[0]);
	v8::Local<v8::Object> op;
	v8::Local<v8::Value> op_type;
	v8::Local<v8::Value> key;
	for (uint32_t i = 0; i < operations->Length(); ++i)
	{
		op = operations->Get(v8::Uint32::New(i))->ToObject();
		if (CS_BLIKELY(op->Has(batch_operation_type) && op->Has(batch_operation_key)))
		{
			op_type = op->Get(batch_operation_type);
			key = op->Get(batch_operation_key);
			if (op_type->Equals(batch_operation_put))
			{
				if (CS_BUNLIKELY(!op->Has(batch_operation_value)))
				{
					raise_typeerr("`value` is required for `put` operation.");
				}
				job->append_put(v8::String::AsciiValue(key->ToString()), v8::String::AsciiValue(op->Get(batch_operation_value)->ToString()));
			}
			else if (op_type->Equals(batch_operation_del))
			{
				job->append_del(v8::String::AsciiValue(key->ToString()));
			}
			else
			{
				raise_typeerr("batch operation supports only `put` and `del`.");
			}
		}
		else
		{
			raise_typeerr("at least `type` and `key` are required for batch operation.");
		}
	}

	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_batch);

	return args.This();
}

void HyperLevelDB::on_batch(uv_work_t* uv_work, int uv_status)
{
	GetJob* job = reinterpret_cast<GetJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}
	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_approximate_size(const v8::Arguments& args)
{
	v8::HandleScope scope;

	if (args.Length() < 3)
	{
		raise_typeerr("at least 3 arguments (key_start, key_end, callback) are required.");
	}

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	v8::Persistent<v8::Function> callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
	ApproximateSizeJob* job = new ApproximateSizeJob(self->db, v8::String::AsciiValue(args[0]->ToString()), v8::String::AsciiValue(args[1]->ToString()), callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_approximate_size);

	return args.This();
}

void HyperLevelDB::on_approximate_size(uv_work_t* uv_work, int uv_status)
{
	ApproximateSizeJob* job = reinterpret_cast<ApproximateSizeJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			v8::Local<v8::Value> argv[argc] = { v8::Uint32::New(job->size) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}
	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_get_property(const v8::Arguments& args)
{
	v8::HandleScope scope;

	if (args.Length() < 1 || !args[0]->IsString())
	{
		raise_typeerr("the first argument (property_name) must be a string.");
	}

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	v8::String::AsciiValue name(args[0]);
	std::string value;
	if (self->db->GetProperty(leveldb::Slice(*name, name.length()), &value))
	{
		return scope.Close(v8::String::New(value.data(), value.size()));
	}
	else
	{
		return scope.Close(v8::Undefined());
	}
}

v8::Handle<v8::Value> HyperLevelDB::js_iterator(const v8::Arguments& args)
{
	v8::HandleScope scope;
	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());
	leveldb::ReadOptions read_options;
	IterOptions iter_options;
	if (args.Length() > 0)
	{
		self->fill_iter_settings(args[0]->ToObject(), read_options, iter_options);
	}
	else
	{
		read_options.fill_cache = false;	// defaults not to fill cache.
	}
	return scope.Close(Jiterator::create(self->db->NewIterator(read_options), iter_options));
}

v8::Handle<v8::Value> HyperLevelDB::js_repair(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	leveldb::Options options;
	self->init_default_open_options(options);

	if (CS_BUNLIKELY(args.Length() < 2))
	{
		raise_typeerr("at least 2 arguments (location, callback) are required.");
	}
	else if (args.Length() > 2)
	{
		v8::Local<v8::Object> opts_from = args[2]->ToObject();
		self->fill_open_options(opts_from, options);
	}

	v8::Persistent<v8::Function> callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
	RepairJob* job = new RepairJob(self->db, v8::String::AsciiValue(args[0]->ToString()), options, callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_approximate_size);

	return args.This();
}

void HyperLevelDB::on_repair(uv_work_t* uv_work, int uv_status)
{
	RepairJob* job = reinterpret_cast<RepairJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}
	delete job;
}

v8::Handle<v8::Value> HyperLevelDB::js_destroy(const v8::Arguments& args)
{
	v8::HandleScope scope;

	HyperLevelDB* self = node::ObjectWrap::Unwrap<HyperLevelDB>(args.This());

	leveldb::Options options;
	self->init_default_open_options(options);

	if (CS_BUNLIKELY(args.Length() < 2))
	{
		raise_typeerr("at least 2 arguments (location, callback) are required.");
	}
	else if (args.Length() > 2)
	{
		v8::Local<v8::Object> opts_from = args[2]->ToObject();
		self->fill_open_options(opts_from, options);
	}

	v8::Persistent<v8::Function> callback = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(args[2]));
	DestroyJob* job = new DestroyJob(self->db, v8::String::AsciiValue(args[0]->ToString()), options, callback);
	uv_queue_work(uv_default_loop(), &job->uv_work, job->execute, on_approximate_size);

	return scope.Close(v8::Undefined());
}

void HyperLevelDB::on_destroy(uv_work_t* uv_work, int uv_status)
{
	DestroyJob* job = reinterpret_cast<DestroyJob*>(uv_work->data);
	if (CS_BLIKELY(job->status.ok()))
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			job->callback->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}
	}
	else
	{
		if (CS_BLIKELY(job->callback->IsFunction()))
		{
			const uint32_t argc = 1;
			std::string err = job->status.ToString();
			v8::Local<v8::Value> argv[argc] = { Jstatus::convert(job->status) };
			job->callback->Call(v8::Context::GetCurrent()->Global(), argc, argv);
		}
	}
	delete job;
}

inline leveldb::Status HyperLevelDB::put(const leveldb::WriteOptions& options, const v8::Handle<v8::Value>& key_, const v8::Handle<v8::Value>& value_)
{
	v8::String::AsciiValue key_data(key_->ToString()), value_data(value_->ToString());
	leveldb::Slice key(*key_data, key_data.length()), value(*value_data, value_data.length());
	return db->Put(options, key, value);
}

inline leveldb::Status HyperLevelDB::get(const leveldb::ReadOptions& options, const v8::Handle<v8::Value>& key_, std::string& res)
{
	v8::String::AsciiValue key_data(key_->ToString());
	leveldb::Slice key(*key_data, key_data.length());
	return db->Get(options, key, &res);
}

inline leveldb::Status HyperLevelDB::del(const leveldb::WriteOptions& options, const v8::Handle<v8::Value>& key_)
{
	v8::String::AsciiValue key_data(key_->ToString());
	leveldb::Slice key(*key_data, key_data.length());
	return db->Delete(options, key);
}

const v8::Persistent<v8::String> HyperLevelDB::batch_operation_type = v8::Persistent<v8::String>::New(v8::String::New("type"));
const v8::Persistent<v8::String> HyperLevelDB::batch_operation_put = v8::Persistent<v8::String>::New(v8::String::New("put"));
const v8::Persistent<v8::String> HyperLevelDB::batch_operation_del = v8::Persistent<v8::String>::New(v8::String::New("del"));
const v8::Persistent<v8::String> HyperLevelDB::batch_operation_key = v8::Persistent<v8::String>::New(v8::String::New("key"));
const v8::Persistent<v8::String> HyperLevelDB::batch_operation_value = v8::Persistent<v8::String>::New(v8::String::New("value"));

const v8::Persistent<v8::String> HyperLevelDB::write_option_sync = v8::Persistent<v8::String>::New(v8::String::New("sync"));

const v8::Persistent<v8::String> HyperLevelDB::read_option_verify_checksums = v8::Persistent<v8::String>::New(v8::String::New("verifyChecksums"));
const v8::Persistent<v8::String> HyperLevelDB::read_option_fill_cache = v8::Persistent<v8::String>::New(v8::String::New("fillCache"));
const v8::Persistent<v8::String> HyperLevelDB::read_option_as_buffer = v8::Persistent<v8::String>::New(v8::String::New("asBuffer"));

const v8::Persistent<v8::String> HyperLevelDB::iter_option_start = v8::Persistent<v8::String>::New(v8::String::New("start"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_end = v8::Persistent<v8::String>::New(v8::String::New("end"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_reverse = v8::Persistent<v8::String>::New(v8::String::New("reverse"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_keys = v8::Persistent<v8::String>::New(v8::String::New("keys"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_values = v8::Persistent<v8::String>::New(v8::String::New("values"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_limit = v8::Persistent<v8::String>::New(v8::String::New("limit"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_fill_cache = v8::Persistent<v8::String>::New(v8::String::New("fillCache"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_key_as_buffer = v8::Persistent<v8::String>::New(v8::String::New("keyAsBuffer"));
const v8::Persistent<v8::String> HyperLevelDB::iter_option_value_as_buffer = v8::Persistent<v8::String>::New(v8::String::New("valueAsBuffer"));


const leveldb::Status Job::status_ok = leveldb::Status::OK();

v8::Persistent<v8::Function> Jstatus::jsctor;	// extern here to omit "jstatus.cc".

const v8::Persistent<v8::String> Jstatus::js_err_ok = v8::Persistent<v8::String>::New(v8::String::New("OK"));
const v8::Persistent<v8::String> Jstatus::js_err_prefix = v8::Persistent<v8::String>::New(v8::String::New("leveldb Error: "));
const v8::Persistent<v8::String> Jstatus::js_err_io_error = v8::Persistent<v8::String>::New(v8::String::New("IOError"));
const v8::Persistent<v8::String> Jstatus::js_err_not_found = v8::Persistent<v8::String>::New(v8::String::New("Not Found"));
const v8::Persistent<v8::String> Jstatus::js_err_corruption = v8::Persistent<v8::String>::New(v8::String::New("Corruption"));
const v8::Persistent<v8::String> Jstatus::js_err_unknown = v8::Persistent<v8::String>::New(v8::String::New("Unknown"));

v8::Persistent<v8::Function> Jiterator::jsctor;

}
