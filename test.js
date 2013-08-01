
var HyperLevelDB = require("./build/Release/hyperleveldb").HyperLevelDB;

var db = new HyperLevelDB("/tmp/hyperleveldb");

var key_exists = "an-existing-key", a_value = "this-is-a-value",
    key_nonexists = "this-key-is-nonexists";

var testOpen = function() {
    var onOpen = function(err) {
        console.log("db.open() " + (err ? "failed" : "succed"));
        if (err) {
            console.log(err);
        }
        testPut();
    };
    db.open({cacheSize: 10 << 20, compression: false}, onOpen);
}

var testPut = function() {
    var onPut = function(err) {
        console.log("db.put() " + (err ? "failed" : "succed"));
        testBatch();
    };
    db.put(key_exists, a_value, onPut);
}

var testBatch = function() {
    var onBatch = function(err) {
        if (err) {
            console.log("db.batch(): " + err);
        } else {
            console.log("db.batch() succed");
        }
        testGet(key_exists);
    };
    var operations = [
        {type: "put", key: "key-that-exists", value: "value-1"},
        {type: "put", key: "key-that-nonexists", value: "value-2"},
        {type: "del", key: "key-that-nonexists"},
    ];
    db.batch(operations, onBatch);
}

var testGet = function(key) {
    var onGet = function(err, data) {
        if (err) {
            console.log("db.get() " + err + 
                ". err.ok():" + err.ok() + 
                ", err.isIOError():" + err.isIOError() + 
                ", err.isNotFound():" + err.isNotFound() + 
                ", err.isCorruption():" + err.isCorruption()
            );
            testDel();
        } else {
            console.log("db.get() succed: [" + data + "]");
            testGet(key_nonexists);
        }
    };
    db.get(key, onGet);
}

var testDel = function() {   
    var onDel = function(err) {
        console.log("db.del() " + (err === undefined ? "succed" : "failed"));
        testClose();
    };
    db.del(key_exists, onDel);
}

var testClose = function() {
    var onClose = function(err) {
        console.log("db.close() " + (err ? "failed" : "succed"));
        if (err) {
            console.log(err);
        }
    };
    db.close(onClose);
}

if (require.main == module) {
    testOpen();
}

