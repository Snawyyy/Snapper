#include "ui/golden_splitter.h"

#include <QEvent>
#include <QSettings>
#include <QSplitter>
#include <QWidget>

#include <cassert>
#include <memory>
#include <utility>

#include "ui/panel_kit.h"

namespace snapper {
namespace {

// The length the sizes are given in before the splitter has one of its
// own; it scales them to its real length when laid out.
constexpr int kNominal = 1000;

QString SplitKey(const QString& key, const QSplitter* splitter) {
  assert(splitter != nullptr);
  assert(!splitter->objectName().isEmpty());
  return key + QStringLiteral("/split_") + splitter->objectName();
}

// Saves the named splitters under root when root's window closes.
class SplitKeeper final : public QObject {
 public:
  SplitKeeper(QWidget* root, QString settings_path, QString key)
      : QObject(root), root_(root),
        settings_path_(std::move(settings_path)), key_(std::move(key)) {
    assert(root_ != nullptr);
    assert(!settings_path_.isEmpty() && !key_.isEmpty());
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(event != nullptr);
    assert(root_ != nullptr);
    const bool is_closing =
        watched == root_->window() && event->type() == QEvent::Close;
    if (is_closing) {
      QSettings settings(settings_path_, QSettings::IniFormat);
      for (const QSplitter* splitter : root_->findChildren<QSplitter*>()) {
        const bool is_named = !splitter->objectName().isEmpty();
        if (is_named) {
          settings.setValue(SplitKey(key_, splitter), splitter->saveState());
        }
      }
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  QWidget* root_;
  QString settings_path_;
  QString key_;
};

}  // namespace

void SplitGolden(QSplitter* splitter, int long_index) {
  assert(splitter != nullptr);
  assert(splitter->count() == 2 && (long_index == 0 || long_index == 1));
  const GoldenParts parts = GoldenSplit(kNominal);
  const int short_index = 1 - long_index;
  splitter->setStretchFactor(long_index, kGoldenLong);
  splitter->setStretchFactor(short_index, kGoldenShort);
  QList<int> sizes = {0, 0};
  sizes[long_index] = parts.long_part;
  sizes[short_index] = parts.short_part;
  splitter->setSizes(sizes);
}

void SplitGoldenAround(QSplitter* splitter) {
  assert(splitter != nullptr);
  assert(splitter->count() == 3);
  const GoldenParts parts = GoldenSplit(kNominal);
  const int side = parts.short_part / 2;
  splitter->setStretchFactor(0, kGoldenShort / 2);
  splitter->setStretchFactor(1, kGoldenLong);
  splitter->setStretchFactor(2, kGoldenShort / 2);
  splitter->setSizes({side, parts.long_part, side});
}

void RememberSplits(QWidget* root, const QString& settings_path,
                    const QString& key) {
  assert(root != nullptr);
  assert(!key.isEmpty() && !key.contains('/'));
  const bool is_kept = !settings_path.isEmpty();
  if (!is_kept) {
    return;
  }
  const QSettings settings(settings_path, QSettings::IniFormat);
  for (QSplitter* splitter : root->findChildren<QSplitter*>()) {
    const bool is_named = !splitter->objectName().isEmpty();
    const QByteArray state =
        is_named ? settings.value(SplitKey(key, splitter)).toByteArray()
                 : QByteArray();
    const bool has_state = !state.isEmpty();
    if (has_state) {
      splitter->restoreState(state);
    }
  }
  auto keeper = std::make_unique<SplitKeeper>(root, settings_path, key);
  root->window()->installEventFilter(keeper.get());
  keeper.release();
}

}  // namespace snapper
