#include "HomeWindow.h"

#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace brep::viewer
{

HomeWindow::HomeWindow(QWidget* parent) : QMainWindow(parent)
{
  setWindowTitle(QStringLiteral("XCAD"));
  resize(960, 600);
  setMinimumSize(720, 480);
  build_ui();
}

void HomeWindow::build_ui()
{
  auto* central = new QWidget(this);
  setCentralWidget(central);

  // Calm CAD start-page look (deep blue, not a marketing landing collage).
  central->setStyleSheet(QStringLiteral(
      "QWidget#homeRoot {"
      "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
      "    stop:0 #071428, stop:0.55 #0c2748, stop:1 #102a4a);"
      "}"
      "QLabel#brand {"
      "  color: #e8eef7;"
      "  font-size: 42px;"
      "  font-weight: 700;"
      "  letter-spacing: 4px;"
      "}"
      "QLabel#subtitle {"
      "  color: #9eb4d0;"
      "  font-size: 15px;"
      "}"
      "QPushButton#primaryBtn {"
      "  background-color: #1f6feb;"
      "  color: white;"
      "  border: none;"
      "  border-radius: 6px;"
      "  padding: 14px 36px;"
      "  font-size: 16px;"
      "  font-weight: 600;"
      "  min-width: 200px;"
      "}"
      "QPushButton#primaryBtn:hover { background-color: #388bfd; }"
      "QPushButton#primaryBtn:pressed { background-color: #1558c0; }"
      "QPushButton#secondaryBtn {"
      "  background: transparent;"
      "  color: #c4d4e8;"
      "  border: 1px solid #3d5a80;"
      "  border-radius: 6px;"
      "  padding: 12px 28px;"
      "  font-size: 14px;"
      "  min-width: 200px;"
      "}"
      "QPushButton#secondaryBtn:hover {"
      "  border-color: #6ea0d4;"
      "  color: white;"
      "}"
      "QFrame#card {"
      "  background-color: rgba(12, 28, 52, 180);"
      "  border: 1px solid #2a4a72;"
      "  border-radius: 10px;"
      "}"));

  central->setObjectName(QStringLiteral("homeRoot"));

  auto* root = new QVBoxLayout(central);
  root->setContentsMargins(48, 40, 48, 40);
  root->setSpacing(0);

  auto* brand = new QLabel(QStringLiteral("XCAD"), central);
  brand->setObjectName(QStringLiteral("brand"));
  brand->setAlignment(Qt::AlignHCenter);

  auto* subtitle = new QLabel(
      QStringLiteral("B-Rep 内核学习版 · 选择开始方式"), central);
  subtitle->setObjectName(QStringLiteral("subtitle"));
  subtitle->setAlignment(Qt::AlignHCenter);

  auto* card = new QFrame(central);
  card->setObjectName(QStringLiteral("card"));
  card->setMaximumWidth(420);
  auto* card_layout = new QVBoxLayout(card);
  card_layout->setContentsMargins(36, 32, 36, 32);
  card_layout->setSpacing(14);

  auto* btn_new = new QPushButton(QStringLiteral("新建"), card);
  btn_new->setObjectName(QStringLiteral("primaryBtn"));
  btn_new->setCursor(Qt::PointingHandCursor);
  btn_new->setDefault(true);
  btn_new->setToolTip(QStringLiteral("创建空白文档并进入工作区"));

  auto* btn_open = new QPushButton(QStringLiteral("打开"), card);
  btn_open->setObjectName(QStringLiteral("secondaryBtn"));
  btn_open->setCursor(Qt::PointingHandCursor);
  btn_open->setToolTip(QStringLiteral("打开 .xl 文档并进入工作区"));

  auto* btn_exit = new QPushButton(QStringLiteral("退出"), card);
  btn_exit->setObjectName(QStringLiteral("secondaryBtn"));
  btn_exit->setCursor(Qt::PointingHandCursor);

  card_layout->addWidget(btn_new);
  card_layout->addWidget(btn_open);
  card_layout->addWidget(btn_exit);

  root->addStretch(2);
  root->addWidget(brand);
  root->addSpacing(8);
  root->addWidget(subtitle);
  root->addSpacing(36);

  auto* card_row = new QHBoxLayout();
  card_row->addStretch(1);
  card_row->addWidget(card);
  card_row->addStretch(1);
  root->addLayout(card_row);
  root->addStretch(3);

  connect(btn_new, &QPushButton::clicked, this,
          &HomeWindow::new_document_requested);
  connect(btn_open, &QPushButton::clicked, this, &HomeWindow::on_open_clicked);
  connect(btn_exit, &QPushButton::clicked, this, &HomeWindow::exit_requested);
}

void HomeWindow::on_open_clicked()
{
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开文档"), QDir::homePath(),
      QStringLiteral("XCAD Document (*.xl);;All Files (*)"));
  if (path.isEmpty()) return;
  emit open_document_requested(path);
}

}  // namespace brep::viewer
