# top-level SConstruct
print '..Building Coherence Benchmark'

# define the attributes of the build environment
common_env = Environment(
        CPPPATH=[],
        CPPDEFINES=[],
        LIBS=['pthread'],
        CXXFLAGS=[],
        SCONS_CXX_STANDARD='c++11')

build_envs = {}

# x86-platform build based on common build environment
x86_env = common_env.Clone()
x86_env.VariantDir('build/x86/src', 'src', duplicate=0)
x86_env.VariantDir('build/x86/regress', 'regress', duplicate=0)
x86_env.Append(CXXFLAGS=['-std=c++11', '-m64', '-pthread'])
#x86_env.Append(CXXFLAGS=['-g'])

build_envs['x86'] = x86_env

# iterate and invoke the lower level sconscript files
for mode,env in build_envs.iteritems():
    modeDir = 'build/%s' % mode
    env.SConscript('%s/src/SConscript' % modeDir, exports={'env':env})
    env.SConscript('%s/regress/SConscript' % modeDir, exports={'env':env})

