#ifndef SNAPPER_UI_ACHIEVEMENTS_WINDOW_H_
#define SNAPPER_UI_ACHIEVEMENTS_WINDOW_H_

#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <vector>

namespace snapper {

class AchievementManager;

// The trophy case: your numbers at the top (strokes drawn, keys set,
// days in a row...), then every achievement on its shelf as a pixel
// badge with its name and how far along you are. Hovering one tells how
// to earn it and its joke. It shows what AchievementManager keeps and
// changes nothing.
class AchievementsWindow final : public QDialog {
  Q_OBJECT

 public:
  AchievementsWindow(AchievementManager* achievements, QWidget* parent);

 private:
  void Build();
  void Refresh();

  AchievementManager* achievements_;
  QVBoxLayout layout_;
  QLabel stats_;
  QLabel summary_;
  // One per achievement, in kAchievements order.
  std::vector<QLabel*> badges_;
  std::vector<QLabel*> captions_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_ACHIEVEMENTS_WINDOW_H_
