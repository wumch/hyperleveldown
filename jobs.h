
#pragma once

#include <cstdlib>
#include <vector>
#include <queue>
#include <string>
#include <db.h>
#include <options.h>
#include <status.h>
#include <write_batch.h>
#include <uv.h>
#include <v8.h>

namespace leveldb {

enum BatchOpType {Put, Del};

typedef v8::Persistent<v8::Function> Callback;

template<typename JobType>
class Execute
{
public:
	static void execute(uv_work_t* uv_work)
	{
		JobType* job = reinterpret_cast<JobType*>(uv_work->data);
		job->operate();
	}

	virtual void operate() = 0;

	virtual ~Execute() {}
};

class Job
{
protected:
	static const leveldb::Status status_ok;

public:
	uv_work_t uv_work;
	leveldb::DB* db;
	Callback callback;

	leveldb::Status status;		// operate status. Exists only if status.ok() is false.

	Job(leveldb::DB* db, Callback callback_):
		db(db), callback(callback_)
	{
		uv_work.data = this;
	}

	virtual ~Job()
	{
		callback.Dispose();
	}
};

class OpenJob: public Job, public Execute<OpenJob>
{
public:
	const leveldb::Options options;
	const std::string directory;
	leveldb::DB** db_ptr;

	OpenJob(leveldb::DB* db, const leveldb::Options& options_, const std::string& directory_, leveldb::DB** db_ptr, Callback callback_):
		Job(db, callback_), options(options_), directory(directory_), db_ptr(db_ptr)
	{}

	virtual void operate()
	{
		status = leveldb::DB::Open(options, directory, db_ptr);
	}
};

class CloseJob: public Job, public Execute<CloseJob>
{
public:
	leveldb::Cache* cache;

	CloseJob(leveldb::DB* db, leveldb::Cache* cache_, Callback callback_):
		Job(db, callback_), cache(cache_)
	{}

	virtual void operate()
	{
		delete db;
		if (cache)
		{
			std::free(cache);
		}
	}
};

class PutJob: public Job, public Execute<PutJob>
{
public:
	const leveldb::WriteOptions options;
	const std::string key, value;

	PutJob(leveldb::DB* db, const leveldb::WriteOptions& options_, const std::string& key_, const std::string& value_, Callback callback_):
		Job(db, callback_), options(options_), key(key_), value(value_)
	{}

	PutJob(leveldb::DB* db, const leveldb::WriteOptions& options_,
			const v8::String::AsciiValue& key_data, const v8::String::AsciiValue& value_data, Callback callback_):
		Job(db, callback_), options(options_), key(*key_data, key_data.length()), value(*value_data, value_data.length())
	{}

	virtual void operate()
	{
		status = db->Put(options, leveldb::Slice(key), leveldb::Slice(value));
	}
};

class DelJob: public Job, public Execute<DelJob>
{
public:
	const leveldb::WriteOptions options;
	const std::string key;

	DelJob(leveldb::DB* db, const leveldb::WriteOptions& options_, const std::string& key_, Callback callback_):
		Job(db, callback_), options(options_), key(key_)
	{}

	DelJob(leveldb::DB* db, const leveldb::WriteOptions& options_, const v8::String::AsciiValue& key_data, Callback callback_):
		Job(db, callback_), options(options_), key(*key_data, key_data.length())
	{}

	virtual void operate()
	{
		status = db->Delete(options, leveldb::Slice(key));
	}
};

class GetJob: public Job, public Execute<GetJob>
{
public:
	const leveldb::ReadOptions options;
	const std::string key;
	std::string result;
	const bool as_buffer;

	GetJob(leveldb::DB* db, const leveldb::ReadOptions& options_, bool as_buffer_, const std::string& key_, Callback callback_):
		Job(db, callback_), options(options_), key(key_), as_buffer(as_buffer_)
	{}

	GetJob(leveldb::DB* db, const leveldb::ReadOptions& options_, bool as_buffer_, const v8::String::AsciiValue& key_data, Callback callback_):
		Job(db, callback_), options(options_), key(*key_data, key_data.length()), as_buffer(as_buffer_)
	{}

	virtual void operate()
	{
		status = db->Get(options, leveldb::Slice(key), &result);
	}
};

class BatchJob: public Job, public Execute<BatchJob>
{
public:
	class BatchWork
	{
	public:
		virtual ~BatchWork() {}
	};

	class BatchWorkPut: public BatchWork
	{
	public:
		std::string key, value;

		BatchWorkPut(const std::string& key_, const std::string& value_)
			: key(key_), value(value_)
		{}
	};

	class BatchWorkDel: public BatchWork
	{
	public:
		std::string key;

		BatchWorkDel(const std::string& key_)
			: key(key_)
		{}
	};

	class BatchOp
	{
	public:
		BatchOpType type;
		BatchWork* const work;

		BatchOp(BatchOpType type_, BatchWork* work_)
			: type(type_), work(work_)
		{}

		~BatchOp()
		{
			delete work;
		}
	};

	typedef std::vector<BatchOp*> BatchOpList;

	const leveldb::WriteOptions options;
	BatchOpList oplist;

	BatchJob(leveldb::DB* db, const leveldb::WriteOptions& options_, Callback callback_):
		Job(db, callback_), options(options_)
	{}

	void append_put(const std::string& key_, const std::string& value_)
	{
		oplist.push_back(new BatchOp(Put, new BatchWorkPut(key_, value_)));
	}

	void append_put(const v8::String::AsciiValue& key_data, const v8::String::AsciiValue& value_data)
	{
		append_put(std::string(*key_data, key_data.length()), std::string(*value_data, value_data.length()));
	}

	void append_del(const std::string& key_)
	{
		oplist.push_back(new BatchOp(Del, new BatchWorkDel(key_)));
	}

	void append_del(const v8::String::AsciiValue& key_data)
	{
		append_del(std::string(*key_data, key_data.length()));
	}

	virtual void operate()
	{
		if (!oplist.empty())
		{
			leveldb::WriteBatch batch;
			for (BatchOpList::iterator it = oplist.begin(); it != oplist.end(); ++it)
			{
				if ((*it)->type == Put)
				{
					BatchWorkPut* work = (BatchWorkPut*)((*it)->work);
					batch.Put(leveldb::Slice(work->key), leveldb::Slice(work->value));
				}
				else if ((*it)->type == Del)
				{
					BatchWorkDel* work = (BatchWorkDel*)((*it)->work);
					batch.Delete(leveldb::Slice(work->key));
				}
			}
			status = db->Write(options, &batch);
		}
		else
		{
			status = status_ok;
		}
	}

	virtual ~BatchJob()
	{
		for (BatchOpList::iterator it = oplist.begin(); it != oplist.end(); ++it)
		{
			delete *it;
		}
	}
};

class ApproximateSizeJob: public Job, public Execute<ApproximateSizeJob>
{
public:
	const std::string start, end;
	leveldb::Range range;
	uint64_t size;

	ApproximateSizeJob(leveldb::DB* db, const std::string& key_start, const std::string& key_end, Callback callback_):
		Job(db, callback_), start(key_start), end(key_end), range(start, end)
	{}

	ApproximateSizeJob(leveldb::DB* db, const v8::String::AsciiValue& key_start_data, const v8::String::AsciiValue& key_end_data, Callback callback_):
		Job(db, callback_), start(*key_start_data, key_start_data.length()), end(*key_end_data, key_end_data.length()), range(start, end)
	{}

	virtual void operate()
	{
		// so `status` will always be OK.
		db->GetApproximateSizes(&range, 1, &size);
	}
};

class RepairJob: public Job, public Execute<RepairJob>
{
public:
	const std::string location;
	const leveldb::Options options;

	RepairJob(leveldb::DB* db, const std::string& location_, const leveldb::Options& options, Callback callback_):
		Job(db, callback_), location(location_)
	{}

	RepairJob(leveldb::DB* db, const v8::String::AsciiValue& location_data, const leveldb::Options& options_, Callback callback_):
		Job(db, callback_), location(*location_data, location_data.length()), options(options_)
	{}

	virtual void operate()
	{
		status = leveldb::RepairDB(location, options);
	}
};

class DestroyJob: public Job, public Execute<DestroyJob>
{
public:
	const std::string location;
	const leveldb::Options options;

	DestroyJob(leveldb::DB* db, const std::string& location_, const leveldb::Options& options_, Callback callback_):
		Job(db, callback_), location(location_), options(options_)
	{}

	DestroyJob(leveldb::DB* db, const v8::String::AsciiValue& location_data, const leveldb::Options& options_, Callback callback_):
		Job(db, callback_), location(*location_data, location_data.length()), options(options_)
	{}

	virtual void operate()
	{
		status = leveldb::DestroyDB(location, options);
	}
};

}
