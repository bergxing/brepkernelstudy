#include "view_panel.hpp"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace brep::viewer {
namespace {

QPushButton* make_view_button(const QString& text, const QString& tip,
                              QWidget* parent) {
  auto* btn = new QPushButton(text, parent);
  btn->setToolTip(tip);
  btn->setMinimumHeight(28);
  btn->setCursor(Qt::PointingHandCursor);
  return btn;
}

}  // namespace

ViewPanel::ViewPanel(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  auto* group = new QGroupBox(QStringLiteral("标准视图"), this);
  auto* grid = new QGridLayout(group);
  grid->setSpacing(6);

  auto* btn_front =
      make_view_button(QStringLiteral("前"), QStringLiteral("前视图 (Front)"), group);
  auto* btn_back =
      make_view_button(QStringLiteral("后"), QStringLiteral("后视图 (Back)"), group);
  auto* btn_left =
      make_view_button(QStringLiteral("左"), QStringLiteral("左视图 (Left)"), group);
  auto* btn_right =
      make_view_button(QStringLiteral("右"), QStringLiteral("右视图 (Right)"), group);
  auto* btn_top =
      make_view_button(QStringLiteral("顶"), QStringLiteral("顶视图 (Top)"), group);
  auto* btn_bottom =
      make_view_button(QStringLiteral("底"), QStringLiteral("底视图 (Bottom)"), group);
  auto* btn_iso = make_view_button(QStringLiteral("轴侧"),
                                   QStringLiteral("轴测 / Home 视图"), group);

  // Layout:
  //     [顶]
  // [左][前][右]
  //     [底]
  // [后] [轴侧]
  grid->addWidget(btn_top, 0, 1);
  grid->addWidget(btn_left, 1, 0);
  grid->addWidget(btn_front, 1, 1);
  grid->addWidget(btn_right, 1, 2);
  grid->addWidget(btn_bottom, 2, 1);
  grid->addWidget(btn_back, 3, 0);
  grid->addWidget(btn_iso, 3, 1, 1, 2);

  root->addWidget(group);

  ortho_check_ = new QCheckBox(QStringLiteral("正交投影"), this);
  ortho_check_->setToolTip(QStringLiteral("切换透视 / 正交"));
  root->addWidget(ortho_check_);
  root->addStretch(1);

  connect(btn_front, &QPushButton::clicked, this, [this] { apply_view('f'); });
  connect(btn_back, &QPushButton::clicked, this, [this] { apply_view('k'); });
  connect(btn_left, &QPushButton::clicked, this, [this] { apply_view('l'); });
  connect(btn_right, &QPushButton::clicked, this, [this] { apply_view('r'); });
  connect(btn_top, &QPushButton::clicked, this, [this] { apply_view('t'); });
  connect(btn_bottom, &QPushButton::clicked, this, [this] { apply_view('b'); });
  connect(btn_iso, &QPushButton::clicked, this, [this] { apply_view('h'); });
  connect(ortho_check_, &QCheckBox::toggled, this,
          &ViewPanel::on_ortho_toggled);
}

void ViewPanel::apply_view(char face) {
  if (!camera_) return;
  camera_->set_standard_view(face);
  // Keep ortho checkbox in sync after standard views (most force framed ortho).
  if (ortho_check_) {
    const QSignalBlocker block(ortho_check_);
    ortho_check_->setChecked(camera_->ortho);
  }
  if (redraw_) redraw_();
}

void ViewPanel::on_ortho_toggled(bool checked) {
  if (!camera_) return;
  camera_->ortho = checked;
  if (redraw_) redraw_();
}

void ViewPanel::sync_from_camera() {
  if (!camera_ || !ortho_check_) return;
  const QSignalBlocker block(ortho_check_);
  ortho_check_->setChecked(camera_->ortho);
}

}  // namespace brep::viewer
