{
    "targets": [{
        "target_name": "callback-worker",
        "sources": [ "worker.cpp" ],
        'cflags': [ '-lpulse', '-fexceptions' ],
        'cflags_cc': [ '-lpulse', '-fexceptions' ],
        "include_dirs": [
            "<!@(node -p \"require('node-addon-api').include\")",
            "/usr/include"
        ],
        "libraries": [
            "/usr/lib/x86_64-linux-gnu/libpulse.so"
        ],
        'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
    }]
}
