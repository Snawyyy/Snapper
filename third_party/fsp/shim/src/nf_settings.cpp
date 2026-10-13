// Stand-ins for F's NF_Settings.cpp (preset files picked with Windows
// file dialogs, used by preset buttons in After Effects' effect panel).
// Snapper keeps its own presets, so each one fails loudly. Defined here
// once, at global scope, where unit_prelude.h declares them.

#include <string>
#include <vector>

#include "fsp_shim/host_api.h"

namespace {

std::string Refuse(const char* call) {
  fsp_shim_unsupported(call);
  return std::string();
}

}  // namespace

std::string GetPluginConfigFilePath(const std::string&) {
  return Refuse("NF_Settings GetPluginConfigFilePath");
}

std::string loadTextFile(std::string) {
  return Refuse("NF_Settings loadTextFile");
}

void saveTextFile(std::string, std::string) {
  Refuse("NF_Settings saveTextFile");
}

std::string OpenFileDialog(std::string, std::string,
                           std::vector<std::string>, std::string) {
  return Refuse("NF_Settings OpenFileDialog");
}

std::string SaveFileDialog(std::string, std::string,
                           std::vector<std::string>, std::string) {
  return Refuse("NF_Settings SaveFileDialog");
}

std::string SaveFileDialogWithCheck(std::string, std::string,
                                    std::vector<std::string>, std::string) {
  return Refuse("NF_Settings SaveFileDialogWithCheck");
}

std::string ChangeFileExtension(const std::string&, const std::string&) {
  return Refuse("NF_Settings ChangeFileExtension");
}

std::string GetDocumentsFolderPath() {
  return Refuse("NF_Settings GetDocumentsFolderPath");
}
