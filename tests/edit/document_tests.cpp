#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "edit/document_manager.h"
#include "edit/history_manager.h"

namespace snapper {

class DocumentTests final : public QObject {
  Q_OBJECT

 private slots:
  void NewStartsCleanAndChecksSize();
  void SaveNeedsAPlaceFirst();
  void SaveAndOpenRoundTrip();
  void SettingsAreUndoable();
  void AutosaveRecoversUnsavedWork();
  void RecentFilesStayShortAndReal();
};

void DocumentTests::NewStartsCleanAndChecksSize() {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document(&history, dir.path());
  QVERIFY(document.New("Clip", {1080, 1920}).has_value());
  QCOMPARE(history.current().canvas, (CanvasSize{1080, 1920}));
  QVERIFY(!history.IsDirty());
  QVERIFY(!document.New("Clip", {1081, 1920}).has_value());
  QVERIFY(!document.New("  ", {1080, 1920}).has_value());
}

void DocumentTests::SaveNeedsAPlaceFirst() {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document(&history, dir.path());
  const auto saved = document.Save();
  QVERIFY(!saved.has_value());
  QVERIFY(saved.error().message.contains("where"));
}

void DocumentTests::SaveAndOpenRoundTrip() {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document(&history, dir.path());
  QSignalSpy moved(&document, &DocumentManager::PathChanged);
  QVERIFY(document.Rename("Teto").has_value());
  QVERIFY(history.IsDirty());
  QVERIFY(document.SaveAs(QDir(dir.path()).filePath("teto")).has_value());
  QVERIFY(document.path().endsWith("teto.snapper"));
  QVERIFY(!history.IsDirty());
  QCOMPARE(moved.count(), 1);
  QVERIFY(document.New("Other", {640, 480}).has_value());
  QVERIFY(document.Open(QDir(dir.path()).filePath("teto.snapper"))
              .has_value());
  QCOMPARE(history.current().name, QString("Teto"));
  QVERIFY(!history.CanUndo());
}

void DocumentTests::SettingsAreUndoable() {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document(&history, dir.path());
  const QString song = QDir(dir.path()).filePath("song.wav");
  QFile file(song);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();
  QVERIFY(document.SetSong(song).has_value());
  QCOMPARE(history.current().song, song);
  QVERIFY(!document.SetSong(song + "x").has_value());
  QVERIFY(document.SetCanvas({720, 720}).has_value());
  QVERIFY(!document.SetCanvas({0, 720}).has_value());
  history.Undo();
  QCOMPARE(history.current().canvas, CanvasSize());
  history.Undo();
  QVERIFY(history.current().song.isEmpty());
}

void DocumentTests::AutosaveRecoversUnsavedWork() {
  QTemporaryDir dir;
  {
    HistoryManager history{Project()};
    DocumentManager document(&history, dir.path());
    QVERIFY(document.Autosave().has_value());
    QVERIFY(!document.HasAutosave());
    QVERIFY(document.Rename("Lost work").has_value());
    QVERIFY(document.Autosave().has_value());
    QVERIFY(document.HasAutosave());
  }
  HistoryManager history{Project()};
  DocumentManager document(&history, dir.path());
  QVERIFY(document.HasAutosave());
  QVERIFY(document.Recover().has_value());
  QCOMPARE(history.current().name, QString("Lost work"));
  QVERIFY(history.IsDirty());
  QVERIFY(!history.CanUndo());
  QVERIFY(!document.HasPath());
  QVERIFY(document.SaveAs(QDir(dir.path()).filePath("kept")).has_value());
  QVERIFY(!document.HasAutosave());
}

void DocumentTests::RecentFilesStayShortAndReal() {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document(&history, dir.path());
  for (int i = 0; i < kMaxRecentFiles + 2; ++i) {
    QVERIFY(document.SaveAs(QDir(dir.path()).filePath(QString::number(i)))
                .has_value());
  }
  QStringList recent = document.RecentFiles();
  QCOMPARE(recent.size(), kMaxRecentFiles);
  QVERIFY(recent.first().endsWith(
      QString::number(kMaxRecentFiles + 1) + ".snapper"));
  QFile::remove(recent.first());
  QCOMPARE(document.RecentFiles().size(), kMaxRecentFiles - 1);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::DocumentTests)
#include "document_tests.moc"
