#include "media/voice_worker.h"

#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <cassert>
#include <utility>

#include "base/text.h"

namespace snapper {
namespace {

constexpr int kKeptErrors = 6;
constexpr int kStartWaitMs = 3000;
constexpr int kStopWaitMs = 3000;
// A bound on the answers handled from one read.
constexpr int kMaxAnswersPerRead = 1000;

QByteArray JobLine(int id, const VoiceJob& job) {
  assert(id > 0);
  assert(!job.sample.isEmpty() || job.kind == VoiceKind::kDesign);
  const bool is_transcribe = job.kind == VoiceKind::kTranscribe;
  if (is_transcribe) {
    const QJsonObject json{
        {"id", id}, {"kind", "transcribe"}, {"sample", job.sample}};
    return QJsonDocument(json).toJson(QJsonDocument::Compact) + '\n';
  }
  const bool is_design = job.kind == VoiceKind::kDesign;
  QJsonObject json{{"id", id},
                   {"kind", is_design ? "design" : "clone"},
                   {"text", job.text},
                   {"language", job.language},
                   {"out", job.out}};
  if (is_design) {
    json.insert("describe", job.describe);
  } else {
    json.insert("sample", job.sample);
    json.insert("words", job.words);
  }
  return QJsonDocument(json).toJson(QJsonDocument::Compact) + '\n';
}

}  // namespace

VoiceWorker::VoiceWorker(QString python, QString script)
    : python_(std::move(python)), script_(std::move(script)) {
  assert(!python_.isEmpty());
  assert(!script_.isEmpty());
  connect(&process_, &QProcess::readyReadStandardOutput, this,
          &VoiceWorker::ReadAnswers);
  connect(&process_, &QProcess::readyReadStandardError, this,
          &VoiceWorker::ReadErrors);
  connect(&process_, &QProcess::finished, this, &VoiceWorker::Finished);
}

VoiceWorker::~VoiceWorker() {
  assert(kStopWaitMs > 0);
  assert(!python_.isEmpty());
  is_cancelling_ = true;
  Stop();
}

bool VoiceWorker::IsInstalled() const {
  assert(!python_.isEmpty());
  assert(!script_.isEmpty());
  return QFileInfo::exists(python_) && QFileInfo::exists(script_);
}

Result<void> VoiceWorker::Submit(const VoiceJob& job) {
  assert(!job.out.isEmpty() || job.kind == VoiceKind::kTranscribe);
  assert(kStartWaitMs > 0);
  const bool is_busy = IsBusy();
  if (is_busy) {
    return std::unexpected(Error{Tr("Already speaking.")});
  }
  const bool is_down = process_.state() == QProcess::NotRunning;
  if (is_down) {
    answers_.clear();
    process_.start(python_, {QStringLiteral("-u"), script_});
    const bool is_started = process_.waitForStarted(kStartWaitMs);
    if (!is_started) {
      return std::unexpected(Error{Tr("Could not start the voice worker.")});
    }
  }
  const int id = next_id_++;
  const qint64 sent = process_.write(JobLine(id, job));
  const bool is_sent = sent > 0;
  if (!is_sent) {
    return std::unexpected(Error{Tr("Could not talk to the voice worker.")});
  }
  pending_id_ = id;
  return {};
}

void VoiceWorker::Cancel() {
  assert(kStopWaitMs > 0);
  assert(next_id_ > 0);
  is_cancelling_ = true;
  Stop();
  is_cancelling_ = false;
  pending_id_ = 0;
  answers_.clear();
}

void VoiceWorker::Stop() {
  assert(kStopWaitMs > 0);
  assert(is_cancelling_);
  const bool is_running = process_.state() != QProcess::NotRunning;
  if (is_running) {
    process_.kill();
    process_.waitForFinished(kStopWaitMs);
  }
}

void VoiceWorker::ReadAnswers() {
  assert(kMaxAnswersPerRead > 0);
  assert(next_id_ > 0);
  answers_.append(process_.readAllStandardOutput());
  for (int i = 0; i < kMaxAnswersPerRead; ++i) {
    const qsizetype end = answers_.indexOf('\n');
    const bool has_line = end >= 0;
    if (!has_line) {
      return;
    }
    const QByteArray line = answers_.left(end);
    answers_.remove(0, end + 1);
    Answer(line);
  }
}

void VoiceWorker::Answer(const QByteArray& line) {
  assert(next_id_ > 0);
  assert(pending_id_ >= 0);
  const QJsonObject json = QJsonDocument::fromJson(line).object();
  const bool is_current = json.value("id").toInt() == pending_id_;
  const bool is_known = !json.isEmpty() && is_current && IsBusy();
  if (!is_known) {
    qDebug() << "voice worker said:" << line;
    return;
  }
  const QString state = json.value("state").toString();
  const bool is_loading = state == "loading";
  const bool is_speaking = state == "speaking";
  const bool is_done = state == "done";
  const bool is_error = state == "error";
  pending_id_ = is_done || is_error ? 0 : pending_id_;
  if (is_loading) {
    emit Loading();
  }
  if (is_speaking) {
    emit Speaking();
  }
  if (is_done) {
    emit Done(json.value("seconds").toDouble(),
              json.value("text").toString());
  }
  if (is_error) {
    emit Failed(json.value("why").toString());
  }
}

void VoiceWorker::ReadErrors() {
  assert(kKeptErrors > 0);
  assert(recent_errors_.size() <= kKeptErrors);
  const QString text = QString::fromUtf8(process_.readAllStandardError());
  for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
    qDebug().noquote() << "voice worker:" << line;
    recent_errors_.append(line.trimmed());
    const bool is_over = recent_errors_.size() > kKeptErrors;
    if (is_over) {
      recent_errors_.removeFirst();
    }
  }
}

void VoiceWorker::Finished() {
  assert(kKeptErrors > 0);
  assert(next_id_ > 0);
  ReadAnswers();
  ReadErrors();
  const bool is_crash = IsBusy() && !is_cancelling_;
  if (!is_crash) {
    return;
  }
  pending_id_ = 0;
  const QString last = recent_errors_.join('\n');
  emit Failed(last.isEmpty()
                  ? Tr("The voice worker stopped.")
                  : Tr("The voice worker stopped:\n%1").arg(last));
}

}  // namespace snapper
