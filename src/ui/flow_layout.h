#ifndef SNAPPER_UI_FLOW_LAYOUT_H_
#define SNAPPER_UI_FLOW_LAYOUT_H_

#include <QLayout>
#include <QLayoutItem>
#include <QRect>
#include <QSize>

#include <memory>
#include <vector>

namespace snapper {

// Lays its widgets out left to right and wraps to a new line when the
// next one does not fit, like words in a paragraph: a button bar that
// stays whole on a narrow window instead of clipping. Its height follows
// its width (heightForWidth), so the bar grows a line when it wraps.
class FlowLayout final : public QLayout {
 public:
  // gap is the space between items, across and between lines; -1 takes
  // theme::kTileGap.
  explicit FlowLayout(QWidget* parent, int gap = -1);
  ~FlowLayout() override;
  FlowLayout(const FlowLayout&) = delete;
  FlowLayout& operator=(const FlowLayout&) = delete;

  void addItem(QLayoutItem* item) override;
  int count() const override;
  QLayoutItem* itemAt(int index) const override;
  QLayoutItem* takeAt(int index) override;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;
  QSize minimumSize() const override;
  QSize sizeHint() const override;
  void setGeometry(const QRect& rect) override;

 private:
  // Places the items in rect (only measures when is_placing is false)
  // and returns the height they take.
  int Place(const QRect& rect, bool is_placing) const;

  std::vector<std::unique_ptr<QLayoutItem>> items_;
  int gap_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FLOW_LAYOUT_H_
