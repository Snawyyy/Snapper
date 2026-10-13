#ifndef SNAPPER_EDIT_LIBRARY_FOLDER_PATHS_H_
#define SNAPPER_EDIT_LIBRARY_FOLDER_PATHS_H_

#include <QString>

#include <cassert>

namespace snapper {

// Library folder paths: "Hair/Long" is Long inside Hair; the top is "".

inline QString ParentOf(const QString& path) {
  assert(!path.endsWith('/'));
  assert(!path.startsWith('/'));
  const int cut = static_cast<int>(path.lastIndexOf('/'));
  return cut < 0 ? QString() : path.left(cut);
}

inline QString NameOf(const QString& path) {
  assert(!path.endsWith('/'));
  assert(!path.startsWith('/'));
  return path.mid(static_cast<int>(path.lastIndexOf('/')) + 1);
}

inline QString Join(const QString& parent, const QString& name) {
  assert(!name.contains('/'));
  assert(!parent.endsWith('/'));
  return parent.isEmpty() ? name : parent + '/' + name;
}

// Whether path is folder or lies inside it.
inline bool IsWithin(const QString& path, const QString& folder) {
  assert(!folder.isEmpty());
  assert(!folder.endsWith('/'));
  return path == folder || path.startsWith(folder + '/');
}

// path with the folder from at its front replaced by to.
inline QString Moved(const QString& path, const QString& from,
                     const QString& to) {
  assert(IsWithin(path, from));
  assert(!to.endsWith('/'));
  const QString rest = path.mid(static_cast<int>(from.size()));
  const bool has_rest = !rest.isEmpty();
  return has_rest ? Join(to, rest.mid(1)) : to;
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_LIBRARY_FOLDER_PATHS_H_
