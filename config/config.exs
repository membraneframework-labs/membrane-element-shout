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
          "../../membrane_common_c/c_src/membrane/ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/portaudio/pa_ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/log.c",
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
          "../../membrane_common_c/c_src/membrane/ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/portaudio/pa_ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/log.c",
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
          "../../membrane_common_c/c_src/membrane/ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/portaudio/pa_ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/log.c",
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
          "../../membrane_common_c/c_src/membrane/ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/portaudio/pa_ringbuffer.c",
          "../../membrane_common_c/c_src/membrane/log.c",
        ],
        libs: [
        ],
        pkg_configs: [
          "shout",
        ]
      ]
    ]
]
