#include "calendarnotebookquery.h"
#include "calendarmanager.h"

CalendarNotebookQuery::CalendarNotebookQuery(QObject *parent) :
    QObject(parent),
    m_isValid(false)
{
    connect(CalendarManager::instance(), SIGNAL(notebooksChanged(QList<CalendarData::Notebook>)),
            this, SLOT(updateData()));
}

CalendarNotebookQuery::~CalendarNotebookQuery()
{
}

QString CalendarNotebookQuery::targetUid() const
{
    return m_targetUid;
}

void CalendarNotebookQuery::setTargetUid(const QString &target)
{
    if (target != m_targetUid) {
        m_targetUid = target;
        updateData();
        emit targetUidChanged();
    }
}

bool CalendarNotebookQuery::isValid() const
{
    return m_isValid;
}

QString CalendarNotebookQuery::name() const
{
    return m_notebook.name;
}

QString CalendarNotebookQuery::description() const
{
    return m_notebook.description;
}

QString CalendarNotebookQuery::color() const
{
    return m_notebook.color;
}

int CalendarNotebookQuery::accountId() const
{
    return m_notebook.accountId;
}

QUrl CalendarNotebookQuery::accountIcon() const
{
    return m_notebook.accountIcon;
}

bool CalendarNotebookQuery::isDefault() const
{
    return m_notebook.isDefault;
}

bool CalendarNotebookQuery::localCalendar() const
{
    return m_notebook.localCalendar;
}

bool CalendarNotebookQuery::isReadOnly() const
{
    return m_notebook.readOnly;
}

void CalendarNotebookQuery::updateData()
{
    // fetch new info and signal changes
    QList<CalendarData::Notebook> notebooks = CalendarManager::instance()->notebooks();

    CalendarData::Notebook notebook;
    bool found = false;

    for (int i = 0; i < notebooks.length(); ++i) {
        CalendarData::Notebook current = notebooks.at(i);
        if (current.uid == m_targetUid) {
            notebook = current;
            found = true;
            break;
        }
    }

    bool nameUpdated = (notebook.name != m_notebook.name);
    bool descriptionUpdated = (notebook.description != m_notebook.description);
    bool colorUpdated = (notebook.color != m_notebook.color);
    bool accountIdUpdated = (notebook.accountId != m_notebook.accountId);
    bool accountIconUpdated = (notebook.accountIcon != m_notebook.accountIcon);
    bool isDefaultUpdated = (notebook.isDefault != m_notebook.isDefault);
    bool localCalendarUpdated = (notebook.localCalendar != m_notebook.localCalendar);
    bool isReadOnlyUpdated = (notebook.readOnly != m_notebook.readOnly);

    m_notebook = notebook;

    if (nameUpdated)
        emit nameChanged();
    if (descriptionUpdated)
        emit descriptionChanged();
    if (colorUpdated)
        emit colorChanged();
    if (accountIdUpdated)
        emit accountIdChanged();
    if (accountIconUpdated)
        emit accountIconChanged();
    if (isDefaultUpdated)
        emit isDefaultChanged();
    if (localCalendarUpdated)
        emit localCalendarChanged();
    if (isReadOnlyUpdated)
        emit isReadOnlyChanged();

    if (m_isValid != found) {
        m_isValid = found;
        emit isValidChanged();
    }
}
