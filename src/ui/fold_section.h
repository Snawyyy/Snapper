#ifndef SNAPPER_UI_FOLD_SECTION_H_
#define SNAPPER_UI_FOLD_SECTION_H_

#include <QFormLayout>
#include <QPointer>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWidget>

namespace snapper {

class MotionManager;

// A heading that folds the rows under it away. It opens and closes by
// sliding over the Move time (at once in Off). Whether it is open is
// kept in the settings file under key, so it stays as the user left it.
class FoldSection final : public QWidget {
  Q_OBJECT

 public:
  // settings_path may be empty: the fold is then not remembered.
  // is_open_at_first is how it starts until the user has folded it.
  FoldSection(const QString& title, MotionManager* motion,
              const QString& settings_path, const QString& key,
              QWidget* parent, bool is_open_at_first = false);

  // The rows of the folded part.
  QFormLayout* form() { return &form_; }
  bool is_open() const { return is_open_; }
  void SetOpen(bool is_open);

 private:
  void Slide(int from, int to);
  void Finish();
  void Remember() const;

  MotionManager* motion_;
  QString settings_path_;
  QString key_;
  QString title_;
  QVBoxLayout layout_;
  QToolButton header_;
  QWidget body_;
  QFormLayout form_;
  QPointer<QVariantAnimation> animation_;
  bool is_open_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FOLD_SECTION_H_
