# top-level SConstruct
print '..Building Coherence Benchmark'

# define the attributes of the build environment
common_env = Environment(
        CPPPATH=[],
        CPPDEFINES=[],
        LIBS=[],
        CXXFLAGS=[],
        SCONS_CXX_STANDARD='c++11')

# TODO: make this work across platforms
common_env.Append(CXXFLAGS=['-std=c++11'])

build_envs = {}

# release build based on common build environment
release_env = common_env.Clone()
#release_env.VariantDir('build/src', 'src', duplicate=0)
#release_env.VariantDir('build/regress', 'regress', duplicate=0)
build_envs['release'] = release_env

# iterate and invoke the lower level sconscript files
for mode,env in build_envs.iteritems():
    modeDir = 'build/%s' % mode
    env.SConscript('src/SConscript', exports = {'env': env})
    env.SConscript('regress/SConscript', exports = {'env': env})

