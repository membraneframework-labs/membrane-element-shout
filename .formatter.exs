[
  inputs: [
    "{lib,test,config,c_src}/**/*.{ex,exs}",
    "*.exs",
    ".formatter.exs"
  ],
  import_deps: [:membrane_core, :bundlex, :unifex]
]
