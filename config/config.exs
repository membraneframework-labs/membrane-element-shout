use Mix.Config

config :membrane_element_shout, :bundlex_lib,
  macosx: [
    nif: [
      membrane_element_shout_sink: [
        includes: [
          "../membrane_common_c/include",
          "./deps/membrane_common_c/include",
        ],
        sources: [
          "sink.c",
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
          "../membrane_common_c/include",
          "./deps/membrane_common_c/include",
        ],
        sources: [
          "sink.c",
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
          "../membrane_common_c/include",
          "./deps/membrane_common_c/include",
        ],
        sources: [
          "sink.c",
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
          "../membrane_common_c/include",
          "./deps/membrane_common_c/include",
        ],
        sources: [
          "sink.c",
        ],
        libs: [
        ],
        pkg_configs: [
          "shout",
        ]
      ]
    ]
]
