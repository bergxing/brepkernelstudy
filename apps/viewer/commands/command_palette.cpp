#include "commands/command_palette.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace brep::viewer::commands {

CommandPalette::CommandPalette(const CommandRegistry& registry, QWidget* parent)
    : QDialog(parent), registry_(registry) {
  setWindowTitle(QStringLiteral("命令面板"));
  resize(420, 360);
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(
      QStringLiteral("输入过滤并回车执行（或双击）"), this);
  filter_ = new QLineEdit(this);
  filter_->setPlaceholderText(QStringLiteral("例如 doc.new / box / export"));
  list_ = new QListWidget(this);

  layout->addWidget(hint);
  layout->addWidget(filter_);
  layout->addWidget(list_, 1);

  rebuild_list({});

  connect(filter_, &QLineEdit::textChanged, this,
          [this](const QString& t) { rebuild_list(t); });
  connect(filter_, &QLineEdit::returnPressed, this, [this] {
    if (list_->currentItem()) accept();
  });
  connect(list_, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*) { accept(); });

  filter_->setFocus();
}

void CommandPalette::rebuild_list(const QString& filter) {
  list_->clear();
  const QString f = filter.trimmed().toLower();
  for (const std::string& id : registry_.ids()) {
    const QString qid = QString::fromStdString(id);
    if (!f.isEmpty() && !qid.toLower().contains(f)) continue;
    list_->addItem(qid);
  }
  if (list_->count() > 0) list_->setCurrentRow(0);
}

QString CommandPalette::selected_command_id() const {
  if (auto* item = list_->currentItem()) return item->text();
  return {};
}

}  // namespace brep::viewer::commands
