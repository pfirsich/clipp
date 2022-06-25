project('cli', 'cpp', default_options : ['warning_level=3', 'cpp_std=c++17'])

clipp_dep = declare_dependency(include_directories : include_directories('.'))

if not meson.is_subproject()
  executable('clitest', 'test.cpp')

  executable('intro', 'examples/intro.cpp')
  executable('subcommands', 'examples/subcommands.cpp')
  executable('customtypes', 'examples/customtypes.cpp')
endif
