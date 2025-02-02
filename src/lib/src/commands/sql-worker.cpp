#include "commands/sql-worker.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlDriver>
#include <QtSql/QSqlError>
#include <QtSql/QSqlField>
#include <QtSql/QSqlQuery>
#include <utility>
#include "logger.h"


SqlWorker::SqlWorker(QString driver, QString host, QString user, QString password, QString database, bool dryRun, QObject *parent)
	: QThread(parent), m_driver(std::move(driver)), m_host(std::move(host)), m_user(std::move(user)), m_password(std::move(password)), m_database(std::move(database)), m_dryRun(dryRun)
{
	m_enabled = (m_driver == QLatin1String("QSQLITE") && !m_database.isEmpty())
		|| (!m_host.isEmpty() && !m_user.isEmpty() && !m_database.isEmpty());

	m_started = false;
}

bool SqlWorker::connect()
{
	if (!m_enabled || m_started) {
		return true;
	}

	m_db = QSqlDatabase::addDatabase(m_driver, "SQL worker - " + m_database);
	m_db.setDatabaseName(m_database);
	m_db.setUserName(m_user);
	m_db.setPassword(m_password);

	const int portSeparator = m_host.lastIndexOf(':');
	if (portSeparator > 0) {
		m_db.setHostName(m_host.left(portSeparator));
		m_db.setPort(m_host.midRef(portSeparator + 1).toInt());
	} else {
		m_db.setHostName(m_host);
	}

	if (!m_db.open()) {
		log(QStringLiteral("Error initializing commands: %1").arg(m_db.lastError().text()), Logger::Error);
		return false;
	}

	m_started = true;
	return true;
}

QString SqlWorker::escape(const QVariant &val)
{
	QSqlDriver *driver = QSqlDatabase::database().driver();
	if (driver == nullptr) {
		return nullptr;
	}

	QSqlField f;
	f.setType(val.type());
	f.setValue(val);

	return driver->formatValue(f);
}

bool SqlWorker::execute(const QString &sql)
{
	if (!m_enabled || !connect()) {
		return false;
	}

	log(QStringLiteral("SQL execution of \"%1\"").arg(sql));
	Logger::getInstance().logCommandSql(sql);

	if (m_dryRun) {
		return true;
	}

	QSqlQuery query(m_db);
	return query.exec(sql);
}
