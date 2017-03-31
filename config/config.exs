use Mix.Config

config :membrane_element_shout, :bundlex_lib,
  macosx: [
    nif: [
      membrane_element_shout_sink: [
        includes: [
          "../membrane_common_c/c_src",
          "./deps/membrane_common_c/c_src",
        ],
        sources: [
          "sink.c",
          "../deps/membrane_common_c/c_src/membrane/ringbuffer.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/spsc_queue.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/memory.c",
        ],
        libs: [
        ],
        pkg_configs: [
          "shout",
        ]
      ]
    ]
  ],
  windows32: [
    nif: [
      membrane_element_shout_sink: [
        includes: [
          "../membrane_common_c/c_src",
          "./deps/membrane_common_c/c_src",
        ],
        sources: [
          "sink.c",
          "../deps/membrane_common_c/c_src/membrane/ringbuffer.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/spsc_queue.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/memory.c",
        ],
        libs: [
        ]
      ]
    ]
  ],
  windows64: [
    nif: [
      membrane_element_shout_sink: [
        includes: [
          "../membrane_common_c/c_src",
          "./deps/membrane_common_c/c_src",
        ],
        sources: [
          "sink.c",
          "../deps/membrane_common_c/c_src/membrane/ringbuffer.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/spsc_queue.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/memory.c",
        ],
        libs: [
        ]
      ]
    ]
  ],
  linux: [
    nif: [
      membrane_element_shout_sink: [
        includes: [
          "../membrane_common_c/c_src",
          "./deps/membrane_common_c/c_src",
        ],
        sources: [
          "sink.c",
          "../deps/membrane_common_c/c_src/membrane/ringbuffer.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/spsc_queue.c",
          "../deps/membrane_common_c/c_src/membrane/c11-queues/memory.c",
        ],
        libs: [
        ],
        pkg_configs: [
          "shout",
        ]
      ]
    ]
]
