// Exercise the unchanged Linux NFD adapter using fake dialog responses.
#include "../lib/RecompFrontend/recompui/src/util/file.h"
#include <SDL.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <nfd.h>
static nfdresult_t dialogResult = NFD_OKAY;
static int freed = 0;
namespace recompui::programconfig {
const std::u8string &get_program_id() {
  static const std::u8string id = u8"fixture";
  return id;
}
} // namespace recompui::programconfig
extern "C" nfdresult_t NFD_OpenDialogN(nfdnchar_t **out,
                                       const nfdnfilteritem_t *,
                                       nfdfiltersize_t, const nfdnchar_t *) {
  if (dialogResult == NFD_OKAY)
    *out = strdup("/fixture/rom.z64");
  return dialogResult;
}
extern "C" void NFD_FreePathN(nfdnchar_t *path) {
  ++freed;
  free(path);
}
extern "C" nfdresult_t NFD_OpenDialogMultipleN(const nfdpathset_t **out,
                                               const nfdnfilteritem_t *,
                                               nfdfiltersize_t,
                                               const nfdnchar_t *) {
  *out = reinterpret_cast<const nfdpathset_t *>(1);
  return dialogResult;
}
extern "C" nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t *,
                                            nfdpathsetsize_t *count) {
  *count = 1;
  return NFD_OKAY;
}
extern "C" nfdresult_t NFD_PathSet_GetPathN(const nfdpathset_t *,
                                            nfdpathsetsize_t,
                                            nfdnchar_t **path) {
  *path = strdup("/fixture/mod.zip");
  return NFD_OKAY;
}
extern "C" void NFD_PathSet_Free(const nfdpathset_t *) {}
extern "C" int SDL_ShowSimpleMessageBox(Uint32, const char *, const char *,
                                        SDL_Window *) {
  return 0;
}
int main() {
  int callbacks = 0;
  recompui::file::open_file_dialog([&](bool good, const auto &path) {
    assert(good && path == "/fixture/rom.z64");
    ++callbacks;
  });
  assert(callbacks == 1 && freed == 1);
  dialogResult = NFD_CANCEL;
  recompui::file::open_file_dialog([&](bool good, const auto &path) {
    assert(!good && path.empty());
    ++callbacks;
  });
  dialogResult = NFD_ERROR;
  recompui::file::open_file_dialog([&](bool good, const auto &path) {
    assert(!good && path.empty());
    ++callbacks;
  });
  assert(callbacks == 3 && freed == 1);
  dialogResult = NFD_OKAY;
  recompui::file::open_file_dialog_multiple([&](bool good, const auto &paths) {
    assert(good && paths.size() == 1 && paths.front() == "/fixture/mod.zip");
    ++callbacks;
  });
  assert(callbacks == 4 && freed == 1);
  std::cout << "PASS: production Linux NFD single/multiple success, "
               "cancellation/error, synchronous callbacks and path ownership\n";
}
