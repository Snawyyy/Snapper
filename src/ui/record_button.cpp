#include "ui/record_button.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "edit/playback_manager.h"
#include "edit/record_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kDotShare = 0.32;
constexpr double kMeterHeight = 3.0;

}  // namespace

RecordButton::RecordButton(const Managers& managers)
    : TapeKey(tr("Rec")), managers_(managers) {
  assert(managers_.record != nullptr);
  assert(managers_.playback != nullptr);
  setObjectName("record");
  // Room for the dot before the print.
  SetPrintInset(fontMetrics().height() + 4.0);
  setFocusPolicy(Qt::NoFocus);
  RecordManager* record = managers_.record;
  connect(this, &QPushButton::clicked, this, [this, record] {
    const QString problem = ProblemOf(record->Toggle());
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  });
  connect(record, &RecordManager::StateChanged, this, &RecordButton::Refresh);
  connect(record, &RecordManager::Beat, this, [this] { Refresh(); });
  connect(record, &RecordManager::Problem, this, &RecordButton::Problem);
  connect(record, &RecordManager::Level, this, [this](double peak) {
    level_ = std::clamp(peak, 0.0, 1.0);
    update();
  });
  connect(managers_.playback, &PlaybackManager::TimelineChanged, this,
          &RecordButton::Refresh);
  Refresh();
}

void RecordButton::Refresh() {
  assert(managers_.record != nullptr);
  assert(kCountInBeats > 0);
  const RecordManager& record = *managers_.record;
  const RecordManager::State state = record.state();
  const bool is_idle = state == RecordManager::State::kIdle;
  if (is_idle) {
    level_ = 0.0;
  }
  setText(state == RecordManager::State::kCounting
              ? QString::number(record.count_left())
          : is_idle ? tr("Rec")
                    : tr("Stop"));
  const int armed = record.armed_track();
  const QString target =
      armed >= 0 ? tr("the armed sound track") : tr("a free sound track");
  Explain(this, record.WhyNoRecord(),
          is_idle ? tr("Records sound from the playhead while the video "
                       "plays, after a %1 beat count-in, onto %2. Arm a "
                       "track with its R button. Press again or stop the "
                       "video to end the take; Escape throws it away.")
                        .arg(kCountInBeats)
                        .arg(target)
                  : tr("Ends the take and lays it on the reel."));
  update();
}

void RecordButton::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.record != nullptr);
  TapeKey::paintEvent(event);
  QPainter painter(this);
  const bool is_on = isEnabled();
  const bool is_recording =
      managers_.record->state() == RecordManager::State::kRecording;
  // A square dot on the key's top, as every mark here is square: dim
  // when greyed. It rides down with the key.
  const QRectF top = TopFace();
  const double side = height() * kDotShare;
  const QRectF dot(top.left() + side * 0.8, top.center().y() - side / 2.0,
                   side, side);
  painter.fillRect(dot, is_on ? theme::kPick : theme::kTextOff);
  if (is_recording) {
    const double width = (top.width() - 4.0) * level_;
    painter.fillRect(QRectF(top.left() + 2.0, top.bottom() - kMeterHeight,
                            width, kMeterHeight),
                     theme::kHandle);
  }
}

}  // namespace snapper
