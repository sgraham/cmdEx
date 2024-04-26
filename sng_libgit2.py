def DEP_libgit2(action):
  if action == 'sync':
    GitPullOrClone('https://github.com/libgit2/libgit2.git')
    Run(['cmake', '-G', 'Visual Studio 17 2022', '.'], cwd='third_party/libgit2')
    # PDB collision with git2.exe, have to build only dll.
    Run(['devenv.com', 'libgit2.sln', '/Build', 'Release', '/Project', 'libgit2package'],
        cwd='third_party/libgit2')
  elif action == 'reftag':
    return '#include "git2.h', None  # We don't actually want to link against it.
  elif action == 'cflags':
    return ['/Ithird_party/libgit2/include']
