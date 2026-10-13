#ifndef SNAPPER_UI_VIDEO_PAGE_H_
#define SNAPPER_UI_VIDEO_PAGE_H_

#include <QElapsedTimer>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <set>

#include "base/frame.h"
#include "model/reel.h"
#include "model/shot.h"
#include "ui/flow_layout.h"
#include "ui/managers.h"
#include "ui/record_button.h"
#include "ui/reel_preview.h"
#include "ui/reel_text_panel.h"
#include "ui/reel_timeline.h"
#include "ui/shot_bin.h"
#include "ui/shot_clip_panel.h"
#include "ui/sound_clip_panel.h"
#include "ui/subtitle_panel.h"
#include "ui/transport_bar.h"

namespace snapper {

// Which side panels the Video tab shows for a pick: only what the pick
// needs.
struct SidePanels final {
  bool text = false;
  bool subtitles = false;
  bool sound = false;
  bool shot = false;
  // Nothing above applies: a hint says what to pick.
  bool IsEmpty() const { return !text && !subtitles && !sound && !shot; }
};
// Text clips alone show Text (and Subtitles when one is a cue); sound
// clips alone the Sound clip panel; shot clips alone the Shot clip
// panel; nothing picked the Subtitles panel, where lines are written to
// time; anything else (video clips, a mixed pick) none.
SidePanels SidePanelsFor(const Reel& reel, const std::set<ClipId>& picked);

// The Video tab, where the final video is cut: the shot list to drag
// from (with a "+" tile: New shot here at the playhead), the preview
// and the side panel for the pick on top, the reel's timeline below,
// and a wrapping bar with the play controls, Rec (record sound) and
// buttons to bring in video files, add text (T+, Ctrl+T), split clips,
// mark cuts and add tracks. L starts the next subtitle line at the
// playhead; held, it ends that line when let go.
class VideoPage final : public QWidget {
  Q_OBJECT

 public:
  explicit VideoPage(const Managers& managers);

  ShotBin* bin() { return &bin_; }
  ReelTimeline* timeline() { return &timeline_; }
  // Puts each file on the bottom track, after what is there; the file
  // dialog's answer goes here.
  void ImportVideos(const QStringList& paths);

 signals:
  void Problem(const QString& why);
  // Open shot on the Pose tab at frame (counted in the shot): a double
  // click on a shot clip or in the shot list, or the Shot clip panel.
  void OpenShotAsked(ShotId shot, Frame frame);

 protected:
  // Sees L let go, wherever the focus is, to end a held subtitle line.
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void BuildBar();
  void BuildSide();
  void AskForVideos();
  void AddText();
  void EditText(ClipId clip);
  void PressTap();
  void ReleaseTap();
  void WireText();
  void Refresh();
  void ShowSide();
  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }

  Managers managers_;
  // Containers come before what they hold, so the held widgets leave
  // them before they go.
  QVBoxLayout layout_;
  QSplitter split_;
  QSplitter top_;
  QWidget bin_column_;
  QVBoxLayout bin_layout_;
  QWidget bottom_;
  QVBoxLayout bottom_layout_;
  QWidget bar_;
  FlowLayout bar_layout_;
  QTabWidget side_;
  ShotBin bin_;
  // "+": New shot here, over the marked range or the gap at the playhead.
  QPushButton new_shot_;
  ReelPreview preview_;
  ReelTextPanel text_panel_;
  SubtitlePanel subtitle_panel_;
  SoundClipPanel sound_panel_;
  ShotClipPanel shot_panel_;
  QWidget hint_page_;
  QVBoxLayout hint_layout_;
  QPushButton import_;
  // T+: a text clip at the playhead (Ctrl+T).
  QPushButton add_text_;
  // L: the next subtitle line; how long it has been held.
  QShortcut tap_key_;
  QElapsedTimer tap_clock_;
  QPushButton split_button_;
  QPushButton mark_;
  QPushButton add_track_;
  TransportBar transport_;
  // Only with a RecordManager.
  std::unique_ptr<RecordButton> record_;
  ReelTimeline timeline_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VIDEO_PAGE_H_
