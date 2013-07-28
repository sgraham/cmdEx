def DEP_libgit2(action):
  if action == 'sync':
    GitPullOrClone('https://github.com/libgit2/libgit2.git')
    Run(['cmake', '-G', 'Visual Studio 11', '-DSTDCALL=Off'],
        cwd='third_party/libgit2')
    Run(['devenv.com', 'libgit2.sln', '/Build', 'Release'],
        cwd='third_party/libgit2')
  elif action == 'reftag':
    return '#include "git2.h', None  # We don't actually want to link against it.
  elif action == 'cflags':
    return ['/Ithird_party/libgit2/include']
