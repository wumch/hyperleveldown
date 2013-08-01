
{
    "targets": [
        {
            "target_name": "hyperleveldb",
            "sources": [
                "hyperleveldb.cc", 
                "db.cc"
            ],
            "include_dirs": [
                "/usr/include/node",
                "/usr/include/hyperleveldb"
            ],
            "cflags_cc": [
                "-fPIC", "-O2", "-pthread",
                "-finline", "-finline-small-functions",
                "-fomit-frame-pointer", "-momit-leaf-frame-pointer",
                "-Wno-unused-function"
            ],
            "defines": ["NDEBUG"],
            "conditions": [
                [
                    "OS=='linux'",
                    {
                        "link_settings": {
                            "ldflags": ["-Wl,-O3"],
                            "libraries": ["-lhyperleveldb"]
                        }
                    }
                ]
            ]
        }
    ]
}
