project(
  'mycomp',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
  ],
)

add_project_arguments('-DWLR_USE_UNSTABLE', '-Wno-unused-parameter', language: 'c')

fs = import('fs')

# Required dependencies
wlroots = dependency('wlroots-0.20', 'wlroots', version: ['>=0.17.0', '<=0.20.2'])
wayland_server = dependency('wayland-server')
xkbcommon = dependency('xkbcommon')

# Optional dependencies
cairo = dependency('cairo', required: false, disabler: true)
egl = dependency('egl', required: false, disabler: true)
glesv2 = dependency('glesv2', required: false, disabler: true)
wayland_client = dependency('wayland-client', required: false, disabler: true)
wayland_egl = dependency('wayland-egl', required: false, disabler: true)

# Wayland protocols and scanner
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
  wayland_scanner_dep.get_variable('wayland_scanner'),
  native: true,
)

xdg_shell_xml = wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_server_header = custom_target(
  'xdg-shell-protocol.h',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)

xdg_shell = declare_dependency(
  sources: [xdg_shell_server_header],
)

proto_deps = {
  'xdg-shell': xdg_shell,
}

# Program registry
programs = {
  'my-scene-graph': {
    'src': ['my-scene-graph.c'],
  },
  'simple': {
    'src': ['simple.c'],
  },
  'pointer': {
    'src': ['pointer.c'],
  },
  'touch': {
    'src': ['touch.c', 'cat.c'],
  },
  'tablet': {
    'src': ['tablet.c'],
  },
  'rotation': {
    'src': ['rotation.c', 'cat.c'],
  },
  'output-layout': {
    'src': ['output-layout.c', 'cat.c'],
  },
  'scene-graph': {
    'src': ['scene-graph.c'],
    'proto': ['xdg-shell'],
  },
  'output-layers': {
    'src': ['output-layers.c'],
    'proto': ['xdg-shell'],
  },
  'cairo-buffer': {
    'src': ['cairo-buffer.c'],
    'dep': [cairo],
  },
}

foreach name, info : programs
  srcs = info.get('src', [])
  all_srcs_exist = true
  foreach s : srcs
    if not fs.exists(s)
      message('Bỏ qua mục "@0@": file nguồn "@1@" không tồn tại'.format(name, s))
      all_srcs_exist = false
    endif
  endforeach

  if not all_srcs_exist
    continue
  endif

  extra_deps = []
  foreach p : info.get('proto', [])
    if p in proto_deps
      extra_deps += proto_deps[p]
    else
      error('Giao thức không xác định "@0@" được yêu cầu bởi "@1@"'.format(p, name))
    endif
  endforeach

  executable(
    name,
    srcs,
    dependencies: [
      wlroots,
      wayland_server,
      xkbcommon,
      extra_deps,
      info.get('dep', []),
    ],
  )
endforeach
