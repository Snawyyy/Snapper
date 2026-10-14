#include <QMenu>
#include <QMenuBar>
#include <QTableWidget>
#include <QTest>
#include <QWidget>

#include <cassert>

#include "bench.h"
#include "ui/command_palette.h"
#include "ui/shortcut_sheet.h"
#include "ui/storyboard_menu.h"

namespace snapper {
namespace {

Project OneShotProject() {
  Project project;
  Shot shot;
  shot.id = ShotId(1);
  project.shots = {std::make_shared<const Shot>(shot)};
  project.next_shot_id = 2;
  assert(project.shots.size() == 1);
  assert(IsValidCanvas(project.canvas));
  return project;
}

}  // namespace

class StoryboardMenuTests final : public QObject {
  Q_OBJECT

 private slots:
  void TheKeysShowInTheSheetAndTheCommandList();
  void TheyGreyOutWithAReason();
};

void StoryboardMenuTests::TheKeysShowInTheSheetAndTheCommandList() {
  Bench bench;
  QWidget window;
  QWidget stage(&window);
  StoryboardMenu menu(bench.All(), &stage);
  QMenuBar bar(&window);
  QMenu edit("&Edit");
  edit.addActions(menu.actions());
  bar.addMenu(&edit);
  QStringList found;
  for (const Command& command : CommandsOf(&bar)) {
    found.append(command.name + " = " + command.action->shortcut().toString());
  }
  QVERIFY(found.contains("Edit: New storyboard frame here = Alt+N"));
  QVERIFY(found.contains("Edit: Draw over storyboard frame = Alt+Shift+N"));
  ShortcutSheet sheet(&bar, &window);
  sheet.Open();
  const auto* table = sheet.findChild<QTableWidget*>("shortcut_table");
  QVERIFY(table != nullptr);
  QStringList rows;
  for (int row = 0; row < table->rowCount(); ++row) {
    rows.append(table->item(row, 0)->text() + " = " +
                table->item(row, 1)->text());
  }
  QVERIFY(rows.contains("Alt+N = Edit: New storyboard frame here"));
  QVERIFY(rows.contains("Alt+Shift+N = Edit: Draw over storyboard frame"));
}

void StoryboardMenuTests::TheyGreyOutWithAReason() {
  Bench bench;
  QWidget window;
  QWidget stage(&window);
  window.show();
  StoryboardMenu menu(bench.All(), &stage);
  QAction* fresh = menu.actions()[0];
  QAction* over = menu.actions()[1];
  // No shot yet.
  QVERIFY(!fresh->isEnabled());
  QVERIFY(!fresh->toolTip().isEmpty());
  bench.history.Reset(OneShotProject());
  menu.Refresh();
  QVERIFY(fresh->isEnabled());
  // Nothing to draw over before the first frame.
  QVERIFY(!over->isEnabled());
  QVERIFY(!over->toolTip().isEmpty());
  // Off the Pose tab the stage is hidden.
  stage.hide();
  menu.Refresh();
  QVERIFY(!fresh->isEnabled());
  QVERIFY(fresh->toolTip().contains("Pose tab"));
}

}  // namespace snapper

QTEST_MAIN(snapper::StoryboardMenuTests)
#include "storyboard_menu_tests.moc"
