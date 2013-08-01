
#pragma once

#include <v8.h>
#include <node.h>
#include <status.h>
#include "assist.h"

namespace leveldb
{

class Jstatus: public node::ObjectWrap
{
private:
	leveldb::Status status;

	static v8::Persistent<v8::Function> jsctor;

	static const v8::Persistent<v8::String> js_err_ok;
	static const v8::Persistent<v8::String> js_err_prefix;
	static const v8::Persistent<v8::String> js_err_io_error;
	static const v8::Persistent<v8::String> js_err_not_found;
	static const v8::Persistent<v8::String> js_err_corruption;
	static const v8::Persistent<v8::String> js_err_unknown;

public:
	Jstatus():
		status(leveldb::Status::OK())
	{}

	explicit Jstatus(const leveldb::Status& status_)
		: status(status_)
	{}

	static v8::Local<v8::Value> convert(const leveldb::Status& status_)
	{
		v8::HandleScope scope;

		v8::Local<v8::Object> status = jsctor->NewInstance();
		Jstatus* instance = node::ObjectWrap::Unwrap<Jstatus>(status);
		instance->set_status(status_);

		return scope.Close(status);
	}

	void set_status(const leveldb::Status& status_)
	{
		status = status_;
	}

	static v8::Handle<v8::Value> js_new(const v8::Arguments& args)
	{
		v8::HandleScope scope;
		Jstatus* instance = new Jstatus;
		instance->Wrap(args.This());
		return scope.Close(args.This());
	}

	static v8::Handle<v8::Value> js_ok(const v8::Arguments& args)
	{
		return node::ObjectWrap::Unwrap<Jstatus>(args.This())->status.ok() ? v8::True() : v8::False();
	}

	static v8::Handle<v8::Value> js_is_io_error(const v8::Arguments& args)
	{
		return node::ObjectWrap::Unwrap<Jstatus>(args.This())->status.IsIOError() ? v8::True() : v8::False();
	}

	static v8::Handle<v8::Value> js_is_not_found(const v8::Arguments& args)
	{
		return node::ObjectWrap::Unwrap<Jstatus>(args.This())->status.IsNotFound() ? v8::True() : v8::False();
	}

	static v8::Handle<v8::Value> js_is_corruption(const v8::Arguments& args)
	{
		return node::ObjectWrap::Unwrap<Jstatus>(args.This())->status.IsCorruption() ? v8::True() : v8::False();
	}

	static v8::Handle<v8::Value> js_to_string(const v8::Arguments& args)
	{
		v8::HandleScope scope;
		Jstatus* self = node::ObjectWrap::Unwrap<Jstatus>(args.This());
		if (self->status.IsNotFound())
		{
			return scope.Close(v8::String::Concat(js_err_prefix, js_err_not_found));
		}
		else if (self->status.ok())
		{
			return scope.Close(js_err_ok);
		}
		else if (self->status.IsIOError())
		{
			return scope.Close(v8::String::Concat(js_err_prefix, js_err_io_error));
		}
		else if (self->status.IsCorruption())
		{
			return scope.Close(v8::String::Concat(js_err_prefix, js_err_corruption));
		}
		return scope.Close(v8::String::Concat(js_err_prefix, js_err_unknown));
	}

	static void init(v8::Handle<v8::Object> exports)
	{
		v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(js_new);
		tpl->SetClassName(v8::String::NewSymbol("Status"));
		tpl->InstanceTemplate()->SetInternalFieldCount(1);

		v8::Local<v8::ObjectTemplate> prototype = tpl->PrototypeTemplate();
		attach_func(prototype, "ok", js_ok);
		attach_func(prototype, "isIOError", js_is_io_error);
		attach_func(prototype, "isNotFound", js_is_not_found);
		attach_func(prototype, "isCorruption", js_is_corruption);
		attach_func(prototype, "toString", js_to_string);

		jsctor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
		exports->Set(v8::String::NewSymbol("Status"), jsctor);
	}
};

}
