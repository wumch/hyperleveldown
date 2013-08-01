
#include <node.h>
#include <v8.h>
#include "./db.h"
#include "./jstatus.h"
#include "./jiterator.h"

extern "C" void init(v8::Handle<v8::Object> exports)
{
	leveldb::HyperLevelDB::init(exports);
	leveldb::Jstatus::init(exports);
	leveldb::Jiterator::init(exports);
}

NODE_MODULE(hyperleveldb, init)
