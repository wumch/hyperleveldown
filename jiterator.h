
#pragma once

#include "./assist.h"
#include <string>
#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <iterator.h>
#include "jstatus.h"

namespace leveldb {

class IterOptions
{
public:
	static const int64_t no_limit = -1;

	std::string start, end;
	int64_t limit;
	bool reverse,
		keys,
		values,
		key_as_buffer,
		value_as_buffer;

	IterOptions():
		limit(no_limit),
		reverse(false), keys(true), values(true), key_as_buffer(true), value_as_buffer(true)
	{}
};

class Jiterator: public node::ObjectWrap
{
private:
	IterOptions options;

	leveldb::Iterator* iter;

	int64_t walked;

	std::string cur_key, cur_value;

	static v8::Persistent<v8::Function> jsctor;

public:
	Jiterator()
		: iter(NULL), walked(0)
	{}

	static void init(v8::Handle<v8::Object> exports)
	{
		v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(js_new);
		tpl->SetClassName(v8::String::NewSymbol("Iterator"));
		tpl->InstanceTemplate()->SetInternalFieldCount(4);

		v8::Local<v8::ObjectTemplate> prototype = tpl->PrototypeTemplate();
		attach_func(prototype, "next", js_next);
		attach_func(prototype, "end", js_end);
	}

	static v8::Local<v8::Value> create(leveldb::Iterator* it, const IterOptions& iter_options)
	{
		v8::HandleScope scope;
		v8::Local<v8::Object> js_iter = jsctor->NewInstance();
		Jiterator* self = node::ObjectWrap::Unwrap<Jiterator>(js_iter);
		self->options = iter_options;
		self->iter = it;
		if (self->options.start.empty())
		{
			self->iter->SeekToFirst();
		}
		else
		{
			self->iter->Seek(leveldb::Slice(self->options.start));
		}
		return scope.Close(js_iter);
	}

	static v8::Handle<v8::Value> js_new(const v8::Arguments& args)
	{
		v8::HandleScope scope;
		Jiterator* instance = new Jiterator;
		instance->Wrap(args.This());
		return scope.Close(args.This());
	}

	static v8::Handle<v8::Value> js_next(const v8::Arguments& args)
	{
		v8::HandleScope scope;

		if (CS_BUNLIKELY(args.Length() < 1 || !args[0]->IsFunction()))
		{
			raise_typeerr("the first argument (callback) must be a Function.");
		}

		Jiterator* self = node::ObjectWrap::Unwrap<Jiterator>(args.This());

		if (CS_BLIKELY(self->iter->Valid() &&
				(self->options.limit == self->options.no_limit || self->walked < self->options.limit)))
		{
			++self->walked;
			if (self->options.reverse)
			{
				self->iter->Prev();
			}
			else
			{
				self->iter->Next();
			}
		}
		else
		{
			v8::Local<v8::Function>::Cast(args[0])->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}

		int argc = 1;
		v8::Local<v8::Value> argv[3];
		if (CS_BLIKELY(self->iter->status().ok()))
		{
			argv[0] = v8::Local<v8::Value>::New(v8::Undefined());
		}
		else
		{
			argv[0] = Jstatus::convert(self->iter->status());
		}
		if (self->options.keys)
		{
			self->cur_key = self->iter->key().ToString();
			if (self->options.key_as_buffer)
			{
				argv[argc] = v8::Local<v8::Value>::New(node::Buffer::New(self->cur_key.data(), self->cur_key.size())->handle_);
			}
			else
			{
				argv[argc] = v8::String::New(self->cur_key.data(), self->cur_key.size());
			}
			argc += 1;
		}
		if (self->options.values)
		{
			self->cur_value = self->iter->value().ToString();
			if (self->options.key_as_buffer)
			{
				argv[argc] = v8::Local<v8::Value>::New(node::Buffer::New(self->cur_value.data(), self->cur_value.size())->handle_);
			}
			else
			{
				argv[argc] = v8::String::New(self->cur_value.data(), self->cur_value.size());
			}
			argc += 1;
		}
		v8::Local<v8::Function>::Cast(args[0])->Call(v8::Context::GetCurrent()->Global(), argc, argv);

		return args.This();
	}

	static v8::Handle<v8::Value> js_end(const v8::Arguments& args)
	{
		v8::HandleScope scope;

		Jiterator* self = node::ObjectWrap::Unwrap<Jiterator>(args.This());

		delete self->iter;
		self->iter = NULL;

		if (args.Length() > 0 and args[0]->IsFunction())
		{
			v8::Local<v8::Function>::Cast(args[0])->Call(v8::Context::GetCurrent()->Global(), 0, NULL);
		}

		return scope.Close(v8::Undefined());
	}

	~Jiterator()
	{
		delete iter;
	}
};

}
