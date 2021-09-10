# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2021 Iñigo Martinez <inigomartinez@gmail.com>

project(
  'ModemManager', 'c',
  version: '1.19.0',
  license: 'GPL2',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu89',
    'warning_level=2',
  ],
  meson_version: '>= 0.53.0',
)

mm_name = meson.project_name()
mm_version = meson.project_version()
version_array = mm_version.split('.')
mm_major_version = version_array[0].to_int()
mm_minor_version = version_array[1].to_int()
mm_micro_version = version_array[2].to_int()

mm_prefix = get_option('prefix')
mm_datadir = get_option('datadir')
mm_includedir = get_option('includedir')
mm_libdir = get_option('libdir')
mm_sbindir = get_option('sbindir')
mm_sysconfdir = get_option('sysconfdir')

mm_pkgdatadir = mm_datadir / mm_name
mm_pkgincludedir = mm_includedir / mm_name
mm_pkglibdir = mm_libdir / mm_name

mm_glib_name = 'libmm-glib'
mm_glib_pkgincludedir = mm_includedir / mm_glib_name

# libtool versioning for libmm-glib (-version-info c:r:a)
# - If the interface is unchanged, but the implementation has changed or been fixed, then increment r
# - Otherwise, increment c and zero r.
#   - If the interface has grown (that is, the new library is compatible with old code), increment a.
#   - If the interface has changed in an incompatible way (that is, functions have changed or been removed), then zero a.
current = 8
revision = 0
age = 8
mm_glib_version = '@0@.@1@.@2@'.format(current - age, age, revision)

mm_gir_version = '1.0'

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

source_root = meson.current_source_dir()
build_root = meson.current_build_dir()

build_aux_dir = source_root / 'build-aux'
plugins_dir = source_root / 'plugins'
po_dir = source_root / 'po'
src_dir = source_root / 'src'

top_inc = include_directories('.')

cc = meson.get_compiler('c')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', mm_version)
config_h.set_quoted('VERSION', mm_version)

# Globally define_GNU_SOURCE and therefore enable the GNU extensions
config_h.set('_GNU_SOURCE', true)

# compiler flags
common_args = ['-DHAVE_CONFIG_H']

# compiler flags that are always enabled, even in release builds
cc_args = cc.get_supported_arguments([
  # warning on unused parameters is overkill, never do that
  '-Wno-unused-parameter',
  # function type cast disabled: used throughout the code especially to
  # cast GAsyncReadyCallbacks with the real object type instead of GObject
  '-Wno-cast-function-type',
  # all message protocol structs are packed, never complain about it
  '-Wno-packed',
  # we use some floating point ids as unknown, so we want to compare with them
  '-Wno-float-equal',
  # avoid warning if we're ignoring fields on purpose
  '-Wno-missing-field-initializers',
])

# strict flags to use in debug builds
if get_option('buildtype').contains('debug')
  cc_args += cc.get_supported_arguments([
    '-fno-strict-aliasing',
    '-Waggregate-return',
    '-Wcast-align',
    '-Wdeclaration-after-statement',
    '-Wdouble-promotion',
    '-Wduplicated-branches',
    '-Wduplicated-cond',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Winit-self',
    '-Winline',
    '-Wjump-misses-init',
    '-Wlogical-op',
    '-Wnested-externs',
    '-Wmissing-declarations',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wmissing-prototypes',
    '-Wnull-dereference',
    '-Wpointer-arith',
    '-Wredundant-decls',
    '-Wrestrict',
    '-Wreturn-type',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wsuggest-attribute=format',
    '-Wswitch-default',
    '-Wswitch-enum',
    '-Wundef',
    '-Wunused-but-set-variable',
    '-Wwrite-strings',
  ])
endif

add_project_arguments(common_args + cc_args, language: 'c')

glib_version = '2.56'

gio_unix_dep = dependency('gio-unix-2.0')
glib_dep = dependency('glib-2.0', version: '>= ' + glib_version)
gmodule_dep = dependency('gmodule-2.0')

deps = [
  glib_dep,
  dependency('gio-2.0'),
  dependency('gobject-2.0'),
]

c_args = [
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_' + glib_version.underscorify(),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_' + glib_version.underscorify(),
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
]

glib_deps = declare_dependency(
  dependencies: deps,
  compile_args: c_args,
)

# DBus system directory
dbus_dep = dependency('dbus-1')
dbus_interfaces_dir = dbus_dep.get_pkgconfig_variable('interfaces_dir', define_variable: ['datadir', mm_datadir])
dbus_system_bus_services_dir = dbus_dep.get_pkgconfig_variable('system_bus_services_dir', define_variable: ['datadir', mm_datadir])

dbus_policy_dir = get_option('dbus_policy_dir')
if dbus_policy_dir == ''
  dbus_policy_dir = dbus_dep.get_pkgconfig_variable('sysconfdir', define_variable: ['sysconfdir', mm_sysconfdir]) / 'dbus-1/system.d'
endif

enable_bash_completion = get_option('bash_completion')
if enable_bash_completion
  bash_completion_dep = dependency('bash-completion')
  bash_completion_completionsdir = bash_completion_dep.get_pkgconfig_variable(
    'completionsdir',
    # bash-completion 2.10 changed the substitutions
    define_variable: bash_completion_dep.version().version_compare('>= 2.10') ? ['datadir', mm_datadir] : ['prefix', mm_prefix],
  )
endif

# udev support (enabled by default)
gudev_dep = dependency('gudev-1.0', version: '>= 232', required: get_option('udev'))
enable_udev = gudev_dep.found()
config_h.set('WITH_UDEV', enable_udev)

# udev base directory
udev_udevdir = get_option('udevdir')
if udev_udevdir == ''
  udev_udevdir = dependency('udev').get_pkgconfig_variable('udevdir')
endif
udev_rulesdir = udev_udevdir / 'rules.d'

# systemd unit / service files
systemd_systemdsystemunitdir = get_option('systemdsystemunitdir')
install_systemdunitdir = (systemd_systemdsystemunitdir != 'no')

if install_systemdunitdir and systemd_systemdsystemunitdir == ''
  systemd_dep = dependency('systemd', not_found_message: 'systemd required but not found, please provide a valid systemd user unit dir or disable it')
  systemd_systemdsystemunitdir = systemd_dep.get_pkgconfig_variable('systemdsystemunitdir', define_variable: ['root_prefix', mm_prefix])
endif

# Suspend/resume support
enable_systemd_suspend_resume = get_option('systemd_suspend_resume')
# systemd journal support
enable_systemd_journal = get_option('systemd_journal')

if enable_systemd_suspend_resume or enable_systemd_journal
  libsystemd_dep = dependency('libsystemd', version: '>= 209', required: false)
  if not libsystemd_dep.found()
    libsystemd_dep = dependency('libsystemd-login', version: '>= 183', required: false)
    if not libsystemd_dep.found()
      libsystemd_dep = dependency(
        'libelogind',
        version: '>= 209',
        not_found_message: 'libsystemd, libsystemd-login or elogind must be available at runtime for suspend/resume or systemd journal support',
      )
    endif
  endif
endif
config_h.set('WITH_SYSTEMD_SUSPEND_RESUME', enable_systemd_suspend_resume)
config_h.set('WITH_SYSTEMD_JOURNAL', enable_systemd_journal)

# PolicyKit
polkit = get_option('polkit')
enable_polkit = (polkit != 'no')
if enable_polkit
  polkit_gobject_dep = dependency('polkit-gobject-1', version: '>= 0.97', not_found_message: 'PolicyKit development headers are required')

  polkit_gobject_policydir = polkit_gobject_dep.get_pkgconfig_variable('policydir', define_variable: ['prefix', mm_prefix])

  policy_conf = {'MM_DEFAULT_USER_POLICY': (polkit == 'permissive' ? 'yes' : 'auth_self_keep')}
endif
config_h.set('WITH_POLKIT', enable_polkit)

# AT command via DBus support (disabled by default unless running in --debug)
# It is suggested that this option is only enabled in custom built systems and
# only if truly required.
enable_at_command_via_dbus = get_option('at_command_via_dbus')
config_h.set('WITH_AT_COMMAND_VIA_DBUS', enable_at_command_via_dbus)

# MBIM support (enabled by default)
mbim_glib_dep = dependency('mbim-glib', version: '>= 1.26.0', required: get_option('mbim'))
enable_mbim = mbim_glib_dep.found()
config_h.set('WITH_MBIM', enable_mbim)

# QMI support (enabled by default)
qmi_glib_dep = dependency('qmi-glib', version: '>= 1.30.2', required: get_option('qmi'))
enable_qmi = qmi_glib_dep.found()
config_h.set('WITH_QMI', enable_qmi)

# QRTR support (both as qrtr-glib and qmi-glib apis)
qrtr_glib_dep = dependency('qrtr-glib', version: '>= 1.0.0', required: get_option('qrtr'))
enable_qrtr = qrtr_glib_dep.found()
assert(not enable_qrtr or enable_qmi, 'QRTR support requires QMI enabled')
assert(not enable_qrtr or qmi_glib_dep.get_pkgconfig_variable('qmi_qrtr_supported').to_int().is_odd(), 'Couldn\'t find QRTR support in qmi-glib.')
config_h.set('WITH_QRTR', enable_qrtr)

# Distribution version string
dist_version = get_option('dist_version')
if dist_version != ''
  config_h.set('MM_DIST_VERSION', dist_version)
endif

util_dep = cc.find_library('util')

# introspection support
enable_gir = dependency('gobject-introspection-1.0', version: '>= 0.9.6', required: get_option('introspection')).found()

# vala support
enable_vapi = get_option('vapi')

# gtkdoc support
enable_gtk_doc = get_option('gtk_doc')

enable_plugins = not get_option('plugin_disable_all')

plugins_shared = {
  'foxconn': enable_mbim and enable_plugins,
  'icera': enable_plugins,
  'novatel': enable_plugins,
  'option': enable_plugins,
  'sierra': enable_plugins,
  'telit': enable_plugins,
  'xmm': enable_plugins,
}

plugins_options = {
  'altair-lte': enable_plugins,
  'anydata': enable_plugins,
  'broadmobi': enable_plugins,
  'cinterion': enable_plugins,
  'dell': enable_plugins,
  'dlink': enable_plugins,
  'fibocom': enable_plugins,
  'foxconn': enable_plugins,
  'generic': enable_plugins,
  'gosuncn': enable_plugins,
  'haier': enable_plugins,
  'huawei': enable_plugins,
  'iridium': enable_plugins,
  'linktop': enable_plugins,
  'longcheer': enable_plugins,
  'mbm': enable_plugins,
  'motorola': enable_plugins,
  'mtk': enable_plugins,
  'nokia': enable_plugins,
  'nokia-icera': enable_plugins,
  'novatel': enable_plugins,
  'novatel-lte': enable_plugins,
  'option': enable_plugins,
  'option-hso': enable_plugins,
  'pantech': enable_plugins,
  'qcom-soc': enable_plugins,
  'quectel': enable_plugins,
  'samsung': enable_plugins,
  'sierra-legacy': enable_plugins,
  'sierra': enable_plugins,
  'simtech': enable_plugins,
  'telit': enable_plugins,
  'thuraya': enable_plugins,
  'tplink': enable_plugins,
  'ublox': enable_plugins,
  'via': enable_plugins,
  'wavecom': enable_plugins,
  'x22x': enable_plugins,
  'zte': enable_plugins,
}

plugins_options_reqs = {
  'dell': ['foxconn', 'novatel', 'sierra', 'telit', 'xmm'],
  'fibocom': ['xmm'],
  'foxconn': ['foxconn'],
  'nokia-icera': ['icera'],
  'novatel': ['novatel'],
  'option': ['option'],
  'option-hso': ['option'],
  'samsung': ['icera'],
  'sierra-legacy': ['icera', 'sierra'],
  'sierra': ['xmm'],
  'telit': ['telit'],
  'zte': ['icera'],
}

foreach plugin_name, _: plugins_options
  if get_option('plugin_' + plugin_name.underscorify())
    foreach plugin_req: plugins_options_reqs.get(plugin_name, [])
      if not plugins_shared[plugin_req]
        assert(not enable_plugins, '@0@ required @1@ but is not available'.format(plugin_name, plugin_req))
        plugins_shared += {plugin_req: true}
      endif
    endforeach
  endif
endforeach

version_conf = {
  'MM_MAJOR_VERSION': mm_major_version,
  'MM_MINOR_VERSION': mm_minor_version,
  'MM_MICRO_VERSION': mm_micro_version,
  'VERSION': mm_version,
}

subdir('po')
subdir('data')
subdir('introspection')
subdir('include')

subdir('libqcdm/src')
subdir('libqcdm/tests')

subdir('libmm-glib')
subdir('src')
subdir('plugins')
subdir('cli')
subdir('test')
subdir('tools/tests')

subdir('examples/sms-c')

enable_man = get_option('man')
if enable_man
  subdir('docs/man')
endif

if enable_gtk_doc
  subdir('docs/reference/api')
  subdir('docs/reference/libmm-glib')
endif

configure_file(
  output: 'config.h',
  configuration: config_h,
)

summary({
  'compiler': cc.get_id(),
  'cflags': cc_args,
}, section: 'Build')

summary({
  'prefix': mm_prefix,
  'D-Bus policy directory': dbus_policy_dir,
  'udev base directory': udev_udevdir,
  'systemd user unit directory': systemd_systemdsystemunitdir,
}, section: 'System paths')

summary({
  'udev': enable_udev,
  'policykit': polkit,
  'mbim': enable_mbim,
  'qmi': enable_qmi,
  'qrtr': enable_qrtr,
  'systemd suspend/resume': enable_systemd_suspend_resume,
  'systemd journal': enable_systemd_journal,
  'at command via dbus': enable_at_command_via_dbus,
}, section: 'Features')

summary(plugins_shared, section: 'Shared utils')

summary(plugins_options, section: 'Plugins')

summary({
  'gobject introspection': enable_gir,
  'Man': enable_man,
  'Documentation': enable_gtk_doc,
  'bash completion': enable_bash_completion,
  'vala bindings': enable_vapi,
  'code coverage': get_option('b_coverage'),
}, section: 'Miscellaneous')