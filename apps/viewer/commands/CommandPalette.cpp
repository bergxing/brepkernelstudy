#include "commands/CommandPalette.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace brep::viewer::commands
{

CommandPalette::CommandPalette(const CommandRegistry& registry, QWidget* parent)
    : QDialog(parent), m_registry(registry)
{
  setWindowTitle(QStringLiteral("命令面板"));
  resize(420, 360);
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(
      QStringLiteral("输入过滤并回车执行（或双击）"), this);
  m_filter = new QLineEdit(this);
  m_filter->setPlaceholderText(QStringLiteral("例如 doc.new / box / export"));
  m_list = new QListWidget(this);

  layout->addWidget(hint);
  layout->addWidget(m_filter);
  layout->addWidget(m_list, 1);

  rebuild_list({});

  connect(m_filter, &QLineEdit::textChanged, this,
          [this](const QString& t)
  {
              rebuild_list(t); 
          });
  connect(m_filter, &QLineEdit::returnPressed, this, [this] {
    if (m_list->currentItem()) accept();
  });
  connect(m_list, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*)
  {
              accept(); 
          });

  m_filter->setFocus();
}

void CommandPalette::rebuild_list(const QString& filter)
{
  m_list->clear();
  const QString f = filter.trimmed().toLower();
  for (const std::string& id : m_registry.ids())
  {
    const QString qid = QString::fromStdString(id);
    if (!f.isEmpty() && !qid.toLower().contains(f)) continue;
    m_list->addItem(qid);
  }
  if (m_list->count() > 0) m_list->setCurrentRow(0);
}

QString CommandPalette::selected_command_id() const
{
  if (auto* item = m_list->currentItem()) return item->text();
  return {};
}

}  // namespace brep::viewer::commands
